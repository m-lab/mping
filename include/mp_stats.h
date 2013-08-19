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

#ifndef _MPING_STATS_H_
#define _MPING_STATS_H_

#include <vector>

#include <sys/time.h>

#include "mp_common.h"

struct SendQueueNode {
  int64_t seq;
  struct timeval send_time;
  struct timeval recv_time;
//  unsigned int num_pkt_in_net;  // number of packet in flight when sent
};

class MpingStat {
  public:
    MpingStat(int win_size, bool print_sequence_time)
      : unexpect_num_(0),
        unexpect_num_temp_(0),
        max_recv_seq_(1),
        out_of_order_(0),
        out_of_order_temp_(0),
        recv_num_(0),
        recv_num_temp_(0),
        recv_unique_num_(0),
        recv_unique_num_temp_(0),
        send_num_(0),
        send_num_temp_(0),
        duplicate_num_(0),
        duplicate_num_temp_(0),
        lost_num_(0),
        lost_num_temp_(0),
        window_size_(0),
        send_queue_size_(4 * win_size), // Delay > 4 RTT means packet drop.
        started_(false),
        print_seq_time_(print_sequence_time) { 
      send_queue_.reserve(send_queue_size_);

      if (print_seq_time_) {
        ReserveTimeSeqVectors();
      }
    }

    void EnqueueSend(int64_t seq, struct timeval time);
    void EnqueueRecv(int64_t seq, struct timeval time); 
    void LogUnexpected();

    void PrintStats();
    void PrintTempStats();
    void PrintTimeLine() const;
    void PrintResearch() const;

    std::vector<int64_t> timeline_;
    std::vector<uint64_t> time_of_packets_;  // relative time 
                                            // to start_time in usec
    std::vector<int64_t> seq_of_packets_;
    
  protected:
    int64_t unexpect_num_;
    int64_t unexpect_num_temp_;
    int64_t max_recv_seq_;
    int64_t out_of_order_;
    int64_t out_of_order_temp_;
    int64_t recv_num_;
    int64_t recv_num_temp_;
    int64_t recv_unique_num_;
    int64_t recv_unique_num_temp_;
    int64_t send_num_;
    int64_t send_num_temp_;
    int64_t duplicate_num_;
    int64_t duplicate_num_temp_;
    int64_t lost_num_;
    int64_t lost_num_temp_;
    int window_size_;
    int send_queue_size_;
    std::vector<struct SendQueueNode> send_queue_;

  private:
    void ReserveTimeSeqVectors();
    void InsertSequenceTime(int64_t seq, const struct timeval& now);
    void InsertIntervalBoundary(const struct timeval& now);

    bool started_;
    bool print_seq_time_;
    struct timeval start_time_;
    std::vector<uint64_t> interval_boundary_;
};

#endif
