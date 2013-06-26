#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#elif defined(OS_WINDOWS)
#include <winsock2.h>
#endif
#include <errno.h>
#include <string.h>

#include "log.h"
#include "mp_socket.h"
#include "mlab/mlab.h"
#include "mlab/protocol_header.h"
#include "scoped_ptr.h"

namespace {

const int kMPDefaultTTL = 128;

const char *kPayloadHeader = "mlab-seq#";
const int kPayloadHeaderLength = 9;

int AddressFamilyFor(SocketFamily family) {
  switch (family) {
    case SOCKETFAMILY_UNSPEC: return AF_UNSPEC;
    case SOCKETFAMILY_IPV4: return AF_INET;
    case SOCKETFAMILY_IPV6: return AF_INET6;
  }
  return AF_UNSPEC;
}

}  // namespace

MpingSocket::MpingSocket(const std::string& destip, const std::string& srcip,
                         int ttl, size_t pktsize, int wndsize, uint16_t port) {
  family_ = mlab::RawSocket::GetAddrFamily(destip);

  ASSERT(family_ != SOCKETFAMILY_UNSPEC);

  // handle sourceip
  if (srcip.length() != 0) {
    srcaddr_.ss_family = 
        AddressFamilyFor(mlab::RawSocket::GetAddrFamily(srcip));

    ASSERT(mlab::RawSocket::GetAddrFamily(srcip) == family_);

    switch (srcaddr_.ss_family) {
      case AF_INET: {
        sockaddr_in *in = reinterpret_cast<sockaddr_in *>(&srcaddr_);
        inet_pton(srcaddr_.ss_family, srcip.c_str(), &(in->sin_addr));
        break;
      }
      case AF_INET6: {
        sockaddr_in6 *in6 = reinterpret_cast<sockaddr_in6 *>(&srcaddr_);
        inet_pton(srcaddr_.ss_family, srcip.c_str(), &(in6->sin6_addr));
        break;
      }
    }
  }

  fromaddr_ = destip;

  // create sockets and initialize buffer
  // ICMP v4 and v6 buffer: ICMP header + payload
  // UDP v4 and v6 buffer: payload
  if (ttl == 0) {  // create one icmp socket for send/recv
    use_udp_ = false;
    icmp_sock = 
        mlab::RawSocket::CreateOrDie(SOCKETTYPE_ICMP, family_);


    // bind if specified src ip
    if (srcip.length() != 0) {
      icmp_sock->Bind(mlab::Host(srcip));
    }

    // connect to only receive packets from destip.
    // set it because we only need ECHO REQ/REP for icmp only send/recv
    icmp_sock->Connect(mlab::Host(destip));

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
      case SOCKETFAMILY_UNSPEC: 
        LOG(mlab::FATAL, "Unknown socket family.");
    }

    // validate buffer size
    if (icmp_sock->GetRecvBufferSize() < (pktsize * wndsize)) {
      LOG(mlab::WARNING, "Change recv buffer size.");
      icmp_sock->SetRecvBufferSize(pktsize * wndsize);
    }
      
    if (icmp_sock->GetSendBufferSize() < pktsize) {
      LOG(mlab::WARNING, "Change send buffer size.");
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

    udp_sock = mlab::ClientSocket::CreateOrDie(mlab::Host(destip), dport,
                                               SOCKETTYPE_UDP, family_);

    // bind UDP socket to a src ip with port 0 (kernel will choose random port)
    if (srcip.length() != 0) {
      sockaddr_storage sa, da;
      socklen_t size;
      switch (srcaddr_.ss_family) {
        case AF_INET: {
          sockaddr_in *in = reinterpret_cast<sockaddr_in*>(&da);
          in->sin_family = AF_INET;
          in->sin_port = htons(dport);
          inet_pton(in->sin_family, destip.c_str(), &(in->sin_addr));
          size = sizeof(sockaddr_in);
          break;
        }
        case AF_INET6: {
          sockaddr_in6 *in6 = reinterpret_cast<sockaddr_in6*>(&da);
          in6->sin6_family = AF_INET6;
          in6->sin6_port = htons(dport);
          inet_pton(in6->sin6_family, destip.c_str(), &(in6->sin6_addr));
          size = sizeof(sockaddr_in6);
          break;
        }
      }

      // un connect the socket to unbind it, otherwise the bind will fail
      sa.ss_family = AF_UNSPEC;
      if (connect(udp_sock->raw(), reinterpret_cast<const sockaddr*>(&sa),
                  size) < 0) {
        LOG(mlab::FATAL, "un-connect UDP socket fails. %s [%d]", 
            strerror(errno), errno);
      }

      // bind to send from specified address, 
      // source address is stored in srcaddr_
      if (bind(udp_sock->raw(), 
               reinterpret_cast<const sockaddr*>(&srcaddr_), size) < 0) {
        LOG(mlab::FATAL, 
            "Bind UDP socket to source address %s fails. %s [%d]", 
            srcip.c_str(), strerror(errno), errno);
      }

      // re-connect to destip
      if (connect(udp_sock->raw(), reinterpret_cast<const sockaddr*>(&da),
            size) < 0) {
        LOG(mlab::FATAL, "re-connect UDP socket fails. %s [%d]", 
            strerror(errno), errno);
      }
      
    }

    // icmp to recv
    icmp_sock = 
        mlab::RawSocket::CreateOrDie(SOCKETTYPE_ICMP, family_);

    // icmp_sock->Connect(mlab::Host(destip));

    // build packet
    memcpy(buffer_, kPayloadHeader, kPayloadHeaderLength);
    buffer_length_ = kPayloadHeaderLength;

    // validate socket buffer size
    if (icmp_sock->GetRecvBufferSize() < (pktsize * wndsize)) {
      icmp_sock->SetRecvBufferSize(pktsize * wndsize);
    }
      
    if (udp_sock->GetSendBufferSize() < pktsize) {
      udp_sock->SetSendBufferSize(pktsize);
    }
  }
}

