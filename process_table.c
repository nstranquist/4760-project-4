

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "process_table.h"
#include "config.h"
#include "utils.h"

struct ProcessTable *process_table;

// function implementations to work with process table
void incrementClockRound(int isIdle) {
  // get random ns [0,1000]
  int ns = getRandom(1001);

  // create new time with 1 + ns
  addTimeToClock(1, ns);

  if(isIdle == 1) {
    process_table->total_idle_sec += 1;
    process_table->total_idle_ns += ns;
  }
}

void addTimeToClock(int sec, int ns) {
  // add seconds
  process_table->sec += sec;

  // check ns for overflow, handle accordingly
  if((process_table->ns + ns) >= 1000000) {
    int remaining_ns = (process_table-> ns + ns) - 1000000;
    process_table->sec += 1;
    process_table->ns = remaining_ns;
  }
  else
    process_table->ns += ns;
  
  printf("\n");
  printf("sec: %d\tns: %d\n", process_table->sec, process_table->ns);
}


int isTableFull() {
  // how to check?
  
}