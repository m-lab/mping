#ifndef _MPING_SERVER_H_
#define _MPING_SERVER_H_

#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include <string>
#include <vector>

#include "mlab/accepted_socket.h"
#include "mlab/socket_family.h"
#include "mlab/socket_type.h"
#include "mp_common.h"

struct ClientStateNode {
  uint64_t total_recv;
  uint64_t sequence_recv;
  uint64_t sent_back;
  uint64_t out_of_order;
  uint64_t unexpected;
  int64_t max_recv_seq;
  ServerEchoMode echo_mode;
  struct timeval start_time;
  struct timeval last_recv_time;

  ClientStateNode(ServerEchoMode mode, struct timeval start) 
    : total_recv(0),
      sequence_recv(0),
      sent_back(0),
      out_of_order(0),
      unexpected(0),
      max_recv_seq(0),
      echo_mode(mode),
      start_time(start),
      last_recv_time(start) {  }
};

class MPingServer {
  public:
    MPingServer(size_t packet_size, uint16_t server_port,
                SocketFamily server_family); 
    void Run();

  private:
    static void *TCPThread(void *that);
    static void *UDPThread(void *that);
    static void *CleanupThread(void *that);

    uint16_t GenerateClientCookie();
    ClientStateNode *CheckClient(uint16_t client_cookie);
    void DeleteClient(uint16_t client_cookie);
    uint16_t InsertClient(ClientStateNode *node);
    void PrintClientStats(ClientStateNode *node) const;

    size_t packet_size_;
    uint16_t server_port_;
    SocketFamily server_family_;
    std::vector<std::pair<uint16_t, ClientStateNode*> > client_map_;
    pthread_mutex_t client_map_mutex_;
    pthread_mutex_t MuxLog_;
    uint32_t client_cookie_num;
};

#endif
