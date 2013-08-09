#ifndef _MPING_COMMON_H_
#define _MPING_COMMON_H_

#include <string.h>
#include <stdint.h>

typedef uint64_t MPSEQTYPE;

extern const char* kPayloadHeader;
extern const size_t kPayloadHeaderLength;

extern const char *kStrHeader;
extern const size_t kStrHeaderLength;
extern const size_t kStartOfSeq;
extern const size_t kAllHeaderLen;
extern const size_t kMinBuffer;

#endif
