#include "config.h"

// Process Control Block (PCB) - A fixed size structure and contains information to manage the child process scheduling. Will not need to allocate space to save the context of child processes. Must allocate space for scheduling-related items such as total CPU time used, total time in the system, time used during the last burst, your local simulated pid, and process priority, if any. It resides in shared memory and is accessible to the children. You should allocate space for up to 18 process control blocks. Also create a bit vector, local to oss, that will help you keep track of all the process control blocks (or process ID's), that are alreayd taken.

// Define the Process Control Block structure
struct ProcessControlBlock {
	int pid;
	int priority;
	int total_cpu_time;
	int total_time_in_system;
	int time_in_current_burst;
	int time_remaining_in_current_burst;
};


// Process Table - Process Control Block for each of the user's processes and Information to manage child process scheduling
struct ProcessTable {
  int sec;
  int ms;

  int pid;

  // process control blocks
  int pcb_vector[MAX_PROCESSES];
  struct ProcessControlBlock *pcb; // current block

  // msg queues(?)

};

// functions for managing the process table
void addTimeToClock(int sec, int ms);
void incrementClock();