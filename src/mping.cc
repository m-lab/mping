#include <iostream>
#include <string.h>
#include <errno.h>

#include "mp_mping.h"
#include "mp_server.h"

int main(int argc, const char** argv) {
  MPing mp(argc, argv);

  //TODO(xunfan): pull out the parameter handling as seperate code.
  if (mp.IsServerMode()) {
    MPingServer server(mp.pkt_size, mp.server_port, mp.server_family);
    server.Run();
  } else {
    mp.Run();
  }

//  return 0;
}