bool MpingSocket::SetSendTTL(const int& ttl) {
  if (!use_udp_) {
    LOG(mlab::ERROR, "Not using UDP, no need to set TTL.");
    return false;
  } 

  ASSERT(udp_sock != NULL);
  return udp_sock->SetTTL(ttl);
  
}

MpingSocket::~MpingSocket() {
  delete icmp_sock;
  icmp_sock = NULL;

  delete udp_sock;
  udp_sock = NULL;
}

size_t MpingSocket::SendPacket(const unsigned int& seq, size_t size, int *error) const {
  char buf[size];
  size_t send_size = 0;
  unsigned int* seq_ptr;

  ASSERT(family_ != SOCKETFAMILY_UNSPEC);

  switch (family_) {
    case SOCKETFAMILY_IPV4: {
      if (size < sizeof(mlab::IP4Header) + buffer_length_ + sizeof(seq)) {
        LOG(mlab::FATAL, "send packet size is smaller than MIN.");
      }
      send_size = size - sizeof(mlab::IP4Header); 
      if (use_udp_) {
        send_size -= sizeof(mlab::UDPHeader);
      }
      break;
    }
    case SOCKETFAMILY_IPV6: {
      if (size < sizeof(mlab::IP6Header) + buffer_length_ + sizeof(seq)) {
        LOG(mlab::FATAL, "send packet size is smaller than MIN.");
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
  
  // TODO: use protocol enum so that adding TCP is trivial
  if (!use_udp_) {  // ICMP
    ASSERT(icmp_sock != NULL);
    if (family_ == SOCKETFAMILY_IPV4) {
      // set checksum
      mlab::ICMP4Header *p = reinterpret_cast<mlab::ICMP4Header *>(buf);
      p->icmp_checksum = mlab::CheckSum(buf, send_size);
    }
    return icmp_sock->SendWithError(mlab::Packet(buf, send_size), error);
  } else {  // UDP
    ASSERT(udp_sock != NULL);
    return udp_sock->SendWithError(mlab::Packet(buf, send_size), error);
  }
}

unsigned int MpingSocket::ReceiveAndGetSeq(size_t size, int* error) {
  ASSERT(icmp_sock != NULL);
  ASSERT(family_ != SOCKETFAMILY_UNSPEC);

  size_t recv_size = 0;
  int err = 0;

  if (!use_udp_) {
    switch (family_) {
      case SOCKETFAMILY_IPV4: {
        // parse IP header, ICMP header
        recv_size = size;
        LOG(mlab::VERBOSE, "start receive loop.")
        while (1) {
          mlab::Packet recv_packet = icmp_sock->ReceiveWithError(recv_size,
                                                                 &err);

          if (err != 0) {
            *error = err;
            return 0;
          }

          if (recv_packet.length() >= sizeof(mlab::IP4Header) + buffer_length_
                                      + sizeof(unsigned int)) {
            const mlab::ICMP4Header *icmp_ptr;
            const char *ptr = recv_packet.buffer();
            ptr += sizeof(mlab::IP4Header);

            icmp_ptr = reinterpret_cast<const mlab::ICMP4Header *>(ptr);

            if (icmp_ptr->icmp_type == ICMP_ECHOREPLY) {
              ptr += sizeof(mlab::ICMP4Header);

              std::string head(ptr, kPayloadHeaderLength);  // check payload
              if (head.compare(kPayloadHeader) == 0) {
                const unsigned int *seq = 
                    reinterpret_cast<const unsigned int*>(
                        ptr+ kPayloadHeaderLength);
                *error = err;
                return ntohl(*seq);
              } else {
                LOG(mlab::VERBOSE, "recv an echoreply not belonging to us");
              }
            } else {
              LOG(mlab::VERBOSE, "recv a non-echoreply packet.");
            }
          } else {
            LOG(mlab::VERBOSE, "recv packet is too small.");
          }
        }
        break;
      }
      case SOCKETFAMILY_IPV6: {
        // parse ICMP6 header
        recv_size = size - sizeof(mlab::IP6Header);

        while (1) {
          mlab::Packet recv_packet = icmp_sock->ReceiveWithError(recv_size,
                                                                 &err);

          if (err != 0) {
            *error = err;
            return 0;
          }

          if (recv_packet.length() >= buffer_length_ + sizeof(unsigned int)) {
            const mlab::ICMP6Header *icmp_ptr;
            const char *ptr = recv_packet.buffer();

            icmp_ptr = reinterpret_cast<const mlab::ICMP6Header *>(ptr);
            if (icmp_ptr->icmp6_type == 129) {  // ICMP6_ECHO_REPLY
              ptr += sizeof(mlab::ICMP6Header);
              std::string head(ptr, kPayloadHeaderLength);
              if (head.compare(kPayloadHeader) == 0) {
                const unsigned int *seq =
                    reinterpret_cast<const unsigned int*>(ptr+
                                                         kPayloadHeaderLength);
                *error = err;
                return ntohl(*seq);
              } else {
                LOG(mlab::VERBOSE, "recv an echoreply not belonging to us");
              }
            } else {
              LOG(mlab::VERBOSE, "recv a non-echoreply packet.");
            }
          } else {
            LOG(mlab::VERBOSE, "recv packet is too small.");
          }
        }
        break;
      }
      case SOCKETFAMILY_UNSPEC: return 0;  // only for eliminate compile error
    }
  } else {  // UDP, need to reserve incoming packet's src address
    switch (family_) {
      case SOCKETFAMILY_IPV4: {
        // Parse IP header ICMP header IP header UDP header...
        recv_size = size + sizeof(mlab::IP4Header) + sizeof(mlab::ICMP4Header);
        mlab::Host fromip("127.0.0.1");
        while (1) {
          mlab::Packet recv_packet = icmp_sock->ReceiveFromWithError(recv_size, 
                                                                     &fromip,
                                                                     &err);

          if (err != 0) {
            *error = err;
            return 0;
          }
          
          if (recv_packet.length() >= 2 * sizeof(mlab::IP4Header) +
                                      sizeof(mlab::ICMP4Header) +
                                      sizeof(mlab::UDPHeader) + 
                                      buffer_length_) {
            const mlab::ICMP4Header *icmp_ptr;
            const char *ptr = recv_packet.buffer();

            icmp_ptr = 
                reinterpret_cast<const mlab::ICMP4Header *>(ptr+
                                                            sizeof(
                                                            mlab::IP4Header));
            if (icmp_ptr->icmp_type == ICMP_DEST_UNREACH || 
                icmp_ptr->icmp_type == ICMP_TIME_EXCEEDED) {
              const mlab::IP4Header *ip_ptr;
              ptr += (sizeof(mlab::ICMP4Header) + sizeof(mlab::IP4Header));
              ip_ptr = reinterpret_cast<const mlab::IP4Header *>(ptr);
              if (ip_ptr->protocol == IPPROTO_UDP) {
                ptr += (sizeof(mlab::IP4Header) + sizeof(mlab::UDPHeader));
                std::string head(ptr, kPayloadHeaderLength);
                if (head.compare(kPayloadHeader) == 0) {
                  fromaddr_ = fromip.original_hostname;
                  const unsigned int *seq =
                      reinterpret_cast<const unsigned int*>(ptr+
                                                         kPayloadHeaderLength);
                  *error = err;
                  return ntohl(*seq);
                } else {
                  LOG(mlab::VERBOSE, "recv an ICMP bot belonging to us");
                }
              } else {
                LOG(mlab::VERBOSE, "not an ICMP for UDP");
              }
            } else {
              LOG(mlab::VERBOSE, "recv an ICMP message with wrong type");
            }
          } else {
            LOG(mlab::VERBOSE, "recv packet is too small.");
          }
        }
        break;
      }
      case SOCKETFAMILY_IPV6: {
        recv_size = size + sizeof(mlab::ICMP6Header); 
        mlab::Host fromip("127.0.0.1");
        while (1) {
          mlab::Packet recv_packet = icmp_sock->ReceiveFromWithError(recv_size,
                                                                     &fromip,
                                                                     &err);
          if (err != 0) {
            *error = err;
            return 0;
          }

          if (recv_packet.length() >= sizeof(mlab::ICMP6Header) +
                                      sizeof(mlab::IP6Header) +
                                      sizeof(mlab::UDPHeader) +
                                      buffer_length_) {
            const mlab::ICMP6Header *icmp_ptr;
            const char *ptr = recv_packet.buffer();

            icmp_ptr = reinterpret_cast<const mlab::ICMP6Header *>(ptr);
            if (icmp_ptr->icmp6_type == 1 ||
                icmp_ptr->icmp6_type == 3) {
              const mlab::IP6Header *ip_ptr;
              ptr += sizeof(mlab::ICMP6Header);
              ip_ptr = reinterpret_cast<const mlab::IP6Header *>(ptr);
              if (ip_ptr->next_header == IPPROTO_UDP) {
                ptr += (sizeof(mlab::IP6Header) + sizeof(mlab::UDPHeader));
                std::string head(ptr, kPayloadHeaderLength);
                if (head.compare(kPayloadHeader) == 0) {
                  fromaddr_ = fromip.original_hostname;
                  const unsigned int *seq =
                      reinterpret_cast<const unsigned int*>(ptr+
                                                         kPayloadHeaderLength);
                  *error = err;
                  return ntohl(*seq);
                } else {
                  LOG(mlab::VERBOSE, "recv an ICMP bot belonging to us");
                }
              } else {
                LOG(mlab::VERBOSE, "not an ICMP for UDP");
              }
            } else {
              LOG(mlab::VERBOSE, "recv an ICMP message with wrong type");
            }
          } else {
            LOG(mlab::VERBOSE, "recv packet is too small.");
          }
        }
        break;
      }
      case SOCKETFAMILY_UNSPEC: return 0;
    }
  }
  return 0;
}

const std::string MpingSocket::GetFromAddress() const {
  return fromaddr_;
}
