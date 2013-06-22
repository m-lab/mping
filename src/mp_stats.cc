#include <climits>
#include "mlab/mlab.h"
#include "mp_stats.h"
#include "log.h"

void MpingStat::EnqueueSend(const unsigned int& seq, const struct timeval& time,
                            const unsigned int& numpkt, size_t size) {
  SendQueueNode TempNode;
  TempNode.seq = seq;
  TempNode.send_time = time;
  TempNode.recv_time.tv_sec = 0;
  TempNode.recv_time.tv_usec = 0;
  TempNode.num_pkt_in_net = numpkt;
  TempNode.size = size;

  send_queue.push_back(TempNode); 

  // timeline
  if (seq > INT_MAX) {
    LOG(mlab::FATAL, "send sequence number is too large.");
  } else {
    timeline.push_back((int)seq);
  }
}

void MpingStat::EnqueueRecv(const unsigned int& seq, 
                            const struct timeval& time) {
  recv_queue.push_back(seq);

  send_queue.at(seq-1).recv_time = time;

  // timeline
  if (seq > INT_MAX) {
    LOG(mlab::FATAL, "recv sequence number is too large.");
  } else {
    int s = (int)seq;
    timeline.push_back(0 - s);
  }
}
