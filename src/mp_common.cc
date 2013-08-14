#include <byteswap.h>

#include "mp_common.h"

// const char *kPayloadHeader = "mlab-seq#";
// const size_t kPayloadHeaderLength = strlen(kPayloadHeader);

const char *kStrHeader = "ML";
const size_t kStrHeaderLength = strlen(kStrHeader);
const size_t kCookieOffset = kStrHeaderLength;
const size_t kCookieSize = sizeof(uint16_t);
const size_t kSequenceOffset = kStrHeaderLength + kCookieSize;

// all length include the last reserved 4 bytes
const size_t kAllHeaderLen = kSequenceOffset + 2 * sizeof(MPSEQTYPE) + 4;
const size_t kMinBuffer = 9000;  // > 2 FDDI?

const char *kTCPHelloMessage = "HLLO";
const char *kTCPDoneMessage = "DONE";
const char *kTCPConfirmMessage = "CFRM";
const size_t kMPTCPMessageSize = sizeof(MPTCPMessage);
const time_t kDefaultCleanUpTime = 300;  // seconds

bool IsBigEndian() {
  int i = 1;
  if (*((char*)&i))
    return false;
  else
    return true;
}

uint64_t MPhton64(uint64_t int_host, bool is_big_end) {
  if (is_big_end)
    return int_host;
  else
    return bswap_64(int_host);
}

uint64_t MPntoh64(uint64_t int_net, bool is_big_end) {
  if (is_big_end)
    return int_net;
  else
    return bswap_64(int_net);
}

MPTCPMessageCode GetTCPMsgCodeFromString(std::string str_code) {
  if (str_code == kTCPHelloMessage)
    return MPTCP_HELLO;

  if (str_code == kTCPDoneMessage)
    return MPTCP_DONE;

  if (str_code == kTCPConfirmMessage)
    return MPTCP_CONFIRM;

  return MPTCP_UNKNOWN;
}

ServerEchoMode GetTCPServerEchoModeFromShort(uint16_t type) {
  if (type == 0x01)
    return ECHOMODE_WHOLE;

  if (type == 0x02)
    return ECHOMODE_SMALL;

  if (type == 0x03)
    return ECHOMODE_LARGE;

  return ECHOMODE_UNSPEC;
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
