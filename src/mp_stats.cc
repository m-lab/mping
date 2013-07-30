#include <climits>
#include <iostream>
#include <iomanip>

#include "mlab/mlab.h"
#include "mp_mping.h"
#include "mp_stats.h"
#include "mp_log.h"

namespace {

unsigned long time_sub(const struct timeval& new_t, 
                       const struct timeval& old_t) {
  struct timeval temp;

  temp.tv_usec = new_t.tv_usec - old_t.tv_usec;
  temp.tv_sec = new_t.tv_sec - old_t.tv_sec;
  if (temp.tv_usec < 0) {
    --temp.tv_sec;
    temp.tv_usec += 1000000;
  }

  return (temp.tv_sec * 1000000 + temp.tv_usec);  // in usec
}

const int kDefaultVectorLength = 20000;
const int kDefaultRunTime = 3600;

}  // namespace

void MpingStat::EnqueueSend(unsigned int seq, 
                            struct timeval time) {
  SendQueueNode TempNode;
  TempNode.seq = seq;
  TempNode.send_time = time;
  TempNode.recv_time.tv_sec = 0;
  TempNode.recv_time.tv_usec = 0;

  int idx = (seq-1) % send_queue_size_;  // seq 1 is stored at send_queue[0]
  send_num_++;
  send_num_temp_++;

  if (seq <= send_queue_size_) {  // no record in queue
    send_queue.push_back(TempNode);
  } else {
    if (send_queue.at(idx).recv_time.tv_sec == 0) {  // previous one recved
      lost_num_++;
      lost_num_temp_++;
    } 

    send_queue.at(idx) = TempNode;
  }
  
#ifdef MP_TEST
  // timeline
  if (seq > INT_MAX) {
    LOG(FATAL, "send sequence number is too large.");
  } else {
    timeline.push_back((int)seq);
  }
#endif
}

void MpingStat::EnqueueRecv(unsigned int seq, 
                            struct timeval time) {
  int idx = (seq-1) % send_queue_size_;

  if (send_queue.at(idx).seq == seq) {
    if (send_queue.at(idx).recv_time.tv_sec == 0) {
      recv_unique_num_++;
      recv_unique_num_temp_++;
      send_queue.at(idx).recv_time = time;  // only record first recv time
    } else {  // dup packet
      duplicate_num_++;
      duplicate_num_temp_++;
    }

    if (seq < max_recv_seq_) {
      out_of_order_++;
      out_of_order_temp_++;
    } else {
      max_recv_seq_ = seq;
    }
  } else if (seq > send_queue.at(idx).seq) {
    unexpect_num_++;
    unexpect_num_temp_++;
  }

  recv_num_++;
  recv_num_temp_++;

  // timeline
#ifdef MP_TEST
  if (seq > INT_MAX) {
    LOG(FATAL, "recv sequence number is too large.");
  } else {
    int s = (int)seq;
    timeline.push_back(0 - s);
  }
#endif
}

void MpingStat::LogUnexpected() {
  unexpect_num_++;
  unexpect_num_temp_++;
}

void MpingStat::PrintTimeLine() const {
  unsigned int idx = 0;
  while (idx<timeline.size()) {
    if (timeline.at(idx) > 0)
      std::cout << std::setw(5) << timeline.at(idx) 
                << " --> " << std::endl;
    else
      std::cout << "      <-- " << std::setw(5) << 
                   timeline.at(idx) << std::endl;

    idx++;
  }
}

void MpingStat::PrintTempStats() {
  MPLOG(MPLOG_WIN, "sent:%u;received:%u;total received:%u;out-of-order:%u;"
                   "lost:%u;dup:%u;unexpected:%u", send_num_temp_,
                   recv_unique_num_temp_, recv_num_temp_, out_of_order_temp_,
                   lost_num_temp_, duplicate_num_temp_, unexpect_num_temp_);

  send_num_temp_ = 0;
  recv_num_temp_ = 0;
  recv_unique_num_temp_ = 0;
  out_of_order_temp_ = 0;
  lost_num_temp_ = 0;
  duplicate_num_temp_ = 0;
  unexpect_num_temp_ = 0;
}

void MpingStat::PrintStats() {
  // check last round losts
  for (unsigned int i = 0; i < send_queue_size_; i++) {
    if (send_queue.at(i).recv_time.tv_sec == 0) {
      lost_num_++;
      lost_num_temp_++;
    }
  }

  MPLOG(MPLOG_DEF, "total_sent:%u;received:%u;total_received:%u;"
                   "total_out-of-order:%u;total_lost:%u:%f%%;total_dup:%u;"
                   "total_unexpected:%u", send_num_, recv_unique_num_,
                   recv_num_, out_of_order_, lost_num_, 
                   lost_num_ * 100.0 / send_num_, duplicate_num_,
                   unexpect_num_);
}

void MpingStat::ReserveTimeSeqVectors() {
  time_of_packets.reserve(sizeof(unsigned long) * kDefaultVectorLength);
  seq_of_packets.reserve(sizeof(long) * kDefaultVectorLength);
  interval_boundry.reserve(sizeof(unsigned long) * kDefaultRunTime);
}

void MpingStat::InsertSequenceTime(int seq, const struct timeval& now) {
  if (!started) {
    started = true;
    start_time = now;
    time_of_packets.push_back(0);
    seq_of_packets.push_back((long)seq);
  }
  
  if (started) {
    time_of_packets.push_back(time_sub(now, start_time));
    seq_of_packets.push_back((long)seq);
  }
}

void MpingStat::InsertIntervalBoundry(const struct timeval& now) {
  interval_boundry.push_back(time_sub(now, start_time));
}

void MpingStat::PrintResearch() const {
  for (unsigned int i = 0; i < time_of_packets.size(); i++) {
    MPLOG(MPLOG_DEF, "[Research seq-time];%ld;%lu", 
          seq_of_packets.at(i), time_of_packets.at(i));
  }

  for (unsigned int j = 0; j < interval_boundry.size(); j++) {
    MPLOG(MPLOG_DEF, "[Research interval-switch];%lu", interval_boundry.at(j));
  }
}
