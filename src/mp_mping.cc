#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>
#include <set>

#include "mp_mping.h"
#include "mp_socket.h"
#include "mp_stats.h"
#include "log.h"
#include "scoped_ptr.h"
#include "mlab/socket_family.h"
#include "mlab/host.h"
#include "mlab/mlab.h"
#include "mlab/protocol_header.h"
#include "mlab/server_socket.h"

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
      -b -<sel>   Loop through message sizes: -1:selected sizes\n\
                  or steps of: -2:64 -3:128 -4:256\n\
      -B <bnum>   Send <bnum> packets in burst, should smaller than <num>\n\
      -p <port>   If UDP, destination port number\n\
\n\
      -s <sport>  Server mode, liten on UDP <sport>\n\
      -4          Server mode, use IPv4\n\
      -6          Server mode, use IPv6\n\
      -c          Client mode, sending with UDP to a server running -s\n\
\n\
      -V, -d  Version, Debug (verbose)\n\
\n\
      -F <addr>   Select a source interface\n\
      <host>     Target host\n";

const size_t kMaxBuffer = 9000;  // > 2 FDDI?
const int kNbTab[] = {64, 100, 500, 1000, 1500, 2000, 3000, 4000, 0};
const char *kVersion = "mping version: 2.0 (2013.06)";
const int kDefaultTTL = 255;

int haltf;
int tick;
bool timedout;

void ring(int signo) {
  struct sigaction sa, osa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = ring;
  sa.sa_flags = SA_INTERRUPT;
  if (sigaction(SIGALRM, &sa, &osa) < 0) {
    LOG(mlab::FATAL, "sigaction SIGALRM. %s [%d]", strerror(errno), errno);
  }
  timedout = true;
  tick = 0;
}

void halt(int signo) {
  struct sigaction sa, osa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_INTERRUPT;
  if (++haltf >= 2) {
    sa.sa_handler = 0;
    if (sigaction(SIGINT, &sa, &osa)) {
      LOG(mlab::FATAL, "sigaction SIGINT. %s [%d]", strerror(errno), errno);
    }
  } else {
    sa.sa_handler = halt;
    if (sigaction(SIGINT, &sa, &osa) < 0) {
      LOG(mlab::FATAL, "sigaction SIGINT. %s [%d]", strerror(errno), errno);
    }
  }
}

}  // namespace

void MPing::Run() {
  if (dest_ips.empty()) {
    LOG(mlab::ERROR, "No target address.");
    return;
  }

  struct sigaction sa, osa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = ring;
  sa.sa_flags = SA_INTERRUPT;
  if (sigaction(SIGALRM, &sa, &osa) < 0) {
    LOG(mlab::FATAL, "sigaction SIGALRM. %s [%d]", strerror(errno), errno);
  }

  sa.sa_handler = halt;
  if (sigaction(SIGINT, &sa, &osa) < 0) {
    LOG(mlab::FATAL, "sigaction SIGINT. %s [%d]", strerror(errno), errno);
  }
  haltf = 0;

  for (std::set<std::string>::iterator it = dest_ips.begin();
       it != dest_ips.end(); ++it) {
    LOG(mlab::INFO, "destination IP: %s", it->c_str());

    if (GoProbing(*it) < 0) {
      LOG(mlab::INFO, "detination IP %s fails, try next.", it->c_str());
      continue;  // The current destination address is not responding
    } else {
      break;
    }
  }
}

