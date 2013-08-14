#include <iostream>
#include <string.h>
#include <errno.h>

#include "mp_client.h"
#include "mp_common.h"

namespace {
char usage[] = 
"Usage:  mping [<switch> [<val>]]* <host>\n\
      -n <num>    Number of messages to keep in transit\n\
      -f          Loop forever (Don't increment # messages in transit)\n\
      -R <rate>   Rate at which to limit number of messages in transit\n\
      -S          Use a TCP style slowstart\n\
\n\
      -t <ttl>    Send UDP packets (instead of ICMP) with a TTL of <ttl>\n\
      -a <ttlmax> Auto-increment TTL up to ttlmax.  Forces -t\n\
\n\
      -b <len>    Message length in bytes, including IP header, etc\n\
      -l <sel>    Loop through message sizes: 1:selected sizes\n\
                  or steps of: 2:64 3:128 4:256\n\
      -B <bnum>   Send <bnum> packets in burst, should smaller than <num>\n\
      -p <port>   If UDP, destination port number\n\
\n\
      -c <mode>   Client mode, sending with UDP to a mping server\n\
                  <mode> coule be 1 and 2, 1 means server echo back\n\
                  whole pakcet, 2 means server echo back first 24 bytes\n\
      -r          Print time and sequence number of every send/recv packet\n\
                  The time is relative to the first packet sent\n\
                  A negative sequence number indicates a recv packet\n\
                  Be careful, there usually are huge number of packets\n\
\n\
      -V, -d  Version, Debug (verbose)\n\
\n\
      -F <addr>   Select a source interface\n\
      <host>     Target host\n";

}  // namespace

int main(int argc, const char** argv) {
  int ac = argc;
  const char **av = argv;
  const char *p;
  bool host_set = false;

  int win_size = 4;
  int rate = 0;
  int ttl = 0;
  int inc_ttl = 0;
  int loop_size = 0;
  int burst = 0;
  uint16_t dport = 0;
  uint16_t client_mode = 0;
  size_t pkt_size = 0;
  bool loop = false;
  bool slow_start = false;
  bool version = false;
  bool debug = false;
  bool print_seq_time = false;
  std::string src_addr;
  std::string dst_host;

  if (argc < 2) {
    printf("%s", usage);
    exit(0);
  }

  ac--;
  av++;

  while (ac > 0) {
    p = *av++;  // p point to switch, av point to value
    if (p[0] == '-') {
      if (ac == 1) {
        switch (p[1]) {  // those switches without value
          case 'f': loop = true; av--; break;
          case 'S': slow_start = true; av--; break;
          case 'V': version = true; av--; break;
          case 'd': debug = true; av--; break;
          case 'r': print_seq_time = true; av--; break;
          default: LOG(FATAL, "\n%s", usage); break;
        }
      } else {
        switch (p[1]) {
          case 'n': { win_size = atoi(*av); ac--; break; }
          case 'f': { loop = true; av--; break; }
          case 'R': { rate = atoi(*av); ac--; break; }
          case 'S': { slow_start = true; av--; break; }
          case 't': { ttl = atoi(*av); ac--; break; }
          case 'a': { inc_ttl = atoi(*av); ttl = inc_ttl; ac--; break; }
          case 'b': { pkt_size = atoi(*av); ac--; break; }
          case 'l': { loop_size = atoi(*av); ac--; break; }
          case 'p': { dport = atoi(*av); ac--; break; }
          case 'B': { burst = atoi(*av); ac--; break; }
          case 'V': { version = true; av--; break; }
          case 'd': { debug = true; av--; break; }
          case 'c': { client_mode = atoi(*av); ac--; break; }
          case 'r': { print_seq_time = true; av--; break; }
          case 'F': { src_addr = std::string(*av); ac--; break; }
          default: { 
            LOG(FATAL, "Unknown parameter -%c\n%s", p[1], usage); break; 
          }
        }
      }
    } else {  // host
      if (!host_set) {
        dst_host = std::string(p);  
        av--;
        host_set = true;
      }else{
        LOG(FATAL, "%s, %s\n%s", p, dst_host.c_str(), usage);
      }
    }

    ac--;
    av++;
  }

  // validate parameters
  if (version) {
    std::cout << kClientVersion << std::endl;
    exit(0);
  }

  if (debug) {
    SetLogSeverity(VERBOSE);
  }

  // destination set?
  if (dst_host.empty()) {
    LOG(FATAL, "Must have destination host. \n%s", usage);
  }

  // client mode
  if (client_mode > 0) {
    if (dport == 0) {
      LOG(FATAL, "Client mode must have destination port using -p.");
    }

    if (client_mode > 2) {
      LOG(FATAL, "Client mode can only be set to 1 or 2.");
    }

    if (ttl == 0) {
      ttl = kDefaultTTL;
    }
  }
  
  // TTL
  if (ttl > 255 || ttl < 0) {
    ttl = 255;
    LOG(WARNING, "TTL %d is either > 255 or < 0, " 
                       "now set TTL to 255.", ttl);
  } 

  // loop_size [1, 4]
  if (loop_size > 4 || loop_size < 0) {
    LOG(FATAL, "loop through message size could only take"
                     "1, 2, 3 or 4");
  }

  // inc_ttl
  if (inc_ttl > 255 || inc_ttl < 0) {
    inc_ttl = 255;
    LOG(WARNING, "Auto-increment TTL %d is either > 255 or < 0, "
                       "now set auto-increment TTL to 255.", inc_ttl);
  }

  // destination host
  mlab::Host dest(dst_host);
  if (dest.resolved_ips.empty()) {
    LOG(FATAL, "Destination host %s invalid.", dst_host.c_str());
  }

  // max packet size
  if (pkt_size > 65535) {
    LOG(FATAL, "Packet size cannot larger than 65535.");
  }

  // validate local host
  if (src_addr.length() > 0) {
    if (mlab::GetSocketFamilyForAddress(src_addr) == 
            SOCKETFAMILY_UNSPEC) {
      LOG(FATAL, "Local host %s invalid. Only accept numeric IP address",
          src_addr.c_str());
    }
  } 

  // validate UDP destination port
  if (dport > 0 ) {
    if (ttl == 0 && client_mode == 0) {
      LOG(FATAL, "-p can only use together with -t -a or -c.\n%s", usage);
    }

    if (dport > 65535) {
      LOG(FATAL, "UDP destination port cannot larger than 65535.");
    }
  }


  MPingClient mpclient(pkt_size, win_size, loop, rate, slow_start, ttl, inc_ttl,
                       loop_size, burst, dport, 
                       client_mode, print_seq_time, src_addr, dst_host);

  mpclient.Run();

  return 0;
}

