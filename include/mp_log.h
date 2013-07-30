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

#ifndef _MPING_LOG_H_
#define _MPING_LOG_H_


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

enum LogSeverity {
  VERBOSE,
  INFO,
  WARNING,
  ERROR,
  FATAL
};

void SetLogSeverity(LogSeverity s);
FILE* GetSeverityFD(LogSeverity s);
const char* GetSeverityTag(LogSeverity s);

#define ASSERT(predicate) if (!(predicate)) raise(SIGABRT)

#define LOG(severity, format, ...) { \
    if (FILE* fd = GetSeverityFD(severity)) { \
      fprintf(fd, "[%s] %s|%d: ", GetSeverityTag(severity), \
              __FILE__, __LINE__); \
      fprintf(fd, format, ##__VA_ARGS__); \
      fprintf(fd, "\n"); \
      fflush(fd); \
      ASSERT(severity != FATAL); \
    } \
  }

enum MPLogLevel {
  MPLOG_DEF,
  MPLOG_TTL,
  MPLOG_BUF,
  MPLOG_WIN
};

void PrintMpingLogLevel(MPLogLevel level);

// MPLOG only print information that would be parsed
#define MPLOG(level, format, ...) { \
  PrintMpingLogLevel(level); \
  fprintf(stdout, format, ##__VA_ARGS__); \
  fprintf(stdout, "\n"); \
  fflush(stdout); \
}
#endif  // _MPING_LOG_H_
