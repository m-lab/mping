#include <errno.h>

#ifndef __STDC_FORMAT_MACROS  // for print int64_t with PRIxx Macros
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>
#include <set>

#include "mlab/socket_family.h"
#include "mlab/host.h"
#include "mlab/mlab.h"
#include "mlab/protocol_header.h"
#include "mp_common.h"
#include "mp_log.h"
#include "mp_client.h"
#include "scoped_ptr.h"

namespace {

const int kNbTab[] = {64, 100, 500, 1000, 1500, 2000, 3000, 4000, 0};

// when testing, only send these packets every sec
const int kMaximumOutPacketsInTest = 20;
}  // namespace

int MPingClient::haltf_ = 0;
long MPingClient::tick_ = 0;
bool MPingClient::timedout_ = false;

void MPingClient::ring(int signo) {
  struct sigaction sa, osa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = &MPingClient::ring;
  sa.sa_flags = SA_INTERRUPT;
  if (sigaction(SIGALRM, &sa, &osa) < 0) {
    LOG(FATAL, "sigaction SIGALRM. %s [%d]", strerror(errno), errno);
  }
  timedout_ = true;
  tick_ = 0;
}

void MPingClient::halt(int signo) {
  struct sigaction sa, osa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_INTERRUPT;
  if (++haltf_ >= 2) {
    sa.sa_handler = 0;
    if (sigaction(SIGINT, &sa, &osa)) {
      LOG(FATAL, "sigaction SIGINT. %s [%d]", strerror(errno), errno);
    }
  } else {
    sa.sa_handler = &MPingClient::halt;
    if (sigaction(SIGINT, &sa, &osa) < 0) {
      LOG(FATAL, "sigaction SIGINT. %s [%d]", strerror(errno), errno);
    }
  }
}

void MPingClient::InitSigAlarm() {
  struct sigaction sa, osa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = &MPingClient::ring;
  sa.sa_flags = SA_INTERRUPT;
  if (sigaction(SIGALRM, &sa, &osa) < 0) {
    LOG(FATAL, "sigaction SIGALRM. %s [%d]", strerror(errno), errno);
  }
}

void MPingClient::InitSigInt() {
  struct sigaction sa, osa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = &MPingClient::halt;
  sa.sa_flags = SA_INTERRUPT;
  if (sigaction(SIGINT, &sa, &osa) < 0) {
    LOG(FATAL, "sigaction SIGINT. %s [%d]", strerror(errno), errno);
  }
}

void MPingClient::Run() {
  if (dest_ips_.empty()) {
    LOG(ERROR, "No target address.");
    return;
  }

  InitSigAlarm();

  InitSigInt();

  for (std::set<std::string>::iterator it = dest_ips_.begin();
       it != dest_ips_.end(); ++it) {
    MPLOG(MPLOG_DEF, "destination IP:%s", it->c_str());

    if (!GoProbing(*it)) {
      MPLOG(MPLOG_DEF, "detination IP %s fails, try next.", it->c_str());
      continue;  // The current destination address is not responding
    } else {
      break;
    }
  }
}

bool MPingClient::GoProbing(const std::string& dst_addr) {
  size_t maxsize;
  start_burst_ = false;  // set true when win_size_ > burst size
  timedout_ = true;
  sseq_ = 0;
  mrseq_ = 0;

  maxsize = std::max(pkt_size_, kMinBuffer);

  scoped_ptr<MpingSocket> mysock(new MpingSocket);

  if (mysock->Initialize(dst_addr, src_addr_, ttl_, 
                         maxsize, win_size_, dport_, client_mode_) < 0) {
    return false;
  }

  // make TCP to server is in C/S mode
  if (client_mode_ > 0) {
    if (!mysock->SendHello(&client_cookie_))
      return false;
  }

  if (!TTLLoop(mysock.get())) {
    return false;
  }

  mp_stat_.PrintStats();

  if (client_mode_ > 0) {
    mysock->SendDone(client_cookie_);
  }

  return true;
}

