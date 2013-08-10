#include <byteswap.h>

#include "mp_common.h"

const char *kPayloadHeader = "mlab-seq#";                                      
const size_t kPayloadHeaderLength = strlen(kPayloadHeader);  

const char *kStrHeader = "ML";
const size_t kStrHeaderLength = strlen(kStrHeader);
const size_t kAllHeaderLen = kStrHeaderLength + 2 * sizeof(MPSEQTYPE);
const size_t kMinBuffer = 9000;  // > 2 FDDI?

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
