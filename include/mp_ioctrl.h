#ifndef _MPING_IOCTRL_H_
#define _MPING_IOCTRL_H_

#include <set>
#include <stdint.h>
#include <string>
#include "mlab/socket_family.h"

class IOCtrl {
  public:
    int        win_size;  
    bool       loop;
    int        rate;
    bool       slow_start;
    int ttl;
    int inc_ttl;  // auto increase TTL to this value
    size_t     pkt_size;  // packet size in bytes
    int        loop_size;  // -1 to -4
    bool       version;
    bool       debug;
    int        burst;  // burst size
    int        interval;  // undefined now
    int        dport;
    SocketFamily   family;  // IPv4 or IPv6
    std::string src_addr;
    std::string dst_addr;
    std::set<std::string> dest_ips;

    void GetConfigPara(const int& argc, const char **argv); 
    void Start();

    IOCtrl() : 
      win_size(4),
      loop(false),
      rate(0),
      slow_start(false),
      ttl(0),
      inc_ttl(0),
      pkt_size(0),
      loop_size(0),
      version(false),
      debug(false),
      burst(0),
      interval(0),
      dport(0),
      family(SOCKETFAMILY_IPV4),
      src_addr(""),
      dst_addr("") {  }
  
    ~IOCtrl() {  }

  private:
    int GoProbing();
    void ValidatePara();
};

#endif  // _MPING_IOCTRL_H_
