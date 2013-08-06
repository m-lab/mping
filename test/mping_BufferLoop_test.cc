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

class MPingBufferLoopTest : public MPingUnitTestBase {
  public:
    std::vector<size_t> size_array;

    MPingBufferLoopTest() : MPingUnitTestBase() { } 
    void GoTest(size_t p_size, int l_size) {
      pkt_size = p_size;
      loop_size = l_size;
      size_array.clear();

      MpingSocket tempmpsock;
      BufferLoop(&tempmpsock);
    }

  protected:
    virtual bool WindowLoop(MpingSocket *sock) {
      size_array.push_back(cur_packet_size);
      return true;
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

