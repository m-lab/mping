#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#elif defined(OS_WINDOWS)
#include <winsock2.h>
#endif
#include <errno.h>
#include <string.h>

#include "mp_log.h"
#include "mp_common.h"
#include "mp_socket.h"
#include "mlab/mlab.h"
#include "mlab/protocol_header.h"
#include "scoped_ptr.h"

namespace {

const int kMPDefaultTTL = 128;

int AddressFamilyFor(SocketFamily family) {
  switch (family) {
    case SOCKETFAMILY_UNSPEC: return AF_UNSPEC;
    case SOCKETFAMILY_IPV4: return AF_INET;
    case SOCKETFAMILY_IPV6: return AF_INET6;
  }
  return AF_UNSPEC;
}

}  // namespace

int MpingSocket::Initialize(const std::string& destip, const std::string& srcip,
                            int ttl, size_t pktsize, int wndsize, 
                            uint16_t port, bool clientmode) {
  family_ = mlab::GetSocketFamilyForAddress(destip);

  ASSERT(family_ != SOCKETFAMILY_UNSPEC);
  size_t min_size = 0;

  // check packet size
  switch (family_) {
    case SOCKETFAMILY_IPV4: {
      min_size = sizeof(mlab::IP4Header) + kPayloadHeaderLength +
                 sizeof(unsigned int) + sizeof(mlab::ICMP4Header);
      if (pktsize < min_size)
        LOG(FATAL, "Packet size should be no less than %zu for IPv4.",
            min_size);

      break;
    }
    case SOCKETFAMILY_IPV6: {
      min_size = sizeof(mlab::IP6Header) + kPayloadHeaderLength +
                 sizeof(unsigned int) + sizeof(mlab::ICMP6Header);
      if (pktsize < min_size)
        LOG(FATAL, "Packet size should be no less than %zu for IPv6.",
            min_size);

      break;
    }
    case SOCKETFAMILY_UNSPEC:
      LOG(FATAL, "unknow destination address family.");
  }

  fromaddr_ = destip;

  // create sockets and initialize buffer
  // ICMP v4 and v6 buffer: ICMP header + payload
  // UDP v4 and v6 buffer: payload
  if (ttl == 0) {  // create one icmp socket for send/recv
    use_udp_ = false;
    icmp_sock = 
        mlab::RawSocket::Create(SOCKETTYPE_ICMP, family_);

    if (icmp_sock == NULL) {
      return -1;
    }

    // bind if specified src ip
    if (srcip.length() != 0) {
      if (icmp_sock->Bind(mlab::Host(srcip)) < 0) {
        return -1;
      }
    }

    // connect to only receive packets from destip.
    // set it because we only need ECHO REQ/REP for icmp only send/recv
    if (icmp_sock->Connect(mlab::Host(destip)) < 0) {
      return -1;
    }

    // initialize packet
    switch (family_) {
      case SOCKETFAMILY_IPV4: {
        scoped_ptr<mlab::ICMP4Header> icmphdr(new mlab::ICMP4Header(ICMP_ECHO, 
                                                                    0, 0, 0));
        // TODO: add get buffer method to protocol headers
        memcpy(buffer_, icmphdr.get(), sizeof(mlab::ICMP4Header));
        memcpy(buffer_ + sizeof(mlab::ICMP4Header), kPayloadHeader,
               kPayloadHeaderLength);
        buffer_length_ = sizeof(mlab::ICMP4Header) + kPayloadHeaderLength;
        break;
      }
      case SOCKETFAMILY_IPV6: {
        scoped_ptr<mlab::ICMP6Header> icmp6hdr(new mlab::ICMP6Header(128, 0,
                                                                     0, 0));
        memcpy(buffer_, icmp6hdr.get(), sizeof(mlab::ICMP6Header));
        memcpy(buffer_ + sizeof(mlab::ICMP6Header), kPayloadHeader, 
               kPayloadHeaderLength);
        buffer_length_ = sizeof(mlab::ICMP6Header) + kPayloadHeaderLength;
        break;
      }
      case SOCKETFAMILY_UNSPEC: {
        LOG(ERROR, "Unknown socket family.");
        return -1;
      }
    }

    // validate buffer size
    if (icmp_sock->GetRecvBufferSize() < (pktsize * wndsize)) {
      LOG(WARNING, "Change recv buffer size.");
      icmp_sock->SetRecvBufferSize(pktsize * wndsize);
    }
      
    if (icmp_sock->GetSendBufferSize() < pktsize) {
      LOG(WARNING, "Change send buffer size.");
      icmp_sock->SetSendBufferSize(pktsize);
    }
  } else {  // UDP socket to send
    use_udp_ = true;
    uint16_t dport;

    if (port > 0) { 
      dport = port;
    } else {
      dport = 32768 + (rand() % 32768);  // random port > 32768
    }

    if (srcip.length()!= 0) {
      udp_sock = mlab::ClientSocket::Create(mlab::Host(srcip), 0,
                                            mlab::Host(destip), dport,
                                            SOCKETTYPE_UDP, family_);
    } else {
      udp_sock = mlab::ClientSocket::Create(mlab::Host(destip), dport,
                                            SOCKETTYPE_UDP, family_);
    }

    if (udp_sock == NULL) {
      return -1;
    }

    if (!clientmode) {
      // icmp to recv
      icmp_sock = 
          mlab::RawSocket::Create(SOCKETTYPE_ICMP, family_);

      if (icmp_sock == NULL) {
        return -1;
      }

      // icmp_sock->Connect(mlab::Host(destip));
      
      // validate socket buffer size
      if (icmp_sock->GetRecvBufferSize() < (pktsize * wndsize)) {
        LOG(INFO, "Set recv buffer to %zu.", pktsize * wndsize);
        icmp_sock->SetRecvBufferSize(pktsize * wndsize);
      }
        
      if (udp_sock->GetSendBufferSize() < pktsize) {
        LOG(INFO, "Set send buffer to %zu.", pktsize * wndsize);
        udp_sock->SetSendBufferSize(pktsize);
      }
    } else {
      client_mode_ = true;
    }

    // build packet
    memcpy(buffer_, kPayloadHeader, kPayloadHeaderLength);
    buffer_length_ = kPayloadHeaderLength;
  }

  return 0;
}

