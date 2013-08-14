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

#ifndef _MP_SOCKET_H_
#define _MP_SOCKET_H_
#include <string.h>

#include "mlab/client_socket.h"
#include "mlab/socket_family.h"
#include "mlab/raw_socket.h"
#include "mp_common.h"
#include "mp_stats.h"

class MpingSocket {
  public:
    MpingSocket() :
      icmp_sock(NULL),
      udp_sock(NULL),
      family_(SOCKETFAMILY_UNSPEC),
      buffer_length_(0),
      use_udp_(false),
      client_mode_(0),
      is_big_end(IsBigEndian()),
      destport_(0) {
        memset(&srcaddr_, 0, sizeof(srcaddr_));
        memset(buffer_, 0, sizeof(buffer_));
    }
    
    int Initialize(const std::string& destip, const std::string& srcip,
                   int ttl, size_t pktsize, int wndsize, 
                   uint16_t port, uint16_t clientmode);
    ~MpingSocket();

    bool SetSendTTL(const int& ttl);
    size_t SendPacket(const MPSEQTYPE& seq, uint16_t client_cookie,
                      size_t size, int *error) const;
    MPSEQTYPE ReceiveAndGetSeq(int* error, MpingStat *mpstat);
    const std::string GetFromAddress() const;

    // TCP communication with server end
    bool SendHello(uint16_t *cookie);
    void SendDone(uint16_t cookie);

  protected:
    mlab::RawSocket *icmp_sock;
    mlab::ClientSocket *udp_sock;

  private:
    MpingSocket(const MpingSocket& other); 
    MpingSocket& operator = (const MpingSocket&);

    SocketFamily family_;
    sockaddr_storage srcaddr_;
    char buffer_[64];
    int buffer_length_;
    bool use_udp_;
    uint16_t client_mode_;
    std::string fromaddr_;
    bool is_big_end;
    std::string destaddr_;
    uint16_t destport_;
    std::string bindaddr_;
};

#endif
