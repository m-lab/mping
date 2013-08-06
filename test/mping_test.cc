#include "mp_mping.h"
#include "mp_common.h"
#include "mp_socket.h"
#include "mp_stats.h"
#include "scoped_ptr.h"

#include "gtest/gtest.h"

namespace {
  const char *myargv[] = {"mping",
                          "-n", 
                          "5", 
                          "-b", 
                          "64", 
                          "127.0.0.1", NULL}; 
  const int kMyArgc = 6; 
}

class MPingUnitTestBase : public MPing {
  public: 
    MPingUnitTestBase() : MPing(kMyArgc, myargv) { }
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
