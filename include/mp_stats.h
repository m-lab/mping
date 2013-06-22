#ifndef _MPING_STATS_H_
#define _MPING_STATS_H_

#include <vector>
#include <time.h>

struct SendQueueNode {
  unsigned int seq;
  struct timeval send_time;
  struct timeval recv_time;
  unsigned int num_pkt_in_net;  // number of packet in flight when sent
  size_t size;
};

class MpingStat {
  public:
    void EnqueueSend(const unsigned int& seq, const struct timeval& time,
                     const unsigned int& numpkt, size_t size);
    void EnqueueRecv(const unsigned int& seq, const struct timeval& time); 

  protected:
    std::vector<struct SendQueueNode> send_queue;
    std::vector<unsigned int> recv_queue;
    std::vector<int> timeline;
};

#endif
