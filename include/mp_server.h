#ifndef _MPING_SERVER_H_
#define _MPING_SERVER_H_

#include <stdint.h>
#include <string.h>
#include "mlab/socket_family.h"

class MPingServer {
  public:
    MPingServer(const int& argc, const char **argv); 
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
    uint16_t server_port;
    SocketFamily server_family;
};

#endif
