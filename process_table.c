

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "process_table.h"
#include "config.h"
#include "utils.h"

struct ProcessTable *process_table;

// function implementations to work with process table
void incrementClockRound() {
  // get random ms [0,1000]
  int ms = getRandom(1000);

  // create new time with 1 + ms
  addTimeToClock(1, ms);
}

void addTimeToClock(int sec, int ms) {
  process_table->sec += sec;
  process_table->ms += ms;
  
  printf("\n");
  printf("sec: %d\tms: %d\n", process_table->sec, process_table->ms);
}
