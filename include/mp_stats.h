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

struct SendQueueNode {
  unsigned int seq;
  struct timeval send_time;
  struct timeval recv_time;
//  unsigned int num_pkt_in_net;  // number of packet in flight when sent
};

class MpingStat {
  public:
    MpingStat() : 
      unexpect_num_(0),
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
      send_queue_size_(0),
      started(false) { 
    }

    void SetWindowSize(int win_size) {
      window_size_ = win_size;
      send_queue_size_ = 4 * win_size;
      send_queue.reserve(sizeof(SendQueueNode) * 4 * window_size_);
    }

    void EnqueueSend(unsigned int seq, struct timeval time);
    void EnqueueRecv(unsigned int seq, struct timeval time); 
    void LogUnexpected();

    void PrintStats();
    void PrintTempStats();
    void PrintTimeLine() const;

    std::vector<int> timeline;
    std::vector<unsigned long> time_of_packets;  // relative time to start_time
    std::vector<long> seq_of_packets;

    void ReserveTimeSeqVectors();
    void InsertSequenceTime(int seq, const struct timeval& now);
    void InsertIntervalBoundry(const struct timeval& now);
    void PrintResearch() const;
  protected:
    unsigned int unexpect_num_;
    unsigned int unexpect_num_temp_;
    unsigned int max_recv_seq_;
    unsigned int out_of_order_;
    unsigned int out_of_order_temp_;
    unsigned int recv_num_;
    unsigned int recv_num_temp_;
    unsigned int recv_unique_num_;
    unsigned int recv_unique_num_temp_;
    unsigned int send_num_;
    unsigned int send_num_temp_;
    unsigned int duplicate_num_;
    unsigned int duplicate_num_temp_;
    unsigned int lost_num_;
    unsigned int lost_num_temp_;
    int window_size_;
    unsigned int send_queue_size_;
    std::vector<struct SendQueueNode> send_queue;

  private:
    bool started;
    struct timeval start_time;

    std::vector<unsigned long> interval_boundry;
};

#endif
