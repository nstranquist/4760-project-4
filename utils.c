#include <stdlib.h>
#include "utils.h"

int getRandom(int upper_bound) {
  int randNum = rand() % (upper_bound);
  return randNum;
}
