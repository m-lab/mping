#ifndef _MPING_SERVER_H_
#define _MPING_SERVER_H_

#include "string.h"
#include "mlab/socket_family.h"

class MPingServer {
  public:
    MPingServer(size_t packetsize, unsigned short port, SocketFamily family);
    void Run();

  private:
    bool have_data;
    int unexpected;
    int seq_recv;
    int sent_back;
    int total_recv;
    int out_of_order;
    unsigned int mrseq;
    size_t packet_size;
    unsigned short server_port;
    SocketFamily server_family;
};

#endif
