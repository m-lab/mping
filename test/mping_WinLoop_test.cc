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

class MPingWinLoopTest: public MPingUnitTestBase {
  public:
    int loop_forever_count;
    std::vector<int> win_array;

    MPingWinLoopTest() : MPingUnitTestBase() { }
    void GoTest(bool isloop, int test_inc_ttl, int test_loop_size) {
      MpingSocket tempmpsock;

      loop = isloop;
      inc_ttl = test_inc_ttl;
      loop_size = test_loop_size;

      win_array.clear();
      loop_forever_count = 0;
      haltf = 0;
      WindowLoop(&tempmpsock);
    }

  protected:
    virtual bool IntervalLoop(int intran, MpingSocket *sock) {
      if (intran == 0)
        return true;

      if (loop && inc_ttl == 0 && loop_size == 0) {
        if (loop_forever_count == 2)
          haltf = 1;
        else
          loop_forever_count++;
      }
      win_array.push_back(intran);
      return true;
    }
};

TEST(MpingUnitTest, WinLoopTest) {
  MPingWinLoopTest t;

  t.GoTest(true, 5, 0);
  EXPECT_EQ(t.win_array.size(), 1u);
  EXPECT_EQ(t.win_array.at(0), 5);

  t.GoTest(true, 0, -2);
  EXPECT_EQ(t.win_array.size(), 1u);
  EXPECT_EQ(t.win_array.at(0), 5);

  t.GoTest(true, 0, 0);
  EXPECT_EQ(t.win_array.size(), 3u);  // loop_forever_count 0, 1, 2,
  EXPECT_EQ(t.win_array.at(0), 5);

  t.GoTest(false, 0, 0);
  EXPECT_EQ(t.win_array.size(), 5u);
  EXPECT_EQ(t.win_array.at(0), 1);
  EXPECT_EQ(t.win_array.at(4), 5);
}
