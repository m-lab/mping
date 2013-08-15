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
struct CompareClientCookie {
  bool operator() (const std::pair<uint16_t, ClientStateNode*>& p,
                   uint16_t c) {
    return p.first < c;
  }
};
}  // namespace

MPingServer::MPingServer(size_t packet_size, uint16_t server_port,
                         SocketFamily server_family) 
    : packet_size_(std::max(packet_size, kMinBuffer)),
      server_port_(server_port),
      server_family_(server_family),
      client_cookie_num(0) { }

void MPingServer::Run() {
  MPLOG(MPLOG_DEF, "Running server mode, port %u.", server_port_);

  // start threads for two sockets and regular clean up

  pthread_mutex_init(&client_map_mutex_, NULL);
  pthread_mutex_init(&MuxLog_, NULL);

  pthread_t tcp_thread;
  if (pthread_create(&tcp_thread, 0, &TCPThread, this) != 0) {
    LOG(FATAL, "Cannot create TCP thread. %s [%d]", strerror(errno), errno);
  }
  
  pthread_t udp_thread;
  if (pthread_create(&udp_thread, 0, &UDPThread, this) != 0) {
    LOG(FATAL, "Cannot create UDP thread. %s [%d]", strerror(errno), errno);
  }

  pthread_t cleanup_thread;
  if (pthread_create(&cleanup_thread, 0, &CleanupThread, 
                     this) != 0) {
    LOG(FATAL, "Cannot create Cleanup thread. %s [%d]", strerror(errno), errno);
  }

  LOG(INFO, "Waiting threads to exit.");
  pthread_exit(NULL);
}