void MPing::RunServer() {
  bool have_data = false;
  size_t packet_size = std::max(kMaxBuffer, pkt_size);
  const char *PayloadHeader = "mlab-seq#";
  const int PayloadHeaderLength = 9;

  int unexpected = 0;
  int seq_recv = 0;
  int sent_back = 0;
  int total_recv = 0;
  int out_of_order = 0;
  unsigned int mrseq = 0;  // sequence number starts from 1.

  LOG(mlab::INFO, "Running server mode, port %u.", server_port);

  // create server socket
  scoped_ptr<mlab::ServerSocket> 
      mysock(mlab::ServerSocket::CreateOrDie(server_port, SOCKETTYPE_UDP, 
                                             server_family));

  // check socket buffer size
  if (mysock->GetRecvBufferSize() < packet_size) {
    LOG(mlab::INFO, "Receive buffer size is smaller than needed, "
                    "set to needed.");
    mysock->SetRecvBufferSize(packet_size);
  }

  if (mysock->GetSendBufferSize() < packet_size) {
    LOG(mlab::INFO, "Send buffer size is smaller than needed, set to needed.");
    mysock->SetSendBufferSize(packet_size);
  }

  // default time out is 5 sec, use it
  while (1) {
    errno = 0;
    mlab::Packet recv_packet = mysock->ReceiveWithError(packet_size);
    if (errno != 0) {
      if (errno == EAGAIN) {
        if (have_data) {
          // print stats
          std::cout << "Total received=" << total_recv << " seq received=" <<
                       seq_recv << " send back=" << sent_back << 
                       " total out-of-order=" << out_of_order <<
                       " total unexpected=" << unexpected << std::endl;
          have_data = false;
        } else {
          continue;
        }
      } else {
        LOG(mlab::FATAL, "Receive fails! %s [%d].", strerror(errno), errno);
      }
    }

    total_recv++;

    if (recv_packet.length() < (PayloadHeaderLength + sizeof(unsigned int))) {
      LOG(mlab::VERBOSE, "recv a packet smaller than min size.");
      unexpected++;
      continue;
    }

    std::string head(recv_packet.buffer(), PayloadHeaderLength);
    if (head.compare(PayloadHeader) != 0){
      LOG(mlab::VERBOSE, "recv a packet not for this program.");
      unexpected++;
      continue;
    }

    have_data = true;
    seq_recv++;
    const unsigned int *seq = 
        reinterpret_cast<const unsigned int*>(recv_packet.buffer() +
                                              PayloadHeaderLength);

    unsigned int rseq = ntohl(*seq);
    if (mrseq > rseq) {
      out_of_order++;
    } else {
      mrseq = rseq;
    }

    mysock->Send(recv_packet);
    sent_back++;
  }
}

bool MPing::IsServerMode() const {
  return (server_port > 0);
}

