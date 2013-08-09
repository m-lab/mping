#include <iostream>
#include <string.h>
#include <errno.h>

#include "mp_server.h"

int main(int argc, const char** argv) {
  MPingServer mp(argc, argv);

  mp.Run();

  return 0;
}
