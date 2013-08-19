#include "mp_common.h"

#include <arpa/inet.h>
#include "gtest/gtest.h"

#include "scoped_ptr.h"
#include "mlab/accepted_socket.h"
#include "mlab/listen_socket.h"
#include "mlab/client_socket.h"

TEST(MpingCommonTest, HostNetConversion) {
  if (htonl(1) == 1) {
    EXPECT_EQ(1u, MPhton64(1));
    EXPECT_EQ(1u, MPntoh64(1));
  } else {
    EXPECT_EQ(0x0100000000000000u, MPhton64(1));
    EXPECT_EQ(0x0100000000000000u, MPntoh64(1));
  }
}

TEST(MpingCommonTest, GetTCPMsgCodeFromStringTest) {
  EXPECT_EQ(MPTCP_HELLO, GetTCPMsgCodeFromString(kTCPHelloMessage));
  EXPECT_EQ(MPTCP_DONE, GetTCPMsgCodeFromString(kTCPDoneMessage));
  EXPECT_EQ(MPTCP_CONFIRM, GetTCPMsgCodeFromString(kTCPConfirmMessage));
  EXPECT_EQ(MPTCP_UNKNOWN, GetTCPMsgCodeFromString("ABCDE"));
}

TEST(MpingCommonTest, GetTCPServerEchoModeFromShortTest) {
  EXPECT_EQ(ECHOMODE_WHOLE, GetTCPServerEchoModeFromShort(1));
  EXPECT_EQ(ECHOMODE_SMALL, GetTCPServerEchoModeFromShort(2));
  EXPECT_EQ(ECHOMODE_LARGE, GetTCPServerEchoModeFromShort(3));
  EXPECT_EQ(ECHOMODE_UNSPEC, GetTCPServerEchoModeFromShort(4));
  EXPECT_EQ(ECHOMODE_UNSPEC, GetTCPServerEchoModeFromShort(0));
}

TEST(MpingCommonTest, MPTCPMessagTest) {
  MPTCPMessage msg(MPTCP_HELLO, ECHOMODE_WHOLE, 0);
  EXPECT_STREQ(kTCPHelloMessage, msg.msg_code);
  EXPECT_EQ(ECHOMODE_WHOLE, GetTCPServerEchoModeFromShort(ntohs(msg.msg_type)));

  MPTCPMessage msg1(MPTCP_DONE, ECHOMODE_WHOLE, 0);
  EXPECT_STREQ(kTCPDoneMessage, msg1.msg_code);

  MPTCPMessage msg2(MPTCP_CONFIRM, ECHOMODE_WHOLE, 1);
  EXPECT_STREQ(kTCPConfirmMessage, msg2.msg_code);
}

TEST(MpingCommonTest, SocketSendRecvTest) {
  scoped_ptr<mlab::ListenSocket> 
      l_sock(mlab::ListenSocket::CreateOrDie(8000));

  scoped_ptr<mlab::ClientSocket> 
      c_sock(mlab::ClientSocket::CreateOrDie(mlab::Host("127.0.0.1"), 8000));

  scoped_ptr<mlab::AcceptedSocket> a_sock(l_sock->Accept());

  std::string send_str("MpingCommonTest, SocketSendRecvTest"); 

  char recv_buffer[64] = "";
  EXPECT_EQ(0, StreamSocketSendWithTimeout(c_sock->raw(),
                                           send_str.c_str(), send_str.size()));
  EXPECT_EQ(0, StreamSocketRecvWithTimeout(a_sock->client_raw(), recv_buffer,
                                           send_str.size()));
  EXPECT_STREQ(send_str.c_str(), recv_buffer);
}

