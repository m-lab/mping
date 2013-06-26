#include <iostream>
#include <string.h>
#include <errno.h>

#include "mp_mping.h"

int main(int argc, const char** argv) {
  MPing mp(argc, argv);

  mp.Run();

  return 0;
}
