// Copyright 2013 M-Lab. All Rights Reserved.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _MPING_MPING_H_
#define _MPING_MPING_H_

#include <stdint.h>
#include <set>
#include <string>
#include "mlab/socket_family.h"

//#define MP_PRINT_TIMELINE

class MPing{
  public:

    MPing(const int& argc, const char **argv); 
    void Run();

        
  private:
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
    std::string src_addr;
    std::string dst_host;
    std::set<std::string> dest_ips;

    int GoProbing(const std::string& dst_addr);
    void ValidatePara();
};

#endif  // _MPING_MPING_H_
