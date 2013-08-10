#ifndef _MPING_SERVER_H_
#define _MPING_SERVER_H_

#include <stdint.h>
#include <string.h>
#include "mlab/socket_family.h"
#include "mp_common.h"

class MPingServer {
  public:
    MPingServer(const int& argc, const char **argv); 
    void Run();

  private:
    bool have_data;
    int64_t unexpected;
    int64_t seq_recv;
    int64_t sent_back;
    int64_t total_recv;
    int64_t out_of_order;
    MPSEQTYPE mrseq;
    size_t packet_size;
    uint16_t server_port;
    SocketFamily server_family;
};

#endif
