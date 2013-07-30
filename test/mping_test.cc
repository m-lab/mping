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
  protected:
    MPingUnitTestBase() : MPing(kMyArgc, myargv) { }
};

class MPingRunClientTest : public MPingUnitTestBase {
  public:
    MPingRunClientTest() : MPingUnitTestBase() { }
    std::vector<std::string> ips;
    void GoTest() {
      std::string strset[] = {"192.168.0.1",
                              "172.16.0.1",
                              "10.0.0.1"};
      std::set<std::string> tempset(strset, strset+3);
      dest_ips = tempset;

      RunClient();
    }

  protected:
    virtual bool GoProbing(const std::string& dst_addr) {
      ips.push_back(dst_addr);
      return false;
    }

};

TEST(MpingUnitTest, RunClientTest) {
  MPingRunClientTest t;
  t.GoTest();
  EXPECT_EQ(t.ips.size(), 3u);
  EXPECT_STREQ(t.ips.at(0).c_str(), "10.0.0.1");
  EXPECT_STREQ(t.ips.at(1).c_str(), "172.16.0.1");
  EXPECT_STREQ(t.ips.at(2).c_str(), "192.168.0.1");
}

class MPingTTLTest : public MPingUnitTestBase {
  public:
    int loop_count;

    MPingTTLTest() : MPingUnitTestBase() { }
    void GoTest(int test_ttl, int test_inc_ttl) {
      scoped_ptr<MpingSocket> mysock(new MpingSocket);
      EXPECT_GE(mysock->Initialize(std::string("127.0.0.1"), src_addr, ttl,
                             kMaxBuffer, win_size, dport, client_mode), 0);

      loop_count = 0;
      ttl = test_ttl;
      inc_ttl = test_inc_ttl;

      TTLLoop(mysock.get(), &mp_stat);
    }

  protected:
    virtual void BufferLoop(MpingSocket *sock, MpingStat *stat) {
      loop_count++;
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

class MPingBufferLoopTest : public MPingUnitTestBase {
  public:
    std::vector<size_t> size_array;

    MPingBufferLoopTest() : MPingUnitTestBase() { } 
    void GoTest(size_t p_size, int l_size) {
      pkt_size = p_size;
      loop_size = l_size;
      size_array.clear();

      MpingSocket tempmpsock;
      BufferLoop(&tempmpsock, &mp_stat);
    }

  protected:
    virtual void WindowLoop(MpingSocket *sock, MpingStat *stat) {
      size_array.push_back(cur_packet_size);
    }
};

TEST(MpingUnitTest, BufferLoopTest) {
  MPingBufferLoopTest t;
  t.GoTest(256, 0);
  EXPECT_EQ(t.size_array.size(), 1u);
  EXPECT_EQ(t.size_array.at(0), 256u);

  t.GoTest(0, -1);
  EXPECT_EQ(t.size_array.size(), 8u);
  EXPECT_EQ(t.size_array.at(0), 64u);
  EXPECT_EQ(t.size_array.at(7), 4000u);

  t.GoTest(0, -2);
  EXPECT_EQ(t.size_array.size(), 23u);  // max 1500, 1500/64=23
  EXPECT_EQ(t.size_array.at(0), 64u);
  EXPECT_EQ(t.size_array.at(22), 1472u);  // 64 * 23 = 1472
  
  t.GoTest(0, -3);
  EXPECT_EQ(t.size_array.size(), 16u);  // max 2048, 2048/128=16
  EXPECT_EQ(t.size_array.at(0), 128u);
  EXPECT_EQ(t.size_array.at(15), 2048u);  // 128 * 16 = 2048

  t.GoTest(0, -4);
  EXPECT_EQ(t.size_array.size(), 17u);  // max 4500, 4500/256=17
  EXPECT_EQ(t.size_array.at(0), 256u);
  EXPECT_EQ(t.size_array.at(16), 4352u);  // 256 * 17 = 4352
}

class MPingWinLoopTest: public MPingUnitTestBase {
  public:
    int loop_forever_count;
    std::vector<uint16_t> win_array;

    MPingWinLoopTest() : MPingUnitTestBase() { }
    void GoTest(bool isloop, int test_inc_ttl, int test_loop_size) {
      MpingSocket tempmpsock;

      loop = isloop;
      inc_ttl = test_inc_ttl;
      loop_size = test_loop_size;

      win_array.clear();
      loop_forever_count = 0;
      haltf = 0;
      WindowLoop(&tempmpsock, &mp_stat);
    }

  protected:
    virtual void IntervalLoop(uint16_t intran, MpingSocket *sock, 
                              MpingStat *stat) {
      if (intran == 0)
        return;

      if (loop && inc_ttl == 0 && loop_size == 0) {
        if (loop_forever_count == 2)
          haltf = 1;
        else
          loop_forever_count++;
      }
      win_array.push_back(intran);
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
