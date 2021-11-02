

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "process_table.h"
#include "config.h"
#include "utils.h"

struct ProcessTable *process_table;

// function implementations to work with process table
Time incrementClockRound() {
  // get random ns [0,1000]
  int ns = getRandom(1001); // 1.xx is NOT nanoseconds.

  // nanoseconds goes from 0 to MAX_INT

  // create new time with 1 + ns
  Time time_diff = addTimeToClock(1, ns);

  return time_diff;
}

// returns time difference to add to stats
Time addTimeToClock(int sec, int ns) {
  // add seconds
  process_table->sec += sec;

  // check ns for overflow, handle accordingly
  if((process_table->ns + ns) >= NANOSECONDS) {
    int remaining_ns = (process_table-> ns + ns) - NANOSECONDS;
    process_table->sec += 1;
    process_table->ns = remaining_ns;
  }
  else
    process_table->ns += ns;
  
  printf("\n");
  printf("sec: %d\tns: %d\n", process_table->sec, process_table->ns);

  Time time_diff = {sec, ns};

  return time_diff;
}


int isTableFull() {
  // how to check?
  
}

Time getClockTime() {
  Time time = {process_table->sec, process_table->ns};
  return time;
}