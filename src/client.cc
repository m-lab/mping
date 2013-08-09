#include <iostream>
#include <string.h>
#include <errno.h>

#include "mp_client.h"

int main(int argc, const char** argv) {
  MPingClient mp(argc, argv);

  mp.Run();

  return 0;
}