bool MpingSocket::SetSendTTL(const int& ttl) {
  if (!use_udp_) {
    LOG(ERROR, "Not using UDP, no need to set TTL.");
    return false;
  } 

  ASSERT(udp_sock != NULL);

  switch (family_) {
    case SOCKETFAMILY_IPV4: {
      if (setsockopt(udp_sock->raw(), IPPROTO_IP, IP_TTL, &ttl, 
                     sizeof(ttl)) < 0) {
        LOG(ERROR, "Set TTL fails. %s [%d]", strerror(errno), errno);
        return false;
      }
      return true;
    }
    case SOCKETFAMILY_IPV6: {
       if (setsockopt(udp_sock->raw(), IPPROTO_IPV6, IPV6_UNICAST_HOPS, &ttl, 
                      sizeof(ttl)) < 0) {
         LOG(ERROR, "Set TTL fails. %s [%d]", strerror(errno), errno);
         return false;
       }
       return true;
    }
    case SOCKETFAMILY_UNSPEC: {
      LOG(FATAL, "unknown socket family.");
      return false;
    }
  }

  LOG(FATAL, "SetSendTTL unknown error.");
  return false;
}

MpingSocket::~MpingSocket() {
  delete icmp_sock;
  icmp_sock = NULL;

  delete udp_sock;
  udp_sock = NULL;
}

