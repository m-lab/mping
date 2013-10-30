// Copyright 2012 M-Lab. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _MLAB_LOG_H_
#define _MLAB_LOG_H_

#include "mlab/mlab.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

namespace mlab {

FILE* GetSeverityFD(mlab::LogSeverity s);
const char* GetSeverityTag(mlab::LogSeverity s);

}  // namespace mlab

#define ASSERT(predicate) if (!(predicate)) raise(SIGABRT)

#define LOG(severity, format, ...) { \
    if (FILE* fd = GetSeverityFD(severity)) { \
      fprintf(fd, "[%s] %s|%d: ", GetSeverityTag(severity), \
              __FILE__, __LINE__); \
      fprintf(fd, format, ##__VA_ARGS__); \
      fprintf(fd, "\n"); \
      fflush(fd); \
      if (severity == mlab::FATAL) exit(1); \
    } \
  }

#endif  // _MLAB_LOG_H_
