#include <climits>
#include <iostream>
#include <iomanip>

#include "mlab/mlab.h"
#include "mp_mping.h"
#include "mp_stats.h"
#include "log.h"

namespace {

double time_sub(const struct timeval *new_t, const struct timeval *old_t) {
  struct timeval temp;

  temp.tv_usec = new_t->tv_usec - old_t->tv_usec;
  temp.tv_sec = new_t->tv_sec - old_t -> tv_sec;
  if (temp.tv_usec < 0) {
    --temp.tv_sec;
    temp.tv_usec += 1000000;
  }

  return (temp.tv_sec * 1000.0 + temp.tv_usec / 1000.0);  // in millisec
}

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
  
#ifdef MP_PRINT_TIMELINE
  // timeline
  if (seq > INT_MAX) {
    LOG(mlab::FATAL, "send sequence number is too large.");
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
#ifdef MP_PRINT_TIMELINE
  if (seq > INT_MAX) {
    LOG(mlab::FATAL, "recv sequence number is too large.");
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
  std::cout << "Sent " << send_num_temp_ << " received " << 
               recv_unique_num_temp_ <<
               " total received " << recv_num_temp_ << " out-of-order " << 
               out_of_order_temp_ << " lost " << lost_num_temp_ << " dup " <<
               duplicate_num_temp_ << " unexpected " << unexpect_num_temp_ << 
               std::endl;

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

  std::cout << "Total sent=" << send_num_ << " received=" << recv_unique_num_ <<
               " Total received=" << recv_num_ << " total out-of-order=" <<
               out_of_order_ << " total lost=" << lost_num_ << "(" <<
               lost_num_ * 100.0 / send_num_ << ")" << " total dup=" << 
               duplicate_num_ << " total unexpected=" << unexpect_num_ << 
               std::endl;
}
