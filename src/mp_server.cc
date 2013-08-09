#if defined(OS_LINUX) || defined(OS_MACOSX)
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#elif defined(OS_WINDOWS)
#include <winsock2.h>
#endif
#include <errno.h>

#include <iostream>

#include "mp_log.h"
#include "mp_common.h"
#include "mp_server.h"
#include "mlab/mlab.h"
#include "mlab/server_socket.h"
#include "scoped_ptr.h"

namespace {
  char usage[] =
"Usage:  mping_server [-6] [-b <packet_size>] <port>\n\
      -6               Listen on IPv6 address\n\
      -b <packet_size> Set socket buffer size. Use this if you\n\
                       think your packet will be very large (> 9000 bytes). \n\
      <port>           Listen port\n";
}  // namespace

MPingServer::MPingServer(const int& argc, const char **argv) 
    : have_data(false),
      unexpected(0),
      seq_recv(0),
      sent_back(0),
      total_recv(0),
      out_of_order(0),
      mrseq(0),
      packet_size(kMinBuffer),
      server_family(SOCKETFAMILY_IPV4) {
  int ac = argc;
  const char **av = argv;
  const char *p;
  bool port_set = false;
  
  if (argc < 2) {
    printf("%s", usage);
    exit(0);
  }

  ac--;
  av++;

  while (ac > 0) {
    p = *av++;

    if (p[0] == '-') {
      if (ac == 1) {
        switch (p[1]) {  // those switches without value
          case '6': server_family = SOCKETFAMILY_IPV6; av--; break;
          default: LOG(FATAL, "\n%s", usage); break;
        }
      } else {
        switch (p[1]) {
          case 'b': { packet_size = atoi(*av); ac--; break; }
          case '6': { server_family = SOCKETFAMILY_IPV6; av--; break; }
          default: { 
            LOG(FATAL, "Unknown parameter -%c\n%s", p[1], usage); break; 
          }
        }
      }
    } else {  // port
      if (!port_set) {
        server_port = atoi(p);  
        av--;
        port_set = true;
      }else{
        LOG(FATAL, "try to set port %s, but port is already set to %d\n%s", 
            p, server_port, usage);
      }
    }

    ac--;
    av++;
  }

  packet_size = std::max(packet_size, kMinBuffer);
}

void MPingServer::Run() {
  MPLOG(MPLOG_DEF, "Running server mode, port %u.", server_port);

  // create server socket
  scoped_ptr<mlab::ServerSocket>
      mysock(mlab::ServerSocket::CreateOrDie(server_port, SOCKETTYPE_UDP,
                                             server_family));

  // check socket buffer size
  if (mysock->GetRecvBufferSize() < packet_size) {
    LOG(VERBOSE, "Receive buffer size is smaller than needed, "
                       "set to needed.");
    mysock->SetRecvBufferSize(packet_size);
  }

  if (mysock->GetSendBufferSize() < packet_size) {
    LOG(VERBOSE, 
        "Send buffer size is smaller than needed, set to needed.");
    mysock->SetSendBufferSize(packet_size);
  }

  // default time out is 5 sec, use it
  while (1) {
    errno = 0;
    ssize_t num_bytes = 0;
    mlab::Packet recv_packet = mysock->Receive(packet_size, &num_bytes);
    if (num_bytes < 0) {
      if (errno == EAGAIN) {
        if (have_data) {
          // print stats
          std::cout << "total received=" << total_recv << ";seq received=" <<
                       seq_recv << ";send back=" << sent_back << 
                       ";total out-of-order=" << out_of_order <<
                       ";total unexpected=" << unexpected << std::endl;
          have_data = false;
        } else {
          // TODO(xunfan): erase time out here
          continue;
        }
      } else if (errno == EINTR) {
        continue;
      }else {
        LOG(FATAL, "Receive fails! %s [%d].", strerror(errno), errno);
      }
    }

    total_recv++;

    if (recv_packet.length() < (kPayloadHeaderLength + sizeof(uint32_t))) {
      LOG(VERBOSE, "recv a packet smaller than min size.");
      unexpected++;
      continue;
    }

    std::string head(recv_packet.buffer(), kPayloadHeaderLength);
    if (head != kPayloadHeader){
      LOG(VERBOSE, "recv a packet not for this program.");
      unexpected++;
      continue;
    }

    have_data = true;
    seq_recv++;
    const unsigned int *seq = 
        reinterpret_cast<const unsigned int*>(recv_packet.buffer() +
                                              kPayloadHeaderLength);

    unsigned int rseq = ntohl(*seq);
    if (mrseq > rseq) {
      out_of_order++;
    } else {
      mrseq = rseq;
    }

    mysock->Send(recv_packet, &num_bytes);
    sent_back++;
  }
}
