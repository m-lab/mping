#include "mp_common.h"

#include <arpa/inet.h>
#include "gtest/gtest.h"

TEST(MpingCommonTest, CheckEndianTest) {
  if (IsBigEndian())
    EXPECT_EQ(1u, htonl(1));
  else
    EXPECT_LT(htonl(1), 1u);
}
