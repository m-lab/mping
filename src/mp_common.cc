#include <byteswap.h>
#include <errno.h>

#include "mp_common.h"

namespace {

bool IsBigEndian() {
  union {
    uint32_t i;
    char c[4];
  } bint = {0x01020304};
  
  return bint.c[0] == 1;
}

}  // namespace

const int kDefaultTTL = 255;

const char *kStrHeader = "ML";
const size_t kStrHeaderLength = strlen(kStrHeader);
const size_t kCookieOffset = kStrHeaderLength;
const size_t kCookieSize = sizeof(uint16_t);
const size_t kSequenceOffset = kStrHeaderLength + kCookieSize;

// all length include the last reserved 4 bytes
const size_t kAllHeaderLen = kSequenceOffset + 2 * sizeof(int64_t) + 4;
const size_t kMinBuffer = 9000;  // > 2 FDDI?

const char *kTCPHelloMessage = "HLLO";
const char *kTCPDoneMessage = "DONE";
const char *kTCPConfirmMessage = "CFRM";
const size_t kMPTCPMessageSize = sizeof(MPTCPMessage);
const time_t kDefaultCleanUpTime = 300;  // seconds
//TODO(xunfan): compare client and server version to ensure compatibility
const char *kVersion = "2.1 (2013.08)";

uint64_t MPhton64(uint64_t int_host) {
  return IsBigEndian() ? int_host : bswap_64(int_host);
}

uint64_t MPntoh64(uint64_t int_net) {
  return IsBigEndian() ? int_net : bswap_64(int_net);
}

MPTCPMessage::MPTCPMessage(MPTCPMessageCode code, ServerEchoMode type,
                           uint16_t value) 
    : msg_type(htons(type)),
      msg_value(htons(value)) {
  switch (code) {
      case MPTCP_HELLO:
        strncpy(msg_code, kTCPHelloMessage, 4);
        break;
      case MPTCP_DONE:
        strncpy(msg_code, kTCPDoneMessage, 4);
        break;
      case MPTCP_CONFIRM:
        strncpy(msg_code, kTCPConfirmMessage, 4);
        break;
      default:
        LOG(FATAL, "unknow TCP code.");
        break;
    }
}

MPTCPMessageCode GetTCPMsgCodeFromString(const std::string& str_code) {
  if (str_code == kTCPHelloMessage)
    return MPTCP_HELLO;

  if (str_code == kTCPDoneMessage)
    return MPTCP_DONE;

  if (str_code == kTCPConfirmMessage)
    return MPTCP_CONFIRM;

  LOG(ERROR, "unknown TCP message code.");
  return MPTCP_UNKNOWN;
}

ServerEchoMode GetTCPServerEchoModeFromShort(uint16_t type) {
  if (type >= NUM_ECHO_MODES)
    return ECHOMODE_UNSPEC;

  return (ServerEchoMode)type;
}

ssize_t StreamSocketSendWithTimeout(int sock, const char* buffer, size_t size) {
  size_t remain_size = size;
  ssize_t sent = 0;

  while (remain_size > 0) {
    errno = 0;
    ssize_t num = send(sock, buffer+sent, remain_size, 0);

    if (num < 0) {
      if (errno == EINTR)  // interupted because Timeout set, resume
        continue;
      else
        return num;
    } else {
      sent += num;
      remain_size -= num;
    }
  }

  return 0;
}

ssize_t StreamSocketRecvWithTimeout(int sock, char* buffer, size_t size) {
  size_t remain_size = size;
  ssize_t recved = 0;

  while (remain_size > 0) {
    errno = 0;
    ssize_t num = recv(sock, buffer+recved, remain_size, 0);

    if (num < 0) {
      if (errno == EINTR)
        continue;
      else 
        return num;
    } else {
      remain_size -= num;
      recved += num;
    }
  }

  return 0;
}
