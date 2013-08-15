#include "mp_client.h"
#include "mp_common.h"
#include "mp_socket.h"
#include "mp_stats.h"
#include "scoped_ptr.h"

#include "gtest/gtest.h"

class MPingUnitTestBase : public MPingClient {
  public: 
    MPingUnitTestBase() : MPingClient(64,     // packet size -b
                                      5,      // window size -n
                                      false,  // loop forever, -f
                                      false,  // slowstart, -S
                                      0,      // ttl, -t
                                      0,      // ttlmax, -a
                                      0,      // loop through size, -l
                                      0,      // burst size, -B
                                      0,      // destination port, -p
                                      0,      // use UDP server, -c
                                      false,  // print time sequence, -r
                                      std::string(""),  // source address, -F
                                      std::string("127.0.0.1")) { } 
};

TEST(MpingUnitTest, GetNeedSendTest) {
  MPingUnitTestBase t;
  EXPECT_EQ(t.GetNeedSend(0, false, false, 0, 0, 10, 0), 10);
  EXPECT_EQ(t.GetNeedSend(0, false, false, 11, 1, 10, 1), 1);
  EXPECT_EQ(t.GetNeedSend(0, false, true, 0, 0, 10, 0), 2);
  EXPECT_EQ(t.GetNeedSend(5, true, false, 15, 8, 10, 0), 0);
  EXPECT_EQ(t.GetNeedSend(5, true, false, 15, 10, 10, 0), 5);
  EXPECT_EQ(t.GetNeedSend(5, false, false, 15, 8, 10, 0), 3);
}