int MPing::GoProbing(const std::string& dst_addr) {
  size_t maxsize;
  size_t packet_size;  // current packet size, include IP header length
  unsigned int sseq = 0;  // send sequence
  unsigned int mrseq = 0;  // recv sequence
  bool start_burst = false;  // set true when win_size > burst size
  timedout = true;

  maxsize = std::max(pkt_size, kMaxBuffer);

  scoped_ptr<MpingSocket> mysock(new MpingSocket);

  if (mysock->Initialize(dst_addr, src_addr, ttl, 
                         maxsize, win_size, dport, client_mode) < 0) {
    return -1;
  }

  scoped_ptr<MpingStat> mystat(new MpingStat(win_size));

  int tempttl = 1;
  if (inc_ttl == 0)
    tempttl = ttl;

  // first loop: ttl
  for (; tempttl <= ttl; tempttl++) {
    if (haltf > 1) break;

    if (ttl) {
      mysock->SetSendTTL(tempttl);
    }

    // second loop: buffer size
    int nbix = 0;
    for (nbix = 0; ; nbix++) {
      if (haltf) 
        break;

      if (pkt_size > 0) {  // set packet size: use static packet size
        packet_size = pkt_size;
        if (nbix != 0) 
          break;
      } else {  // set packet size: increase packet size
        if (loop_size == -1) {
          if (kNbTab[nbix] == 0) 
            break;

          packet_size = kNbTab[nbix];
        } else if (loop_size == -2) {
          if ((nbix+1)*64 > 1500)
            break;

          packet_size = (nbix+1)*64;
        } else if (loop_size == -3) {
          if ((nbix+1)*128 > 2048)
            break;

          packet_size = (nbix+1)*128;
        } else if (loop_size == -4) {
          if ((nbix+1)*256 > 4500)
            break;

          packet_size = (nbix+1)*256;
        } else {
          LOG(mlab::FATAL, "Wrong loop throught message size %d.\n%s",
              loop_size, usage);
        }
      }  // end of set packet size

      // third loop: window size
      // no -f flag:        1,2,3,....,win_size,0,break
      // -f w/ other loops: win_size,break
      // -f no other loops: win_size,win_size,...<interrupt>,0,break
      // 0 is to collect all trailing messages still in transit
      uint16_t intran;  // current window size
      for (intran = loop?win_size:1; intran; intran?intran++:0) {
        int mustsend;
        struct timeval now;

        if (haltf)
          intran = 0;

        if (intran > win_size) {
          if (loop) {
            if (inc_ttl > 0 || loop_size < 0)
              break;
            else
              intran = win_size;
          } else {
            intran = 0;
          }
        }

        if (intran > 0 && timedout) {
          mustsend = 1;
          timedout = false;
        }

        // printing
        if (!loop || inc_ttl > 0 || loop_size < 0) {
          if (ttl > 0) {
            LOG(mlab::INFO, "ttl %d, packet size %lu, window size %d",
                tempttl, packet_size, intran);
          } else {
            LOG(mlab::INFO, "packet size %lu, window size %d", 
                packet_size, intran);
          }
        }

       // if (tick == 0) {
       //   LOG(mlab::INFO, "tick is 0 now.");
       // }
        if (!tick) {  // sync to system clock
          gettimeofday(&now, 0);
          tick = now.tv_sec;
          while (tick >= now.tv_sec) {
            gettimeofday(&now, 0);
          }
        }

        alarm(2);  // reset each time called, recv timeout if recv block
        
        // fourth loop: with in 1 sec
        tick++;

#ifdef MP_PRINT_TIMELINE
        int out = 0;  // for debug
#endif        
        while (tick >= now.tv_sec) {
          int maxopen;
          int rt;
          unsigned int rseq;
          int err;
          bool timeout = false;
          int diff, need_send;

          // send
          if (burst ==  0 || !start_burst) {  // no burst
            maxopen = slow_start?2:10;
            diff = (int)(sseq - mrseq - intran);
            need_send = (diff < 0)?std::min(maxopen, (0-diff)):mustsend;
          } else {  // start burst, now we have built the window
            diff = (int)(sseq - mrseq + burst - intran);
            need_send = (diff > 0)?mustsend:burst;
          }

          mustsend = 0;

          while (need_send > 0) {
            sseq++;
            rt = mysock->SendPacket(sseq, packet_size, &err);

            if (rt < 0) {  // send fails
              if (err != EINTR) { 
                if (err == ENOBUFS) {
                  LOG(mlab::INFO, "send buffer run out.");
                  maxopen = 0;
                  sseq--;
                } else {
                  if (err != ECONNREFUSED) {  // because we connect on UDP sock
                    LOG(mlab::FATAL, "send fails. %s [%d]", strerror(err), err);
                  } else {
                    sseq--;
                  }
                }
              }
              else {
                timeout = true;
                break;
              }
            } else {  // send success, update counters
              gettimeofday(&now, 0);
              mystat->EnqueueSend(sseq, now);
#ifdef MP_PRINT_TIMELINE              
              out++;
#endif              
              
              if (burst > 0 && intran >= burst && 
                  !start_burst && (sseq-mrseq-intran) == 0) {
                // let the on-flight reach window size, then start burst
                LOG(mlab::INFO, "start burst, window %d, burst %d",
                    intran, burst);
                start_burst = true;  // once set, stay true
              }
              need_send--;
            }
          }

          if (timeout) {
            LOG(mlab::INFO, "send being interrupted.");
            break;  // almost never happen
          }
          
          // recv
          rseq = mysock->ReceiveAndGetSeq(&err, mystat.get());
          if (err != 0) {
            //if (err == EINTR)
             // continue;
           // else
            if (err != EINTR)
              LOG(mlab::FATAL, "recv fails. %s [%d]", strerror(err), err);
          } else {
            gettimeofday(&now, 0);
            mystat->EnqueueRecv(rseq, now);

            if ((int)(sseq - rseq) < 0) {
              LOG(mlab::ERROR, "recv a seq larger than sent %d %d %d",
                  mrseq, rseq, sseq);
            } else {
              mrseq = rseq;
            }
          }
#ifdef MP_PRINT_TIMELINE
          if (out >= 50) {
            break;
          }
#endif
          gettimeofday(&now, 0);
        }  // end of fourth loop: time tick

        mystat->PrintTempStats();
      }  // end of third loop: window size
    }  // end of second loop: buffer size

    if (inc_ttl > 0) {
      LOG(mlab::INFO, "From %s", mysock->GetFromAddress().c_str());
    }

    if (haltf == 1) haltf = 0;
  }  // end of first loop: ttl

  mystat->PrintStats();

#ifdef MP_PRINT_TIMELINE
  mystat->PrintTimeLine();
#endif

  return 0;
}

