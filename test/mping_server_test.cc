#include "mp_common.h"
#include "mp_server.h"

#include <arpa/inet.h>
#include "gtest/gtest.h"

#include "scoped_ptr.h"
#include "mlab/accepted_socket.h"
#include "mlab/listen_socket.h"
#include "mlab/client_socket.h"
#include "mlab/socket_family.h"

TEST(MPingServerTest, CookieTest) {
  MPingServer mpsrv(128, 8000, SOCKETFAMILY_IPV4);

  for (uint16_t i = 1; i < 9; i++) {
    timeval now;
    gettimeofday(&now, 0);
    ClientStateNode *node = new ClientStateNode(ECHOMODE_WHOLE, now);
    EXPECT_EQ(mpsrv.InsertClient(node), i);
  }

  for (uint16_t i = 1; i < 9; i++) {
    EXPECT_TRUE(mpsrv.CheckClient(i) != NULL);
  }

  EXPECT_TRUE(mpsrv.CheckClient(10) == NULL);

  mpsrv.DeleteClient(3);
  mpsrv.DeleteClient(5);
  EXPECT_TRUE(mpsrv.CheckClient(3) == NULL);
  EXPECT_TRUE(mpsrv.CheckClient(5) == NULL);
}
