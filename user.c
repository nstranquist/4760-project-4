// Simulates the User Process

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/msg.h>
#include "user.h"
#include "utils.h"

#define TERMINATE_PROBABILITY 0.1

#define CPU_BLOCK_PROBABILITY 0.02

#define IO_BLOCK_PROBABILITY 0.75

int main(int argc, char *argv[]) {
  // no args, proceed to process
  printf("In user process!\n");

  srand(time(NULL) + getpid()); // re-seed the random

  sleep(2);

  return 0;


  // re-attach memory for message queue and process control block?

  // Get Timeslice from message queue


  
  // use TERMINATE_PROBABILITY to determine if program will terminate
  double terminate_temp = (double)rand() / RAND_MAX;
  if(terminate_temp <= TERMINATE_PROBABILITY) {
    printf("Will terminate\n");

    // use random amount of its timeslice before terminating (no sleep delay)

    // tell oss (send msg) it has terminated and how much of timeslice was used


    return 0;
  }

  printf("Won't terminate\n");

  // Get type of program (CPU or I/O) from PCB in sharedmem
  int program_type = 0; // for CPU

  printf("Program type: %d\n", program_type);

  // Get random number to determine if will use entire timeslice or get blocked by event
  double probability_temp = (double)rand() / RAND_MAX;
  if(program_type == 0 && probability_temp <= CPU_BLOCK_PROBABILITY || program_type == 1 && probability_temp <= IO_BLOCK_PROBABILITY) {
    printf("Is Blocked. Generating r,s then putting in blocked queue with a message\n");

    // get r [0,5] and s[0,1000] for the sec / ns


    // send message to queue

  }
  
  return 0;
}


// Get mesage from queue (from parent)


// Send message to queue (to parent)