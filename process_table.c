

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "process_table.h"
#include "config.h"
#include "utils.h"

struct ProcessTable *process_table;

// function implementations to work with process table
void incrementClockRound(int isIdle) {
  // get random ms [0,1000]
  int ms = getRandom(1000);

  // create new time with 1 + ms
  addTimeToClock(1, ms);

  if(isIdle == 1) {
    process_table->total_idle_sec += 1;
    process_table->total_idle_ms += ms;
  }
}

void addTimeToClock(int sec, int ms) {
  // add seconds
  process_table->sec += sec;

  // check ms for overflow, handle accordingly
  if((process_table->ms + ms) >= 1000) {
    int remaining_ms = (process_table-> ms + ms) - 1000;
    process_table->sec += 1;
    process_table->ms = remaining_ms;
  }
  else
    process_table->ms += ms;
  
  printf("\n");
  printf("sec: %d\tms: %d\n", process_table->sec, process_table->ms);
}
