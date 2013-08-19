#ifndef _MPING_COMMON_H_
#define _MPING_COMMON_H_

#include <arpa/inet.h>
#include <string.h>

#include <string>

#include "mp_log.h"

extern const int kDefaultTTL;

extern const char *kStrHeader;
extern const size_t kStrHeaderLength;
extern const size_t kCookieOffset;
extern const size_t kCookieSize;
extern const size_t kSequenceOffset;
extern const size_t kAllHeaderLen;
extern const size_t kMinBuffer;
extern const char *kTCPHelloMessage;
extern const char *kTCPDoneMessage;
extern const char *kTCPConfirmMessage;
extern const size_t kMPTCPMessageSize;
extern const time_t kDefaultCleanUpTime;
extern const char *kVersion;

uint64_t MPhton64(uint64_t int_host);
uint64_t MPntoh64(uint64_t int_net);

// server echo mode

enum ServerEchoMode {
  ECHOMODE_UNSPEC,
  ECHOMODE_WHOLE,
  ECHOMODE_SMALL,
  ECHOMODE_LARGE,  // reserve for future
  NUM_ECHO_MODES
};

enum MPTCPMessageCode {
  MPTCP_UNKNOWN,
  MPTCP_HELLO,
  MPTCP_DONE,
  MPTCP_CONFIRM
};

struct MPTCPMessage {
  char msg_code[4];
  uint16_t msg_type;
  uint16_t msg_value;

  MPTCPMessage(MPTCPMessageCode code, ServerEchoMode type, uint16_t value);
};

MPTCPMessageCode GetTCPMsgCodeFromString(const std::string& str_code);
ServerEchoMode GetTCPServerEchoModeFromShort(uint16_t type);

ssize_t StreamSocketSendWithTimeout(int sock, const char* buffer, size_t size);
ssize_t StreamSocketRecvWithTimeout(int sock, char* buffer, size_t size);

#endif