size_t MpingSocket::SendPacket(const uint32_t& seq, size_t size, 
                               int *error) const {
  char buf[size];
  size_t send_size = 0;
  unsigned int* seq_ptr;

  ASSERT(family_ != SOCKETFAMILY_UNSPEC);

  switch (family_) {
    case SOCKETFAMILY_IPV4: {
      if (size < sizeof(mlab::IP4Header) + buffer_length_ + sizeof(seq)) {
        LOG(FATAL, "send packet size is smaller than MIN.");
      }
      send_size = size - sizeof(mlab::IP4Header); 
      if (use_udp_) {
        send_size -= sizeof(mlab::UDPHeader);
      }
      break;
    }
    case SOCKETFAMILY_IPV6: {
      if (size < sizeof(mlab::IP6Header) + buffer_length_ + sizeof(seq)) {
        LOG(FATAL, "send packet size is smaller than MIN.");
      }
      send_size = size - sizeof(mlab::IP6Header);
      if (use_udp_) {
        send_size -= sizeof(mlab::UDPHeader);
      }
      break;
    }
    case SOCKETFAMILY_UNSPEC: return 0;
  }

  memcpy(buf, buffer_, buffer_length_);
  seq_ptr = reinterpret_cast<unsigned int*>(buf + buffer_length_);
  *seq_ptr = htonl(seq);
  
  ssize_t num_bytes = 0;
  // TODO(xunfan): use protocol enum so that adding TCP is trivial
  if (!use_udp_) {  // ICMP
    ASSERT(icmp_sock != NULL);
    if (family_ == SOCKETFAMILY_IPV4) {
      // set checksum
      mlab::ICMP4Header *p = reinterpret_cast<mlab::ICMP4Header *>(buf);
      p->icmp_checksum = mlab::InternetCheckSum(buf, send_size);
    }
    if (!icmp_sock->Send(mlab::Packet(buf, send_size), &num_bytes)) {
      *error = errno;
    }
    return static_cast<size_t>(num_bytes);
  } else {  // UDP
    ASSERT(udp_sock != NULL);
    if (!udp_sock->Send(mlab::Packet(buf, send_size), &num_bytes)) {
      *error = errno;
    }
    return static_cast<size_t>(num_bytes);
  }
}