MPing::MPing(const int& argc, const char **argv) :
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
      server_port(0),
      server_family(SOCKETFAMILY_UNSPEC),
      client_mode(false) {
  int ac = argc;
  const char **av = argv;
  const char *p;
  bool host_set = false;

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
          case 'c': client_mode = true; av--; break;
          case '4': server_family = SOCKETFAMILY_IPV4; av--; break;
          case '6': server_family = SOCKETFAMILY_IPV6; av--; break;
          default: LOG(mlab::FATAL, "\n%s", usage); break;
        }
      } else {
        switch (p[1]) {
          case 'n': { win_size = atoi(*av); ac--; break; }
          case 'f': { loop = true; av--; break; }
          case 'R': { rate = atoi(*av); ac--; break; }
          case 'S': { slow_start = true; av--; break; }
          case 't': { ttl = atoi(*av); ac--; break; }
          case 's': { server_port = atoi(*av); ac--; break; }
          case 'a': { inc_ttl = atoi(*av); ttl = inc_ttl; ac--; break; }
          case 'b': {
            p = *av;
            if (p[0] == '-') {
              loop_size = atoi(*av);  
            } else {
              pkt_size = atoi(*av);
            }

            ac--;
            break; 
          }
          case 'p': { dport = atoi(*av); ac--; break; }
          case 'B': { burst = atoi(*av); ac--; break; }
          case 'V': { version = true; av--; break; }
          case 'd': { debug = true; av--; break; }
          case 'c': { client_mode = true; av--; break; };
          case '4': { server_family = SOCKETFAMILY_IPV4; av--; break; }
          case '6': { server_family = SOCKETFAMILY_IPV6; av--; break; }
          case 'F': { src_addr = std::string(*av); ac--; break; }
          default: { 
            LOG(mlab::FATAL, "Unknown parameter -%c\n%s", p[1], usage); break; 
          }
        }
      }
    } else {  // host
      if (!host_set) {
        dst_host = std::string(p);  
        av--;
        host_set = 1;
      }else{
        LOG(mlab::FATAL, "\n%s", usage);
      }
    }

    ac--;
    av++;
  }

  // must have set host
  // if (!host_set) {
  //   LOG(mlab::FATAL, "Must have destination host. \n%s", usage);
  // }

  ValidatePara();
}

void MPing::ValidatePara() {

  // print version
  if (version) {
    std::cout << kVersion << std::endl;
    exit(0);
  }

  // server mode
  if (server_port > 0) {
    if (server_port > 65535) {
      LOG(mlab::FATAL, "Server port cannot larger than 65535.");
    }
    
    if (server_family == SOCKETFAMILY_UNSPEC){
      LOG(mlab::FATAL, "Need to know the socket family, use -4 or -6.");
    }

    return;
  }

  if (debug) {
    mlab::SetLogSeverity(mlab::VERBOSE);
  }

  // destination set?
  if (dst_host.empty()) {
    LOG(mlab::FATAL, "Must have destination host. \n%s", usage);
  }

  // client mode
  if (client_mode) {
    if (dport == 0) {
      LOG(mlab::FATAL, "Client mode must have destination port using -p.");
    }

    if (ttl == 0) {
      ttl = kDefaultTTL;
    }
  }
  
  // TTL
  if (ttl > 255 || ttl < 0) {
    ttl = 255;
    LOG(mlab::WARNING, "TTL %d is either > 255 or < 0, " 
                       "now set TTL to 255.", ttl);
  } 

  // loop_size [-4, -1]
  if (loop_size < -4 || loop_size > 0) {
    LOG(mlab::FATAL, "loop through message size could only take"
                     "-1, -2, -3 or -4");
  }

  // inc_ttl
  if (inc_ttl > 255 || inc_ttl < 0) {
    inc_ttl = 255;
    LOG(mlab::WARNING, "Auto-increment TTL %u is either > 255 or < 0, "
                       "now set auto-increment TTL to 255.", inc_ttl);
  }

  // destination host
  mlab::Host dest(dst_host);
  if (dest.resolved_ips.empty()) {
    LOG(mlab::FATAL, "Destination host %s invalid.", dst_host.c_str());
  } else {  // set destination ip set
    dest_ips = dest.resolved_ips;
  }

  // max packet size
  if (pkt_size > 65535) {
    LOG(mlab::FATAL, "Packet size cannot larger than 65535.");
  }

  // validate local host
  if (src_addr.length() > 0) {
    if (mlab::RawSocket::GetAddrFamily(src_addr) == SOCKETFAMILY_UNSPEC) {
      LOG(mlab::FATAL, "Local host %s invalid. Only accept numeric IP address",
          src_addr.c_str());
    }
  } 

  // validate UDP destination port
  if (dport > 0 ) {
    if (ttl == 0 && !client_mode) {
      LOG(mlab::FATAL, "-p can only use together with -t -a or -p.\n%s", usage);
    }

    if (dport > 65535) {
      LOG(mlab::FATAL, "UDP destination port cannot larger than 65535.");
    }
  }
}

