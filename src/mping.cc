#include <iostream>
#include <string.h>
#include <errno.h>

#include "mp_ioctrl.h"
#include "log.h"
#include "mping.h"
#include "mlab/mlab.h"
#include "mlab/raw_socket.h"

#include "scoped_ptr.h"

int haltf = 0;
int tick = 0;

int main(int argc, const char** argv) {
  IOCtrl ioc;

  ioc.GetConfigPara(argc, argv);

  // register signal handler
  struct sigaction sa, osa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = ring;
  sa.sa_flags = SA_INTERRUPT;
  if (sigaction(SIGALRM, &sa, &osa) < 0) {                                              
    LOG(mlab::FATAL, "sigaction SIGALRM.");                                        
  }                                                                             
                                                                                
  sa.sa_handler = halt;
  if (sigaction(SIGINT, &sa, &osa) < 0) {                                                
    LOG(mlab::FATAL, "sigaction SIGINT");                                          
  }                                                                             
  haltf = 0;

  ioc.Start();
  

  return 0;
}

double time_sub(const struct timeval *new_t, const struct timeval *old_t) {
  struct timeval temp;

  temp.tv_usec = new_t->tv_usec - old_t->tv_usec;
  temp.tv_sec = new_t->tv_sec - old_t -> tv_sec;
  if (temp.tv_usec < 0) {
    --temp.tv_sec;
    temp.tv_usec += 1000000;
  }

  return (temp.tv_sec * 1000.0 + temp.tv_usec / 1000.0);  // in millisec
}

void ring(int signo) {
  struct sigaction sa, osa;
  sa.sa_handler = ring;
  sa.sa_flags = SA_INTERRUPT;
  if (sigaction(SIGALRM, &sa, &osa) < 0) {
    LOG(mlab::FATAL, "sigaction SIGALRM. %s [%d]", strerror(errno), errno); 
  }
  tick = 0;
}

void halt(int signo) {
  struct sigaction sa, osa;
  sa.sa_flags = SA_INTERRUPT;
  if (++haltf >= 2) {
    sa.sa_handler = 0;
    if (sigaction(SIGINT, &sa, &osa)) {
      LOG(mlab::FATAL, "sigaction SIGINT. %s [%d]", strerror(errno), errno);
    }
  } else {
    sa.sa_handler = halt;
    if (sigaction(SIGINT, &sa, &osa) < 0) {
      LOG(mlab::FATAL, "sigaction SIGINT. %s [%d]", strerror(errno), errno);
    }
  }
}
