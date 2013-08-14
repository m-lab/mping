#ifndef _MPING_COMMON_H_
#define _MPING_COMMON_H_

#ifndef __STDC_FORMAT_MACROS  // for print int64_t with PRIxx Macros
#define __STDC_FORMAT_MACROS
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include <string>

#include <mp_log.h>
typedef int64_t MPSEQTYPE;

// extern const char* kPayloadHeader;
// extern const size_t kPayloadHeaderLength;

extern const char *kStrHeader;
extern const size_t kStrHeaderLength;
extern const size_t kCookieOffset;
extern const size_t kCookieSize;
extern const size_t kSequenceOffset;
extern const size_t kAllHeaderLen;
extern const size_t kMinBuffer;

bool IsBigEndian();
uint64_t MPhton64(uint64_t int_host, bool is_big_end);
uint64_t MPntoh64(uint64_t int_net, bool is_big_end);

// server echo mode
enum ServerEchoMode {
  ECHOMODE_UNSPEC,
  ECHOMODE_WHOLE,
  ECHOMODE_SMALL,
  ECHOMODE_LARGE  // reserve for future
};

enum MPTCPMessageCode {
  MPTCP_UNKNOWN,
  MPTCP_HELLO,
  MPTCP_DONE,
  MPTCP_CONFIRM
};

extern const char *kTCPHelloMessage;
extern const char *kTCPDoneMessage;
extern const char *kTCPConfirmMessage;

struct MPTCPMessage {
  char msg_code[4];
  uint16_t msg_type;
  uint16_t msg_value;

  MPTCPMessage(MPTCPMessageCode code, uint16_t type, uint16_t value) 
    : msg_type(htons(type)),
      msg_value(htons(value)) {
    switch (code) {
      case MPTCP_HELLO:
        memcpy(msg_code, kTCPHelloMessage, 4);
        break;
      case MPTCP_DONE:
        memcpy(msg_code, kTCPDoneMessage, 4);
        break;
      case MPTCP_CONFIRM:
        memcpy(msg_code, kTCPConfirmMessage, 4);
        break;
      default:
        LOG(FATAL, "unknow TCP code.");
        break;
    }
  }
};

MPTCPMessageCode GetTCPMsgCodeFromString(std::string str_code);
ServerEchoMode GetTCPServerEchoModeFromShort(uint16_t type);

extern const size_t kMPTCPMessageSize;
extern const time_t kDefaultCleanUpTime;

ssize_t StreamSocketSendWithTimeout(int sock, const char* buffer, size_t size);
ssize_t StreamSocketRecvWithTimeout(int sock, char* buffer, size_t size);

#endif
