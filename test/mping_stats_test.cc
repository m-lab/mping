#include "mp_common.h"
#include "mp_log.h"
#include "mp_mping.h"
#include "mp_stats.h"
#include "scoped_ptr.h"

#include "gtest/gtest.h"

const int kTestStatsSize = 10;

class MpingStatsTest : public MpingStat {
  public:
    MpingStatsTest() : MpingStat(kTestStatsSize, false) {
    }

    void TestSend(unsigned int* in1, unsigned int* in2) {
      ASSERT(in1 != NULL); 
      ASSERT(in2 != NULL);

      struct timeval now;
      for (unsigned int i = 1; i < 11; i++) {
        gettimeofday(&now, 0);
        EnqueueSend(i, now);
      }

      *in1 = send_num_;
      *in2 = send_num_temp_;
    }

    void TestRecv(unsigned int* in1, unsigned int* in2) {
      ASSERT(in1 != NULL); 
      ASSERT(in2 != NULL);

      struct timeval now;
      for (unsigned int i = 1; i < 11; i++) {
        gettimeofday(&now, 0);
        EnqueueSend(i, now);
        gettimeofday(&now, 0);
        EnqueueRecv(i, now);
      }

      *in1 = recv_num_;
      *in2 = recv_num_temp_;
    }

    void TestOutOfOrder (unsigned int* in1, unsigned int* in2) {
      ASSERT(in1 != NULL); 
      ASSERT(in2 != NULL);

      struct timeval now;
      for (unsigned int i = 1; i < 11; i++) {
        gettimeofday(&now, 0);
        EnqueueSend(i, now);
      }

      gettimeofday(&now, 0);
      EnqueueRecv(2, now);
      gettimeofday(&now, 0);
      EnqueueRecv(1, now);

      *in1 = out_of_order_;
      *in2 = out_of_order_temp_;
    }

    void TestDuplicate (unsigned int* in1, unsigned int* in2) {
      ASSERT(in1 != NULL); 
      ASSERT(in2 != NULL);

      struct timeval now;
      for (unsigned int i = 1; i < 11; i++) {
        gettimeofday(&now, 0);
        EnqueueSend(i, now);
      }

      gettimeofday(&now, 0);
      EnqueueRecv(1, now);
      gettimeofday(&now, 0);
      EnqueueRecv(1, now);

      *in1 = duplicate_num_;
      *in2 = duplicate_num_temp_;
    }
};

//TEST(TestMpingStats, EnqueueTest) {
//  MpingStatsTest t;
//}
