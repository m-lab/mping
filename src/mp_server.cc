#include <arpa/inet.h>
#include <errno.h>
#include <netinet/ip_icmp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <iostream>
#include <algorithm>

#include "mp_common.h"
#include "mp_log.h"
#include "mp_server.h"
#include "mlab/mlab.h"
#include "mlab/listen_socket.h"
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

  // start threads for two sockets and regular clean up
  pthread_t tcp_thread;
  pthread_t udp_thread;
  pthread_t cleanup_thread;

  pthread_mutex_init(&MuxClientMap, NULL);
  pthread_mutex_init(&MuxLog, NULL);

  if (pthread_create(&tcp_thread, 0, &TCPThread, this) != 0) {
    LOG(FATAL, "Cannot create TCP thread. %s [%d]", strerror(errno), errno);
  }
  
  if (pthread_create(&udp_thread, 0, &UDPThread, this) != 0) {
    LOG(FATAL, "Cannot create UDP thread. %s [%d]", strerror(errno), errno);
  }

  if (pthread_create(&cleanup_thread, 0, &CleanupThread, 
                     this) != 0) {
    LOG(FATAL, "Cannot create Cleanup thread. %s [%d]", strerror(errno), errno);
  }

  LOG(INFO, "Waiting threads to exit.");
  pthread_join(tcp_thread, NULL);
  pthread_join(udp_thread, NULL);
  pthread_join(cleanup_thread, NULL);
}

uint16_t MPingServer::GenerateClientCookie() {
  srand(time(NULL));
  uint16_t r = rand() % 65535;

  if (client_map.empty())
    return r;

  int generate_time = 0;
  while (CheckClient(r) != NULL) {
    r = rand() % 65535;
    generate_time++;

    if (generate_time >= 65535)
      LOG(FATAL, "failed to generate client cookie.");
  }

  return r;
}

void MPingServer::PrintClientStats(ClientStateNode *node) const {
  // tba
  
}

void MPingServer::DeleteClient(uint16_t client_cookie) {
  std::vector<std::pair<uint16_t, ClientStateNode*> >::iterator it =
        std::lower_bound(client_map.begin(), client_map.end(),
                         client_cookie, CompareClientCookie());

  if ((*it).first == client_cookie) {
    PrintClientStats((*it).second);
    delete (*it).second;
    client_map.erase(it);
  } else {
    LOG(ERROR, "Client to be deleted is not in client map.");
  }
}

void MPingServer::InsertClient(uint16_t client_cookie,
                               ClientStateNode *node) {
  if (client_map.empty()) {
    client_map.insert(client_map.begin(),
                      std::pair<uint16_t, ClientStateNode*>(client_cookie,
                                                            node));
    return;
  }

  std::vector<std::pair<uint16_t, ClientStateNode*> >::iterator it = 
        std::lower_bound(client_map.begin(), client_map.end(),
                         client_cookie, CompareClientCookie());

  if ((*it).first == client_cookie) {
    LOG(ERROR, "Client already exist in client map.");
    return;
  }

  client_map.insert(it, 
                    std::pair<uint16_t, ClientStateNode*>(client_cookie,
                                                          node));
}

ClientStateNode *MPingServer::CheckClient(uint16_t client_cookie) {
  if (client_map.empty())
    return NULL;

  std::vector<std::pair<uint16_t, ClientStateNode*> >::iterator it = 
        std::lower_bound(client_map.begin(), client_map.end(),
                         client_cookie, CompareClientCookie());

  if ((*it).first == client_cookie)
    return (*it).second;
  else
    return NULL;
}

void *MPingServer::CleanupThread(void *that) {
  MPingServer* self = static_cast<MPingServer*>(that);

  timeval now;
  while (1) {
    gettimeofday(&now, 0);

    pthread_mutex_lock(&self->MuxClientMap);

    size_t mapsize = self->client_map.size();
    if (mapsize > 0) {
      for (size_t i = 0; i < mapsize; i++) {
        if (now.tv_sec - self->client_map.at(i).second->last_recv_time.tv_sec >
            kDefaultCleanUpTime) {
          self->PrintClientStats(self->client_map.at(i).second);
          delete self->client_map.at(i).second;  // free memory
          self->client_map.erase(self->client_map.begin() + i);
          mapsize--;
          i--;
        }
      }
    }

    pthread_mutex_unlock(&self->MuxClientMap);

    sleep(static_cast<unsigned int>(kDefaultCleanUpTime));
  }

  return NULL;
}

