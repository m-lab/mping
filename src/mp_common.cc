#include "mp_common.h"

const char *kPayloadHeader = "mlab-seq#";                                      
// const int kPayloadMlabHeaderLength = 4;
const int kPayloadHeaderLength = 9;  // include the padding for keep checksum 
                                     // constant
const size_t kMaxBuffer = 9000;  // > 2 FDDI?
