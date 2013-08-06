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

MPingServer::MPingServer(size_t packetsize, unsigned short port,
                         SocketFamily family) :
  have_data(false),
  unexpected(0),
  seq_recv(0),
  sent_back(0),
  total_recv(0),
  out_of_order(0),
  mrseq(0),
  packet_size(std::max(packetsize, kMinBuffer)),
  server_port(port),
  server_family(family) { }

void MPingServer::Run() {
  LOG(INFO, "Running server mode, port %u.", server_port);

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
      } else {
        LOG(FATAL, "Receive fails! %s [%d].", strerror(errno), errno);
      }
    }

    total_recv++;

    if (recv_packet.length() < (kPayloadHeaderLength + sizeof(unsigned int))) {
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