void *MPingServer::UDPThread(void *that) {
  MPingServer* self = static_cast<MPingServer*>(that);
  // create UDP server socket
  scoped_ptr<mlab::ListenSocket>
      myUDPsock(mlab::ListenSocket::CreateOrDie(self->server_port,
                                                SOCKETTYPE_UDP,
                                                self->server_family));

  scoped_ptr<mlab::AcceptedSocket> mysock(myUDPsock->Accept());

  // check UDP socket buffer size
  if (mysock->GetRecvBufferSize() < self->packet_size) {
    LOG(VERBOSE, "Receive buffer size is smaller than needed, "
                       "set to needed.");
    mysock->SetRecvBufferSize(self->packet_size);
  }

  if (mysock->GetSendBufferSize() < self->packet_size) {
    LOG(VERBOSE, 
        "Send buffer size is smaller than needed, set to needed.");
    mysock->SetSendBufferSize(self->packet_size);
  }

  // remove timeout for UDP socket
  timeval zero = {0, 0};
  setsockopt(mysock->raw(), SOL_SOCKET, SO_RCVTIMEO,
             (const char*) &zero, sizeof(zero));
  setsockopt(mysock->raw(), SOL_SOCKET, SO_SNDTIMEO,
             (const char*) &zero, sizeof(zero));

  timeval now;
  bool is_big_end = IsBigEndian();
  while (1) {
    errno = 0;
    ssize_t num_bytes = 0;
    mlab::Packet recv_packet = mysock->Receive(self->packet_size, &num_bytes);
    if (num_bytes < 0) {
      if (errno == EINTR) {
        continue;
      }else {
        LOG(FATAL, "Receive fails! %s [%d].", strerror(errno), errno);
      }
    }

    //std::string client_str = self->GetClientString(mysock.get(), 
    //                                               SOCKETTYPE_UDP);
    

    if (recv_packet.length() < kSequenceOffset) {
      LOG(VERBOSE, "recv a packet smaller than min size.");
      continue;
    }

    std::string head(recv_packet.buffer(), kStrHeaderLength);
    if (head != kStrHeader){
      LOG(VERBOSE, "recv a packet not for this program.");
      continue;
    }

    uint16_t my_cookie = 
          ntohs(*(reinterpret_cast<const uint16_t*>(recv_packet.buffer() +
                                                    kCookieOffset)));

    pthread_mutex_lock(&self->MuxClientMap);
    ClientStateNode *statenode = self->CheckClient(my_cookie);
    pthread_mutex_unlock(&self->MuxClientMap);

    if (statenode == NULL) {
      pthread_mutex_lock(&self->MuxLog);
      LOG(ERROR, "recv UDP packet from unregistered client.");
      pthread_mutex_unlock(&self->MuxLog);
      continue;
    }


    if (recv_packet.length() < kAllHeaderLen) {
      LOG(VERBOSE, "recv UDP packet smaller than min size.");
      statenode->unexpected++;
      continue;
    }


    statenode->total_recv++;

    const MPSEQTYPE *pseq = 
        reinterpret_cast<const MPSEQTYPE*>(recv_packet.buffer() +
                                           kSequenceOffset);

    MPSEQTYPE rseq = MPntoh64(*pseq, is_big_end);
    statenode->sequence_recv++;

    if (statenode->max_recv_seq > rseq) {
      statenode->out_of_order++;
    } else {
      statenode->max_recv_seq = rseq;
    }

    num_bytes = 0;
    switch (statenode->echo_mode) {
      case ECHOMODE_WHOLE: {
         mysock->Send(recv_packet, &num_bytes);
         break;
      }
      case ECHOMODE_SMALL: {
         mysock->Send(mlab::Packet(recv_packet.buffer(), kAllHeaderLen), 
                      &num_bytes);
         break;
      }
      case ECHOMODE_LARGE: {  // now LARGE is the same as SMALL
         mysock->Send(mlab::Packet(recv_packet.buffer(), kAllHeaderLen),
                      &num_bytes);
         break;
      }
      default: {
         LOG(FATAL, "Client map node unknow echo_mode.");
         break;
      }
    }
    statenode->sent_back++;
    gettimeofday(&now, 0);
    statenode->last_recv_time = now;
  }

  return NULL;
}

void *MPingServer::TCPThread(void *that) {
  MPingServer* self = static_cast<MPingServer*>(that);

  // create TCP server socket
  pthread_mutex_lock(&self->MuxLog);
  LOG(INFO, "create TCP socket. port %d", self->server_port);
  pthread_mutex_unlock(&self->MuxLog);

  scoped_ptr<mlab::ListenSocket>
      myTCPsock(mlab::ListenSocket::CreateOrDie(self->server_port, 
                                                SOCKETTYPE_TCP,
                                                self->server_family)); 

  while (1) {
    myTCPsock->Select();

    scoped_ptr<mlab::AcceptedSocket> mysock(myTCPsock->Accept());

    char message_buffer[kMPTCPMessageSize];
    bzero(message_buffer, kMPTCPMessageSize);

    if (StreamSocketRecvWithTimeout(mysock->client_raw(), message_buffer, 
                                    kMPTCPMessageSize) < 0) {
      if (errno == EAGAIN)
        continue;
      else
        LOG(FATAL, "Receive fails! %s [%d].", strerror(errno), errno);
    }

    // switch on message code
    MPTCPMessage *tcpmsg = (MPTCPMessage *)message_buffer;

    switch (GetTCPMsgCodeFromString(std::string(tcpmsg->msg_code, 4))) {
      case MPTCP_HELLO: {
        struct timeval now;
        gettimeofday(&now, 0);
        ClientStateNode *node = 
            new ClientStateNode(GetTCPServerEchoModeFromShort(
                ntohs(tcpmsg->msg_type)), now);

        pthread_mutex_lock(&self->MuxClientMap);
        // get cookie first
        uint16_t cookie = self->GenerateClientCookie();

        // insert to client map
        self->InsertClient(cookie, node);
        pthread_mutex_unlock(&self->MuxClientMap);

        // send TCP confirm, cookie is the msg_value
        MPTCPMessage cfrm_msg(MPTCP_CONFIRM, ntohs(tcpmsg->msg_type),
                              cookie);
  
        if (StreamSocketSendWithTimeout(mysock->client_raw(), 
                                       reinterpret_cast<const char*>(&cfrm_msg),
                                       kMPTCPMessageSize) < 0)
          LOG(FATAL, "Server tcp send fails.");

        break;
      }
      case MPTCP_DONE: {
        pthread_mutex_lock(&self->MuxClientMap);
        self->DeleteClient(ntohs(tcpmsg->msg_value));
        pthread_mutex_unlock(&self->MuxClientMap);
        break;
      }
      default: {
        LOG(ERROR, "Server receive unknow TCP packet.");
        break;
      }
    }
  }

  return NULL;
}
