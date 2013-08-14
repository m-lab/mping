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
    MPingClient(size_t pkt_size, int win_size, bool loop, int rate,
                bool slow_start, int ttl, int inc_ttl, 
                int loop_size, int burst, uint16_t dport,
                uint16_t client_mode, bool print_seq_time,
                const std::string& src_addr,
                const std::string& dst_host);
    void Run();

  protected:
    FRIEND_TEST(MpingPacketIO, DynWindow);
    FRIEND_TEST(MpingPacketIO, DynWindowBurst);
    FRIEND_TEST(MpingUnitTest, GetNeedSendTest);

    // config parameters
    size_t     pkt_size_;  // packet size in bytes
    int        win_size_;  
    bool       loop_;
    int        rate_;
    bool       slow_start_;
    int        ttl_;
    int        inc_ttl_;  // auto increase TTL to this value
    int        loop_size_;  // 1 to 4
    int        burst_;  // burst size
    int        dport_;
    uint16_t   client_mode_;
    uint16_t   client_cookie_;
    std::string src_addr_;
    std::string dst_host_;
    std::set<std::string> dest_ips_;
    bool       print_seq_time_;
    MpingStat  mp_stat_;

    // parameters used during running
    struct timeval now_;
    bool start_burst_;
    int64_t sseq_;
    int64_t mrseq_;
    size_t cur_packet_size_;

    virtual bool GoProbing(const std::string& dst_addr);
    virtual bool TTLLoop(MpingSocket* sock);
    virtual bool BufferLoop(MpingSocket* sock);
    virtual bool WindowLoop(MpingSocket* sock);
    virtual bool IntervalLoop(int intran, MpingSocket* sock);
    int GetNeedSend(int burst, bool start_burst, bool slow_start,
                    int64_t sseq, int64_t mrseq, int intran,
                    int mustsend) const;

    // signal handling
    static int haltf_;
    static long tick_;
    static bool timedout_;

    static void ring(int signo);
    static void halt(int signo);
    void InitSigAlarm();
    void InitSigInt();
};

#endif  // _MPING_MPING_H_