bool MPingClient::TTLLoop(MpingSocket *sock) {
  ASSERT(sock != NULL);

  int tempttl = 1;

  if (inc_ttl_ == 0) {
    tempttl = ttl_;
    MPLOG(MPLOG_TTL, "ttl:%d", tempttl);
  }

  for (; tempttl <= ttl_; tempttl++) {
    if (haltf_ > 1) 
      break;

    if (ttl_) {
      sock->SetSendTTL(tempttl);
    }

    if (inc_ttl_ > 0)
      MPLOG(MPLOG_TTL, "ttl:%d", tempttl);

    if (!BufferLoop(sock)) {
      MPLOG(MPLOG_TTL, "ttl:%d;error", tempttl);
      return false;
    }

    if (inc_ttl_ > 0)
      MPLOG(MPLOG_TTL, "ttl:%d;done;From_addr:%s", 
            tempttl, sock->GetFromAddress().c_str());

    if (haltf_ == 1)
      haltf_ = 0;
  }

  if (inc_ttl_ == 0)
    MPLOG(MPLOG_TTL, "ttl:%d;done", tempttl-1);

  return true;
}

bool MPingClient::BufferLoop(MpingSocket *sock) {
  ASSERT(sock != NULL);

  if (pkt_size_ > 0)
    MPLOG(MPLOG_BUF, "packet_size:%zu", pkt_size_);

  for (int nbix = 0; ; nbix++) {
    if (haltf_)
      break;

    if (pkt_size_ > 0) {  // set packet size: use static packet size
      cur_packet_size_ = pkt_size_;
      if (nbix != 0) 
        break;
    } else {  // set packet size: increase packet size
      if (loop_size_ == 1) {
        if (kNbTab[nbix] == 0) 
          break;

        cur_packet_size_ = kNbTab[nbix];
      } else if (loop_size_ == 2) {
        if ((nbix+1)*64 > 1500)
          break;

        cur_packet_size_ = (nbix+1)*64;
      } else if (loop_size_ == 3) {
        if ((nbix+1)*128 > 2048)
          break;

        cur_packet_size_ = (nbix+1)*128;
      } else if (loop_size_ == 4) {
        if ((nbix+1)*256 > 4500)
          break;

        cur_packet_size_ = (nbix+1)*256;
      } else {
        LOG(FATAL, "Wrong loop throught message size");
      }
    }  // end of set packet size

    if (loop_size_ > 0)
      MPLOG(MPLOG_BUF, "packet_size:%zu", cur_packet_size_);

    if (!WindowLoop(sock)) {
      MPLOG(MPLOG_BUF, "packet_size:%zu;error", cur_packet_size_);
      return false;
    }

    if (loop_size_ > 0)
      MPLOG(MPLOG_BUF, "packet_size:%zu;done", cur_packet_size_);
  }

  if (pkt_size_ > 0)
    MPLOG(MPLOG_BUF, "packet_size:%zu;done", pkt_size_);
  
  return true;
}

bool MPingClient::WindowLoop(MpingSocket *sock) {
  ASSERT(sock != NULL);
  // third loop: window size
  // no -f flag:        1,2,3,....,win_size_,0,break
  // -f w/ other loops: win_size_,break
  // -f no other loops: win_size_,win_size_,...<interrupt>,0,break
  // 0 is to collect all trailing messages still in transit
  int intran;  // current window size

  if (loop_) 
    MPLOG(MPLOG_WIN, "window_size:%d", win_size_);

  for (intran = loop_?win_size_:1; intran; intran?intran++:0) {
    if (haltf_)
      intran = 0;

    if (intran > win_size_) {
      if (loop_) {
        if (inc_ttl_ > 0 || loop_size_ > 0)
          break;
        else
          intran = win_size_;
      } else {
        intran = 0;
      }
    }

    // printing
    if (!loop_) {
      MPLOG(MPLOG_WIN, "window_size:%d", intran);
    }

    if (!IntervalLoop(intran, sock)) {
      MPLOG(MPLOG_WIN, "window_size:%d;error", intran);
      return false;
    }

    mp_stat_.PrintTempStats();
  }

  if (loop_) 
    MPLOG(MPLOG_WIN, "window_size:%d;done", win_size_);

  return true;
}

int MPingClient::GetNeedSend(int burst, bool start_burst, bool slow_start, 
                int64_t sseq, int64_t mrseq, int intran,
                int mustsend) const{
  int diff;
  int maxopen;
  int need_send;

  if (burst ==  0 || !start_burst) {  // no burst
    maxopen = slow_start?2:10;
    diff = (int)(sseq - mrseq - intran);
    need_send = (diff < 0)?std::min(maxopen, (0-diff)):mustsend;
  } else {  // start burst, now we have built the window
    diff = (int)(sseq - mrseq + burst - intran);
    need_send = (diff > 0)?mustsend:burst;
  }

  return need_send;
}