uint32_t MpingSocket::ReceiveAndGetSeq(int* error, 
                                           MpingStat *mpstat) {
  if (client_mode_) {
    ASSERT(udp_sock != NULL);
  } else {
    ASSERT(icmp_sock != NULL);
  }
  ASSERT(mpstat != NULL);
  ASSERT(family_ != SOCKETFAMILY_UNSPEC);

//  size_t recv_size = 0;
  size_t min_recv_size = 0;
  size_t should_recv_size = 0;
  size_t icmp_offset = 0;
  size_t payload_offset = sizeof(mlab::ICMP4Header);

  LOG(VERBOSE, "start receive loop.");

  switch (family_) {
    case SOCKETFAMILY_IPV4: {
      min_recv_size = sizeof(mlab::IP4Header) + sizeof(mlab::ICMP4Header);
      icmp_offset = sizeof(mlab::IP4Header);

      if (use_udp_) {
        // recv_size = size + sizeof(mlab::IP4Header) + 
         //           sizeof(mlab::ICMP4Header);
        should_recv_size = 2 * sizeof(mlab::IP4Header) + 
                           sizeof(mlab::ICMP4Header) +
                           sizeof(mlab::UDPHeader) +
                           buffer_length_ + sizeof(uint32_t);
        payload_offset = payload_offset +
                         sizeof(mlab::IP4Header) + sizeof(mlab::UDPHeader);
      } else {
        // recv_size = size;
        should_recv_size = sizeof(mlab::IP4Header) + buffer_length_ + 
                      sizeof(uint32_t);
      }

      break;
    }
    case SOCKETFAMILY_IPV6: {
      if (use_udp_) {
        // recv_size = size + sizeof(mlab::ICMP6Header);
        should_recv_size = sizeof(mlab::ICMP6Header) + 
                           sizeof(mlab::IP6Header) +
                           sizeof(mlab::UDPHeader) +
                           buffer_length_ + sizeof(uint32_t);
        payload_offset = payload_offset + 
                         sizeof(mlab::IP6Header) + sizeof(mlab::UDPHeader);
      } else {
        // recv_size = size - sizeof(mlab::IP6Header);
        should_recv_size = buffer_length_ + sizeof(uint32_t);
      }

      min_recv_size = sizeof(mlab::ICMP6Header);
      break;
    }
    case SOCKETFAMILY_UNSPEC:
      return 0;
  }

  if (client_mode_) {
    should_recv_size = buffer_length_ + sizeof(uint32_t);
    payload_offset = 0;
  }

  const char *ptr;
  ssize_t num_bytes = 0;
  mlab::Host recvfromaddr("127.0.0.1");
  while (1) {
    mlab::Packet recv_packet(""); 
    if (client_mode_) {
      recv_packet = udp_sock->Receive(should_recv_size, &num_bytes);
    } else {
      if (use_udp_)
        recv_packet = icmp_sock->ReceiveFrom(should_recv_size, &recvfromaddr,
                                             &num_bytes);
      else
        recv_packet = icmp_sock->Receive(should_recv_size, &num_bytes);
    }

    if (num_bytes < 0) {
      *error = errno;
//      LOG(ERROR, "ReceiveAndGetSeq error: %d", errno);
      return 0;
    }

    ptr = recv_packet.buffer();

    if (!client_mode_) {
      // check length: ICMP?
      if (recv_packet.length() < min_recv_size) {
        LOG(VERBOSE, "recv a packet smaller than regular %s.",
            family_==SOCKETFAMILY_IPV4?"ICMP":"ICMPv6");
        mpstat->LogUnexpected();
        continue;
      }
      ptr += icmp_offset;  // now ptr is at icmp header
      const mlab::ICMP4Header *icmp_ptr =
          reinterpret_cast<const mlab::ICMP4Header *>(ptr);

      // check length: include payload?
      if (recv_packet.length() < should_recv_size) {
        LOG(VERBOSE,
            "recv icmp packet size is smaller than expected. "
            "ICMP type %u code %u.", icmp_ptr->icmp_type, 
            icmp_ptr->icmp_code);
        mpstat->LogUnexpected();
        continue;
      }

      // check ICMP message type: echo reply? or dst_unreach time_exceeded?
      if (use_udp_) {
        if (icmp_ptr->icmp_type != 
                (family_==SOCKETFAMILY_IPV4?ICMP_DEST_UNREACH:1) &&
            icmp_ptr->icmp_type != 
                (family_==SOCKETFAMILY_IPV4?ICMP_TIME_EXCEEDED:3)) {
          LOG(VERBOSE,
              "recv an ICMP message with wrong type, type %u code %u",
              icmp_ptr->icmp_type, icmp_ptr->icmp_code);
          mpstat->LogUnexpected();
          continue;
        }

        // UDP, check IP protocol type: UDP?
        uint8_t proto;
        if (family_==SOCKETFAMILY_IPV4) {
          const mlab::IP4Header *ip_ptr = 
              reinterpret_cast<const mlab::IP4Header *>(ptr + 
                                                     sizeof(mlab::ICMP4Header));
          proto = ip_ptr->protocol;
        } else {
          const mlab::IP6Header *ip_ptr = 
              reinterpret_cast<const mlab::IP6Header *>(ptr + 
                                                     sizeof(mlab::ICMP4Header));
          proto = ip_ptr->next_header;
        }

        if (proto != IPPROTO_UDP) {
          LOG(VERBOSE, "not an ICMP for UDP, for protocol %u.", proto);
          mpstat->LogUnexpected();
          continue;
        }
      } else {
        if (icmp_ptr->icmp_type != 
                (family_==SOCKETFAMILY_IPV4?ICMP_ECHOREPLY:129)) {
          LOG(VERBOSE,
              "recv a non-echoreply packet. ICMP type %u, code %u.",
              icmp_ptr->icmp_type, icmp_ptr->icmp_code);
          mpstat->LogUnexpected();
          continue;
        }
      }
    }

    // check payload
    ptr += payload_offset;  // now ptr is at the beginning of the payload
    std::string head(ptr, kPayloadHeaderLength);
    if (head.compare(kPayloadHeader) != 0) {
      LOG(VERBOSE, "recv an packet not for this program.");
      mpstat->LogUnexpected();
      continue;
    }

    if (use_udp_)
      fromaddr_ = recvfromaddr.original_hostname;

    ptr += kPayloadHeaderLength;
    const uint32_t *seq = reinterpret_cast<const uint32_t*>(ptr);
    *error = 0;
    return ntohl(*seq);
  }
}

const std::string MpingSocket::GetFromAddress() const {
  return fromaddr_;
}
