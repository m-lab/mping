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

#include "mlab/socket_family.h"
#include "mp_socket.h"
#include "mp_stats.h"

#include <stdint.h>
#include <set>
#include <string>

//#define MP_PRINT_TIMELINE

class MPingClient{
  public:
    size_t     pkt_size;  // packet size in bytes
//    uint16_t  server_port;
//    SocketFamily server_family;

    MPingClient(const int& argc, const char **argv); 
//    bool IsServerMode() const;
    void Run();

  protected:
    FRIEND_TEST(MpingPacketIO, DynWindow);
    FRIEND_TEST(MpingPacketIO, DynWindowBurst);
    FRIEND_TEST(MpingUnitTest, GetNeedSendTest);
    // config parameters
    int        win_size;  
    bool       loop;
    int        rate;
    bool       slow_start;
    int        ttl;
    int        inc_ttl;  // auto increase TTL to this value
    int        loop_size;  // 1 to 4
    bool       version;
    bool       debug;
    int        burst;  // burst size
    int        interval;  // undefined now
    int        dport;
    uint16_t   client_mode;
    uint16_t   client_cookie;
    std::string src_addr;
    std::string dst_host;
    std::set<std::string> dest_ips;
    bool       print_seq_time;
    MpingStat  mp_stat;

    void ValidatePara();
    virtual bool GoProbing(const std::string& dst_addr);
    virtual bool TTLLoop(MpingSocket *sock);
    virtual bool BufferLoop(MpingSocket *sock);
    virtual bool WindowLoop(MpingSocket *sock);
    virtual bool IntervalLoop(int intran, MpingSocket *sock);
    int GetNeedSend(int _burst, bool _start_burst, bool _slow_start,
                    int64_t _sseq, int64_t _mrseq, int intran,
                    int mustsend);

    // parameters used during running
    struct timeval now;
    bool start_burst;
    int64_t sseq;
    int64_t mrseq;
    size_t cur_packet_size;

    // signal handling
    static int haltf;
    static long tick;
    static bool timedout;

    static void ring(int signo);
    static void halt(int signo);
    void InitSigAlarm();
    void InitSigInt();
};

#endif  // _MPING_MPING_H_