uint16_t MPingServer::GenerateClientCookie() {
  client_cookie_num++;

  uint16_t r = client_cookie_num % 65535;

  if (client_map_.empty())
    return r;

  int generate_time = 0;
  while (CheckClient(r) != NULL) {
    r = (++client_cookie_num) % 65535;

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
  pthread_mutex_lock(&client_map_mutex_);
  std::vector<std::pair<uint16_t, ClientStateNode*> >::iterator it =
        std::lower_bound(client_map_.begin(), client_map_.end(),
                         client_cookie, CompareClientCookie());

  if (it != client_map_.end()) {
    if (it->first != client_cookie) {
      pthread_mutex_unlock(&client_map_mutex_);
      LOG(FATAL, "Bad comparator.");
    }

    PrintClientStats(it->second);
    delete it->second;
    client_map_.erase(it);
    pthread_mutex_unlock(&client_map_mutex_);
  } else {
    pthread_mutex_unlock(&client_map_mutex_);
    LOG(ERROR, "Client to be deleted is not in client map.");
  }
}

uint16_t MPingServer::InsertClient(ClientStateNode *node) {
  pthread_mutex_lock(&client_map_mutex_);

  uint16_t client_cookie = GenerateClientCookie();

  if (client_map_.empty()) {
    client_map_.push_back(std::pair<uint16_t, ClientStateNode*>(client_cookie,
                                                                node));
    pthread_mutex_unlock(&client_map_mutex_);
    return client_cookie;
  }

  std::vector<std::pair<uint16_t, ClientStateNode*> >::iterator it = 
        std::lower_bound(client_map_.begin(), client_map_.end(),
                         client_cookie, CompareClientCookie());

  if (it->first == client_cookie) {
    LOG(ERROR, "Client already exist in client map.");
    pthread_mutex_unlock(&client_map_mutex_);
    return client_cookie;
  }

  client_map_.insert(it, 
                    std::pair<uint16_t, ClientStateNode*>(client_cookie,
                                                          node));
  pthread_mutex_unlock(&client_map_mutex_);
  return client_cookie;
}

ClientStateNode *MPingServer::CheckClient(uint16_t client_cookie) {
  if (client_map_.empty())
    return NULL;

  std::vector<std::pair<uint16_t, ClientStateNode*> >::iterator it = 
        std::lower_bound(client_map_.begin(), client_map_.end(),
                         client_cookie, CompareClientCookie());

  if (it->first == client_cookie)
    return it->second;
  else
    return NULL;
}

void *MPingServer::CleanupThread(void *that) {
  MPingServer* self = static_cast<MPingServer*>(that);

  timeval now;
  while (1) {
    gettimeofday(&now, 0);

    pthread_mutex_lock(&self->client_map_mutex_);

    for (std::vector<std::pair<uint16_t, ClientStateNode*> >::iterator iter = 
              self->client_map_.begin(); iter != self->client_map_.end();) {
      if (now.tv_sec - iter->second->last_recv_time.tv_sec >
          kDefaultCleanUpTime) {
        self->PrintClientStats(iter->second);
        delete iter->second;
        iter = self->client_map_.erase(iter);
      } else {
        ++iter;
      }
    }

    pthread_mutex_unlock(&self->client_map_mutex_);

    sleep(static_cast<unsigned int>(kDefaultCleanUpTime));
  }

  return NULL;
}

void *MPingServer::UDPThread(void *that) {
  MPingServer* self = static_cast<MPingServer*>(that);
  // create UDP server socket
  scoped_ptr<mlab::ListenSocket>
      myUDPsock(mlab::ListenSocket::CreateOrDie(self->server_port_,
                                                SOCKETTYPE_UDP,
                                                self->server_family_));

  scoped_ptr<mlab::AcceptedSocket> mysock(myUDPsock->Accept());

  // check UDP socket buffer size
  if (mysock->GetRecvBufferSize() < self->packet_size_) {
    LOG(VERBOSE, "Receive buffer size is smaller than needed, "
                       "set to needed.");
    mysock->SetRecvBufferSize(self->packet_size_);
  }

  if (mysock->GetSendBufferSize() < self->packet_size_) {
    LOG(VERBOSE, 
        "Send buffer size is smaller than needed, set to needed.");
    mysock->SetSendBufferSize(self->packet_size_);
  }

  // remove timeout for UDP socket
  timeval zero = {0, 0};
  setsockopt(mysock->raw(), SOL_SOCKET, SO_RCVTIMEO,
             (const char*) &zero, sizeof(zero));
  setsockopt(mysock->raw(), SOL_SOCKET, SO_SNDTIMEO,
             (const char*) &zero, sizeof(zero));

  timeval now;
  while (1) {
    errno = 0;
    ssize_t num_bytes = 0;
    mlab::Packet recv_packet = mysock->Receive(self->packet_size_, &num_bytes);
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

    pthread_mutex_lock(&self->client_map_mutex_);
    ClientStateNode *statenode = self->CheckClient(my_cookie);
    if (statenode != NULL) {
      gettimeofday(&now, 0);
      statenode->last_recv_time = now;
    }
    pthread_mutex_unlock(&self->client_map_mutex_);

    if (statenode == NULL) {
      LOG(ERROR, "recv UDP packet from unregistered client.");
      continue;
    }


    if (recv_packet.length() < kAllHeaderLen) {
      LOG(VERBOSE, "recv UDP packet smaller than min size.");
      statenode->unexpected++;
      continue;
    }

    statenode->total_recv++;

    const int64_t *pseq = 
        reinterpret_cast<const int64_t*>(recv_packet.buffer() +
                                           kSequenceOffset);

    int64_t rseq = MPntoh64(*pseq);
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
      case ECHOMODE_LARGE: {  // not implemented
         LOG(FATAL, "ECHOMODE_LARGE not implemented.");
         break;
      }
      default: {
         LOG(FATAL, "Client map node unknow echo_mode.");
         break;
      }
    }
    statenode->sent_back++;
  }

  return NULL;
}

void *MPingServer::TCPThread(void *that) {
  MPingServer* self = static_cast<MPingServer*>(that);

  // create TCP server socket
  pthread_mutex_lock(&self->MuxLog_);
  LOG(INFO, "create TCP socket. port %d", self->server_port_);
  pthread_mutex_unlock(&self->MuxLog_);

  scoped_ptr<mlab::ListenSocket>
      myTCPsock(mlab::ListenSocket::CreateOrDie(self->server_port_, 
                                                SOCKETTYPE_TCP,
                                                self->server_family_)); 

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

        // insert to client map
        uint16_t cookie = self->InsertClient(node);

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
        self->DeleteClient(ntohs(tcpmsg->msg_value));
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
