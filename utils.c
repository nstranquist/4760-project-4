#include <stdlib.h>
#include "utils.h"

int getRandom(int upper_bound) {
  int randNum = rand() % (upper_bound + 1); // adding 1 for convenience, since rand(20) will yield 0-19
  return randNum;
}
