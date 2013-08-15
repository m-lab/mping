#include <iostream>
#include <string.h>
#include <errno.h>

#include "mp_common.h"
#include "mp_server.h"

namespace {
  char usage[] =
"Usage:  mping_server [-6] [-b <packet_size>] [-V] [-d] <port>\n\
      -6               Listen on IPv6 address\n\
      -b <packet_size> Set socket buffer size. Use this if you\n\
                       think your packet will be very large (> 9000 bytes). \n\
      -V, -d  Version, Debug (verbose)\n\
      <port>           Listen port\n";
}  // namespace

int main(int argc, const char** argv) {
  bool port_set = false;

  size_t packet_size = kMinBuffer;
  uint16_t server_port = 0;
  SocketFamily server_family = SOCKETFAMILY_IPV4;
  bool version = false;
  bool debug = false;
  
  if (argc < 2) {
    printf("%s", usage);
    exit(0);
  }

  argc--;
  argv++;

  while (argc > 0) {
    const char *p = *argv++;

    if (p[0] == '-') {
      if (argc == 1) {
        switch (p[1]) {  // those switches without value
          case '6': server_family = SOCKETFAMILY_IPV6; argv--; break;
          case 'V': version = true; argv--; break;
          case 'd': debug = true; argv--; break;
          default: LOG(FATAL, "\n%s", usage); break;
        }
      } else {
        switch (p[1]) {
          case 'b': { packet_size = strtol(*argv, NULL, 10); argc--; break; }
          case '6': { server_family = SOCKETFAMILY_IPV6; argv--; break; }
          case 'V': version = true; argv--; break;
          case 'd': debug = true; argv--; break;
          default: { 
            LOG(FATAL, "Unknown parameter -%c\n%s", p[1], usage); break; 
          }
        }
      }
    } else {  // port
      if (!port_set) {
        server_port = strtol(p, NULL, 10);
        argv--;
        port_set = true;
      }else{
        LOG(FATAL, "try to set port %s, but port is already set to %d\n%s", 
            p, server_port, usage);
      }
    }

    argc--;
    argv++;
  }

  if (version) {
    std::cout << kVersion << std::endl;
    exit(0);
  }

  if (debug) {
    SetLogSeverity(VERBOSE);
  }

  MPingServer mpserver(packet_size, server_port, server_family);

  mpserver.Run();

  return 0;
}
