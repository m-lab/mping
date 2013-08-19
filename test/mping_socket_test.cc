#include "gtest/gtest.h"
#include "mlab/mlab.h"
#include "mp_socket.h"

TEST(MpingSocket, IPv4ICMP) {
  MpingSocket testsock;
  testsock.Initialize("127.0.0.1", "", 0, 1024, 1, 0, 0);

  int err;
  MpingStat mystat(1, false);

  testsock.SendPacket(1, 1, 1024, &err);
  EXPECT_EQ(1, testsock.ReceiveAndGetSeq(&err, &mystat));
}

TEST(MpingSocket, IPv4ICMPwithSrc) {
  MpingSocket test2sock;
  test2sock.Initialize("127.0.0.1", "127.0.0.1", 0, 1024, 1, 0, 0);

  int err;
  MpingStat mystat(1, false);

  test2sock.SendPacket(2, 1, 1024, &err);
  EXPECT_EQ(2, test2sock.ReceiveAndGetSeq(&err, &mystat));
}

TEST(MpingSocket, IPv6ICMP) {
  MpingSocket testsock;
  testsock.Initialize("::1", "", 0, 1024, 1, 0, 0);
  
  int err;
  MpingStat mystat(1, false);

  testsock.SendPacket(3, 1, 1024, &err);
  EXPECT_EQ(3, testsock.ReceiveAndGetSeq(&err, &mystat));
}

TEST(MpingSocket, IPv6ICMPwithSrc) {
  MpingSocket test2sock;
  test2sock.Initialize("::1", "::1",
                        0, 1024, 1, 0, false);
  
  int err;
  MpingStat mystat(1, false);

  test2sock.SendPacket(4, 1, 1024, &err);
  EXPECT_EQ(4, test2sock.ReceiveAndGetSeq(&err, &mystat));
}

TEST(MpingSocket, IPv4UDP) {
  MpingSocket testsock;
  testsock.Initialize("127.0.0.1", "", 14, 1024, 1, 0, 0);
  
  int err;
  MpingStat mystat(1, false);

  testsock.SendPacket(5, 1, 1024, &err);
  EXPECT_EQ(5, testsock.ReceiveAndGetSeq(&err, &mystat));
}

TEST(MpingSocket, IPv4UDPwithSrc) {
  MpingSocket test2sock;
  test2sock.Initialize("127.0.0.1", "127.0.0.1", 14, 1024, 1, 0, 0);
  
  int err;
  MpingStat mystat(1, false);

  test2sock.SendPacket(6, 1, 1024, &err);
  EXPECT_EQ(6, test2sock.ReceiveAndGetSeq(&err, &mystat));
}

TEST(MpingSocket, IPv6UDP) {
  MpingSocket testsock;
  testsock.Initialize("::1", "", 14, 1024, 1, 0, 0);
  
  int err;
  MpingStat mystat(1, false);

  testsock.SendPacket(7, 1, 1024, &err);
  EXPECT_EQ(7, testsock.ReceiveAndGetSeq(&err, &mystat));
}

TEST(MpingSocket, IPv6UDPwithSrc) {
  MpingSocket test2sock;
  test2sock.Initialize("::1", "::1",
                        14, 1024, 1, 0, 0);
  
  int err;
  MpingStat mystat(1, false);

  test2sock.SendPacket(8, 1, 1024, &err);
  EXPECT_EQ(8, test2sock.ReceiveAndGetSeq(&err, &mystat));
}

