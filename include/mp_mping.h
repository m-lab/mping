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

#include "mp_socket.h"
#include "mp_stats.h"
#include "mlab/socket_family.h"

//#define MP_PRINT_TIMELINE

class MPing{
  public:
    MPing(const int& argc, const char **argv); 
    bool IsServerMode() const;
    void RunServer();
    void RunClient();

  protected:
    FRIEND_TEST(MpingPacketIO, DynWindow);
    FRIEND_TEST(MpingPacketIO, DynWindowBurst);
    // config parameters
    int        win_size;  
    bool       loop;
    int        rate;
    bool       slow_start;
    int        ttl;
    int        inc_ttl;  // auto increase TTL to this value
    size_t     pkt_size;  // packet size in bytes
    int        loop_size;  // -1 to -4
    bool       version;
    bool       debug;
    int        burst;  // burst size
    int        interval;  // undefined now
    int        dport;
    unsigned short  server_port;
    SocketFamily server_family;
    bool       client_mode;
    std::string src_addr;
    std::string dst_host;
    std::set<std::string> dest_ips;
    MpingStat  mp_stat;
    bool       print_seq_time;

    void ValidatePara();
    virtual bool GoProbing(const std::string& dst_addr);
    virtual void TTLLoop(MpingSocket *sock, MpingStat *stat);
    virtual void BufferLoop(MpingSocket *sock, MpingStat *stat);
    virtual void WindowLoop(MpingSocket *sock, MpingStat *stat);
    virtual void IntervalLoop(uint16_t intran, MpingSocket *sock, 
                              MpingStat *stat);

    // parameters used during running
    int mustsend;
    bool start_burst;
    unsigned int sseq;
    unsigned int mrseq;
    size_t cur_packet_size;

    // signal handling
    static int haltf;
    static int tick;
    static bool timedout;

    static void ring(int signo);
    static void halt(int signo);
    void InitSigAlarm();
    void InitSigInt();
};

#endif  // _MPING_MPING_H_
