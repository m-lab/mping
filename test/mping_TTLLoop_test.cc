#include "mp_client.h"
#include "mp_common.h"
#include "mp_socket.h"
#include "mp_stats.h"
#include "scoped_ptr.h"

#include "gtest/gtest.h"

class MPingTTLTest : public MPingClient {
  public:
    int loop_count;

    MPingTTLTest() : MPingClient(64, 5, false, false, 0, 0, 0, 0, 0, 0,
                                 false, std::string(""), 
                                 std::string("127.0.0.1")) { }

    void GoTest(int test_ttl, int test_inc_ttl) {
      scoped_ptr<MpingSocket> mysock(new MpingSocket);
      EXPECT_GE(mysock->Initialize(std::string("127.0.0.1"), src_addr_, ttl_,
                                   kMinBuffer, win_size_, dport_, 
                                   use_server_), 0);

      loop_count = 0;
      ttl_ = test_ttl;
      inc_ttl_ = test_inc_ttl;

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

