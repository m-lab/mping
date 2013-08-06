#include "mp_common.h"

const char *kPayloadHeader = "mlab-seq#";                                      
// const int kPayloadMlabHeaderLength = 4;
const int kPayloadHeaderLength = strlen(kPayloadHeader);  
const size_t kMinBuffer = 9000;  // > 2 FDDI?
