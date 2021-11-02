

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "process_table.h"
#include "config.h"
#include "utils.h"

struct ProcessTable *process_table;

void init_process_table_pcbs() {
  for (int i = 0; i < NUMBER_PCBS; i++) {
    process_table->pcb_array[i].pid = -1;
    process_table->pcb_array[i].priority = 0;
    // ...
  }
}

// function implementations to work with process table
Time incrementClockRound() {
  // get random ns [0,1000] (ms)
  int ms = getRandom(MILISECONDS+1);

  // convert ms to ns
  int ns = ms * 1000000;

  // create new time with 1 + ns
  Time time_diff = addTimeToClock(1, ns);

  return time_diff;
}

// returns time difference to add to stats
Time addTimeToClock(int sec, int ns) {
  printf("prev sec: %d\tns: %d\n", process_table->sec ,process_table->ns);
  // add seconds
  process_table->sec = process_table->sec + sec;

  // check ns for overflow, handle accordingly
  if((process_table->ns + ns) >= NANOSECONDS) {
    int remaining_ns = (process_table->ns + ns) - NANOSECONDS;
    process_table->sec += 1;
    process_table->ns = remaining_ns;
  }
  else
    process_table->ns += ns;
  
  printf("\n");
  printf("next sec: %d\tns: %d\n", process_table->sec, process_table->ns);

  Time time_diff;
  time_diff.sec = sec;
  time_diff.ns = ns;

  return time_diff;
}

Time getClockTime() {
  Time time = {process_table->sec, process_table->ns};
  return time;
}

void initPCB(int table_index, int pid, int priority) {
  process_table->pcb_array[table_index].pid = pid;
  process_table->pcb_array[table_index].priority = priority;
}

int getPCBIndexByPid(int pid) {
  for (int i = 0; i < NUMBER_PCBS; i++) {
    if (process_table->pcb_array[i].pid == pid) {
      return i;
    }
  }
  fprintf(stderr, "oss: Warning: Pcb with pid specified was not found\n");
  return -1;
}

int getNextTableIndex() {
  for (int i = 0; i < NUMBER_PCBS; i++) {
    if(process_table->pcb_array[i].pid == -1) {
      return i;
    }
    printf("not -1 at i: %d, is: %d\n", i, process_table->pcb_array[i].pid);
  }
  return -1;
}

void resetPCB(int index) {
  process_table->pcb_array[index].pid = -1;
  process_table->pcb_array[index].priority = 0;

  process_table->pcb_array[index].total_time.sec = 0;
  process_table->pcb_array[index].total_time.ns = 0;

  process_table->pcb_array[index].last_burst_time.sec = 0;
  process_table->pcb_array[index].last_burst_time.ns = 0;

  process_table->pcb_array[index].arrival_time.sec = 0;
  process_table->pcb_array[index].arrival_time.ns = 0;

  process_table->pcb_array[index].wait_time.sec = 0;
  process_table->pcb_array[index].wait_time.ns = 0;
}