bool MPingClient::IntervalLoop(int intran, MpingSocket *sock) {
  ASSERT(sock != NULL);

  int mustsend = 0;

  if (intran > 0 && timedout_) {
    mustsend = 1;
    timedout_ = false;
  }
  
  if (!tick_) {  // sync to system clock
    gettimeofday(&now_, 0);
    tick_ = now_.tv_sec;
    while (tick_ >= now_.tv_sec) {
      gettimeofday(&now_, 0);
    }
  }

  alarm(2);  // reset each time called, recv timeout if recv block
  tick_++;

#ifdef MP_TEST
  int out = 0;  
#endif        
  while (tick_ >= now_.tv_sec) {
    int rt;
    int64_t rseq;
    int err;
    bool timeout = false;
    int need_send;

    need_send = GetNeedSend(burst_, start_burst_, slow_start_,
                            sseq_, mrseq_, intran, mustsend);
//    LOG(INFO, "need_send:%d", need_send);
   
    mustsend = 0;

    while (need_send > 0) {
      sseq_++;
      rt = sock->SendPacket(sseq_, client_cookie_, cur_packet_size_, &err);

      if (rt < 0) {  // send fails
        if (err != EINTR) { 
          if (err == ENOBUFS) {
            LOG(ERROR, "send buffer run out.");
            need_send = 0;
            sseq_--;
          } else {
            if (err != ECONNREFUSED) {  // because we connect on UDP sock
              LOG(ERROR, "send fails. %s [%d]", strerror(err), err);
              return false;
            } else {
              sseq_--;
              return false;
            }
          }
        }
        else {
          timeout = true;
          break;
        }
      } else {  // send success, update counters
        gettimeofday(&now_, 0);
        
        mp_stat_.EnqueueSend(sseq_, now_);
#ifdef MP_TEST              
        out++;
#endif              
        
        if (burst_ > 0 && intran >= burst_ && 
            !start_burst_ && (sseq_-mrseq_-intran) == 0) {
          // let the on-flight reach window size, then start burst
          LOG(VERBOSE, "start burst, window %d, burst %d",
              intran, burst_);
          start_burst_ = true;  // once set, stay true
        }
        need_send--;
      }
    }

    if (timeout) {
      LOG(ERROR, "send being interrupted.");
      break;  // almost never happen
    }
    
    // recv
    rseq = sock->ReceiveAndGetSeq(&err, &mp_stat_);
    if (err != 0) {
      //if (err == EINTR)
       // continue;
     // else
      if (err != EINTR) {
        LOG(ERROR, "recv fails. %s [%d]", strerror(err), err);
        return false;
      }
    } else {
      gettimeofday(&now_, 0);

      mp_stat_.EnqueueRecv(rseq, now_);

      if (sseq_ < rseq) {
        LOG(ERROR, "recv a seq larger than sent %" PRId64 ""
                   "%" PRId64 " %" PRId64 "",
            mrseq_, rseq, sseq_);
      } else {
        mrseq_ = rseq;
      }
    }
#ifdef MP_TEST
    if (out >= kMaximumOutPacketsInTest) {
      break;
    }
#endif
    gettimeofday(&now_, 0);
  }  // end of fourth loop: time tick

  return true;
}

MPingClient::MPingClient(size_t pkt_size, int win_size, bool loop, int rate,
                         bool slow_start, int ttl, int inc_ttl, int loop_size,
                         int burst, uint16_t dport, uint16_t client_mode,
                         bool print_seq_time, const std::string& src_addr,
                         const std::string& dst_host) 
    :  pkt_size_(pkt_size),
       win_size_(win_size),
       loop_(loop),
       rate_(rate),
       slow_start_(slow_start),
       ttl_(ttl),
       inc_ttl_(inc_ttl),
       loop_size_(loop_size),
       burst_(burst),
       dport_(dport),
       client_mode_(client_mode),
       client_cookie_(0),
       src_addr_(src_addr),
       dst_host_(dst_host),
       print_seq_time_(print_seq_time),
       mp_stat_(win_size, print_seq_time),
       start_burst_(false),
       sseq_(0),
       mrseq_(0),
       cur_packet_size_(0) {
  mlab::Host dest(dst_host_);
  if (dest.resolved_ips.empty())
    LOG(FATAL, "Destination host %s invalid.", dst_host_.c_str());

  dest_ips_ = dest.resolved_ips;
}
