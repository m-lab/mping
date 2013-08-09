#include "mp_common.h"

const char *kPayloadHeader = "mlab-seq#";                                      
const size_t kPayloadHeaderLength = strlen(kPayloadHeader);  

const char *kStrHeader = "ML";
const size_t kStrHeaderLength = strlen(kStrHeader);
const size_t kStartOfSeq = kStrHeaderLength + sizeof(MPSEQTYPE);
const size_t kAllHeaderLen = kStartOfSeq + sizeof(MPSEQTYPE);
const size_t kMinBuffer = 9000;  // > 2 FDDI?
