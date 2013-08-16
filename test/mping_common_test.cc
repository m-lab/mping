#include "mp_common.h"

#include <arpa/inet.h>
#include "gtest/gtest.h"

#include "scoped_ptr.h"
#include "mlab/accepted_socket.h"
#include "mlab/listen_socket.h"
#include "mlab/client_socket.h"

TEST(MpingCommonTest, CheckEndianTest) {
  if (IsBigEndian())
    EXPECT_EQ(1u, htonl(1));
  else
    EXPECT_GT(htonl(1), 1u);
}

TEST(MpingCommonTest, HostNetConversion) {
  if (htonl(1) == 1) {
    EXPECT_EQ(MPhton64(1), 1u);
    EXPECT_EQ(MPntoh64(1), 1u);
  } else {
    EXPECT_GT(MPhton64(1), 1u);
    EXPECT_GT(MPntoh64(1), 1u);
  }
}

TEST(MpingCommonTest, GetTCPMsgCodeFromStringTest) {
  EXPECT_EQ(GetTCPMsgCodeFromString(kTCPHelloMessage), MPTCP_HELLO);
  EXPECT_EQ(GetTCPMsgCodeFromString(kTCPDoneMessage), MPTCP_DONE);
  EXPECT_EQ(GetTCPMsgCodeFromString(kTCPConfirmMessage), MPTCP_CONFIRM);
  EXPECT_EQ(GetTCPMsgCodeFromString("ABCDE"), MPTCP_UNKNOWN);
}

TEST(MpingCommonTest, GetTCPServerEchoModeFromShortTest) {
  EXPECT_EQ(GetTCPServerEchoModeFromShort(1), ECHOMODE_WHOLE);
  EXPECT_EQ(GetTCPServerEchoModeFromShort(2), ECHOMODE_SMALL);
  EXPECT_EQ(GetTCPServerEchoModeFromShort(3), ECHOMODE_LARGE);
  EXPECT_EQ(GetTCPServerEchoModeFromShort(4), ECHOMODE_UNSPEC);
  EXPECT_EQ(GetTCPServerEchoModeFromShort(0), ECHOMODE_UNSPEC);
}

TEST(MpingCommonTest, MPTCPMessagTest) {
  MPTCPMessage msg(MPTCP_HELLO, ECHOMODE_WHOLE, 0);
  EXPECT_STREQ(msg.msg_code, kTCPHelloMessage);
  EXPECT_EQ(GetTCPServerEchoModeFromShort(ntohs(msg.msg_type)), ECHOMODE_WHOLE);

  MPTCPMessage msg1(MPTCP_DONE, ECHOMODE_WHOLE, 0);
  EXPECT_STREQ(msg1.msg_code, kTCPDoneMessage);

  MPTCPMessage msg2(MPTCP_CONFIRM, ECHOMODE_WHOLE, 1);
  EXPECT_STREQ(msg2.msg_code, kTCPConfirmMessage);
}

TEST(MpingCommonTest, SocketSendRecvTest) {
  scoped_ptr<mlab::ListenSocket> 
      l_sock(mlab::ListenSocket::CreateOrDie(8000));

  scoped_ptr<mlab::ClientSocket> 
      c_sock(mlab::ClientSocket::CreateOrDie(mlab::Host("127.0.0.1"), 8000));

  scoped_ptr<mlab::AcceptedSocket> a_sock(l_sock->Accept());

  std::string send_str("MpingCommonTest, SocketSendRecvTest"); 

  char recv_buffer[64] = "";
  EXPECT_EQ(StreamSocketSendWithTimeout(c_sock->raw(),
                                        send_str.c_str(), send_str.size()), 0);
  EXPECT_EQ(StreamSocketRecvWithTimeout(a_sock->client_raw(), recv_buffer, 
                                        send_str.size()), 0);
  EXPECT_STREQ(send_str.c_str(), recv_buffer);
}

