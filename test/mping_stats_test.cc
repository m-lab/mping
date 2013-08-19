#include "mp_common.h"
#include "mp_log.h"
#include "mp_client.h"
#include "mp_stats.h"
#include "scoped_ptr.h"

#include "gtest/gtest.h"

const int kTestStatsSize = 10;

class MpingStatsTest : public MpingStat {
  public:
    MpingStatsTest() : MpingStat(kTestStatsSize, false) {
    }

    void TestSendRecv(uint64_t* s1, uint64_t* s2,
                      uint64_t* r1, uint64_t* r2) {
      ASSERT(s1 != NULL);
      ASSERT(s2 != NULL);
      ASSERT(r1 != NULL);
      ASSERT(r2 != NULL);

      struct timeval now;
      for (uint64_t i = 1; i < 11; i++) {
        gettimeofday(&now, 0);
        EnqueueSend(i, now);
        gettimeofday(&now, 0);
        EnqueueRecv(i, now);
      }

      *s1 = send_num_;
      *s2 = send_num_temp_;
      *r1 = recv_num_;
      *r2 = recv_num_temp_;
    }

    void TestOutOfOrder(uint64_t* in1, uint64_t* in2) {
      ASSERT(in1 != NULL); 
      ASSERT(in2 != NULL);

      struct timeval now;
      for (uint64_t i = 1; i < 11; i++) {
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

    void TestDuplicate(uint64_t* in1, uint64_t* in2) {
      ASSERT(in1 != NULL); 
      ASSERT(in2 != NULL);

      struct timeval now;
      for (uint64_t i = 1; i < 11; i++) {
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

    void TestLost(uint64_t* in1, uint64_t* in2) {
      ASSERT(in1 != NULL); 
      ASSERT(in2 != NULL);

      struct timeval now;
      for (uint64_t i = 1; i < 51; i++) {  // win_size is now 10, we need > 40
                                           // to lost.
        gettimeofday(&now, 0);
        EnqueueSend(i, now);
      }

      *in1 = lost_num_;
      *in2 = lost_num_temp_;
    }
};

TEST(TestMpingStats, EnqueueSendRecvTest) {
  MpingStatsTest t;

  uint64_t in1, in2, in3, in4;
  t.TestSendRecv(&in1, &in2, &in3, &in4);
  EXPECT_EQ(in1, 10u);
  EXPECT_EQ(in2, 10u);
  EXPECT_EQ(in3, 10u);
  EXPECT_EQ(in4, 10u);
}


TEST(TestMpingStats, EnqueueOutOfOrderTest) {
  MpingStatsTest t;

  uint64_t in1, in2;
  t.TestOutOfOrder(&in1, &in2);
  EXPECT_EQ(in1, 1u);
  EXPECT_EQ(in2, 1u);
}

TEST(TestMpingStats, EnqueueDuplicateTest) {
  MpingStatsTest t;
  uint64_t in1, in2;
  t.TestDuplicate(&in1, &in2);
  EXPECT_EQ(in1, 1u);
  EXPECT_EQ(in2, 1u);
}

TEST(TestMpingStats, EnqueueLostTest) {
  MpingStatsTest t;
  uint64_t in1, in2;
  t.TestLost(&in1, &in2);
  EXPECT_EQ(in1, 10u);
  EXPECT_EQ(in2, 10u);
}
