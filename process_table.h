#include "config.h"

// Process Control Block (PCB) - A fixed size structure and contains information to manage the child process scheduling. Will not need to allocate space to save the context of child processes. Must allocate space for scheduling-related items such as total CPU time used, total time in the system, time used during the last burst, your local simulated pid, and process priority, if any. It resides in shared memory and is accessible to the children. You should allocate space for up to 18 process control blocks. Also create a bit vector, local to oss, that will help you keep track of all the process control blocks (or process ID's), that are alreayd taken.
typedef struct {
  int sec;
  int ns;
} Time;


// Define the Process Control Block structure
struct ProcessControlBlock {
	int pid;
  int priority;

  Time total_time;
  Time last_burst_time;
  Time arrival_time;
  Time wait_time;

  // timeslice? what else?
};

// Process Table - Process Control Block for each of the user's processes and Information to manage child process scheduling
struct ProcessTable {
  int sec;
  int ns;

  int queueid;
  int blocked_queueid;

  int total_processes;
  Time total_wait_time;
  Time total_process_wait_time;
  Time total_time_in_system;
  Time total_cpu_time; // is same as system clock?
  Time total_idle_time;

  // array of 18 PCBs
  struct ProcessControlBlock pcb[18];
};

// functions for managing the process table
Time incrementClockRound();
Time addTimeToClock(int sec, int ns);
Time getClockTime();

// Helper Function ides
// int checkIfAnyRead();
// int checkIfProcessFull();
int isTableFull();