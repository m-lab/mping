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

      Run();
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
