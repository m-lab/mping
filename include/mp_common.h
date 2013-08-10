#ifndef _MPING_COMMON_H_
#define _MPING_COMMON_H_

#ifndef __STDC_FORMAT_MACROS  // for print int64_t with PRIxx Macros
#define __STDC_FORMAT_MACROS
#endif

#include <string.h>
#include <inttypes.h>

typedef int64_t MPSEQTYPE;

extern const char* kPayloadHeader;
extern const size_t kPayloadHeaderLength;

extern const char *kStrHeader;
extern const size_t kStrHeaderLength;
extern const size_t kAllHeaderLen;
extern const size_t kMinBuffer;

bool IsBigEndian();
uint64_t MPhton64(uint64_t int_host, bool is_big_end);
uint64_t MPntoh64(uint64_t int_net, bool is_big_end);

#endif
