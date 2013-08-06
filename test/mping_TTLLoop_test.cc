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

class MPingTTLTest : public MPingUnitTestBase {
  public:
    int loop_count;

    MPingTTLTest() : MPingUnitTestBase() { }
    void GoTest(int test_ttl, int test_inc_ttl) {
      scoped_ptr<MpingSocket> mysock(new MpingSocket);
      EXPECT_GE(mysock->Initialize(std::string("127.0.0.1"), src_addr, ttl,
                             kMinBuffer, win_size, dport, client_mode), 0);

      loop_count = 0;
      ttl = test_ttl;
      inc_ttl = test_inc_ttl;

      TTLLoop(mysock.get());
    }

  protected:
    virtual bool BufferLoop(MpingSocket *sock) {
      loop_count++;
      return true;
    }
};

TEST(MpingUnitTest, TTLLoopTest) {
  MPingTTLTest t;
  t.GoTest(0, 0);
  EXPECT_EQ(t.loop_count, 1);

  t.GoTest(5, 0);
  EXPECT_EQ(t.loop_count, 1);

  t.GoTest(5, 5);
  EXPECT_EQ(t.loop_count, 5);
}

