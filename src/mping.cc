#include <iostream>
#include <string.h>
#include <errno.h>

#include "mp_mping.h"

int main(int argc, const char** argv) {
  MPing mp(argc, argv);

  if (mp.IsServerMode()) {
    mp.RunServer();
  } else {
    mp.RunClient();
  }

//  return 0;
}
