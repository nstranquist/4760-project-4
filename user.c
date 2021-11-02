// Simulates the User Process
#define _GNU_SOURCE  // for asprintf
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/msg.h>
#include <string.h>
#include "user.h"
#include "utils.h"
#include "process_table.h"
#include "queue.h"

#define TERMINATE_PROBABILITY 0.1

#define CPU_BLOCK_PROBABILITY 0.02

#define IO_BLOCK_PROBABILITY 0.75

#define MAXSIZE 4096

mymsg_t mymsg;
extern struct ProcessTable *process_table;
int size;
int shmid;

char* format_string(char*msg, int data);

int main(int argc, char *argv[]) {
  // no args, proceed to process
  printf("In user process!\n");

  if(argc != 2) {
    fprintf(stderr, "oss: Error: invalid usage for user process. Too many or too few args\n");
    return 1;
  }

  srand(time(NULL) + getpid()); // re-seed the random

  // get shmid from passed arguments
  shmid = atoi(argv[1]);

  printf("shmid: %d\n", shmid);

  // attach shared memory
  process_table = (struct ProcessTable *)shmat(shmid, NULL, 0);
  if (process_table == (void *) -1) {
    perror("oss: Error: Failed to attach to shared memory\n");
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
      perror("oss: Error: Failed to remove memory segment\n");
    return -1;
  }

  fprintf(stderr, "finished attaching process table\n");

  // Wait for timeslice message 
  if((size = msgrcv(process_table->queueid, &mymsg, MAXSIZE, 0, 0)) == -1) {
    perror("oss: Error: could not receive message\n");
    return 0;
  }
  else fprintf(stderr, "finished getting msg\n");

  printf("\nmsg pid: %d\n", mymsg.pid);
  printf("msg timeslice: %d\n\n", mymsg.timeslice);

  // get timeslice from message queue
  int timeslice = mymsg.timeslice;

  // parse msg (util str_slice)
  int time_used = timeslice;
  if(timeslice > 0)
    time_used = getRandom(timeslice);

  // send message back to oss
  if((size = msgwrite(mymsg.mtext, size + 1, mymsg.mtype, process_table->queueid, mymsg.pid, time_used)) == -1) {
    perror("oss: Error: could not send message\n");
    return 1;
  }
  else {
    printf("sent message back to oss\n");
  }

  return 0;

  // use TERMINATE_PROBABILITY to determine if program will terminate
  double terminate_temp = (double)rand() / RAND_MAX;
  if(terminate_temp <= TERMINATE_PROBABILITY) {
    printf("Will terminate\n");

    // use random amount of its timeslice before terminating (no sleep delay)
    printf("using %d ns time of timeslice\n", time_used);

    // tell oss (send msg) it has terminated and how much of timeslice was used
    // 1. update sharedmem values
    // 2. send message back to parent
    char buf[MAXSIZE] = "DISPATCH-PROCESS-TERMINATED-";
    format_string(buf, time_used);
    strcat(buf, "-_");
    msgwrite(format_string, 52, mymsg.mtype, process_table->queueid, mymsg.pid, time_used);

    return 0;
  }

  printf("Won't terminate\n");

  printf("Program type: %ld\n", mymsg.mtype);

  // Get random number to determine if will use entire timeslice or get blocked by event
  double probability_temp = (double)rand() / RAND_MAX;
  if(mymsg.mtype == 1 && probability_temp <= CPU_BLOCK_PROBABILITY || mymsg.mtype == 2 && probability_temp <= IO_BLOCK_PROBABILITY) {
    printf("Is Blocked. Generating r,s then putting in blocked queue with a message\n");

    // get r [0,5] and s[0,1000] for the sec / ns
    int r = getRandom(6);
    int s = getRandom(1001);

    // send message to blocked queue with a message
    char *buf = "DISPATCH-PROCESS-BLOCKED-";
    buf = format_string(buf, r);
    strcat(buf, "-");
    buf = format_string(buf, s);

    msgwrite(buf, 35, mymsg.mtype, process_table->queueid, mymsg.pid, time_used);
  }
  else {
    printf("Is not blocked. Will tell oss and give timeslice\n");
    
    char *buf = "DISPATCH-PROCESS-FINISHED-";
    buf = format_string(buf, timeslice);
    strcat(buf, "-_");
    msgwrite(buf, 40, mymsg.mtype, process_table->queueid, mymsg.pid, time_used);
  }
  
  return 0;
}


char* format_string(char*msg, int data) {
  char *temp;
  char *buf;
  if (asprintf(&temp, "%d", data) == -1) {
    perror("oss: Warning: string format failed\n");
    return "";
  } else {
    strcat(strcpy(buf, msg), temp);
    printf("%s\n", buf);
    free(temp);
    return buf;
  }
}