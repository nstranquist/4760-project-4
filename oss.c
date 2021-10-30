#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/types.h>
// #include <sys/shm.h> // shouldn't need to use shared memory
#include <signal.h>
#include <sys/time.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h> // use the message queues
#include <getopt.h>

#include "config.h"
// #include "user.h"
#include "process_table.h"
#include "utils.h"

#define maxTimeBetweenNewProcsNS 20
#define maxTimeBetweenNewProcsSecs 20
#define DEFAULT_LOGFILE_NAME "oss.log"

// a constant representing the percentage of time a process launched an I/O-bound process or CPU-bound process. It should be weighted to generate more CPU-bound processes
#define CPU_BOUND_PROCESS_PERCENTAGE 0.75

static void myhandler(int signum) {
  if(signum == SIGINT) {
    // is ctrl-c interrupt
    perror("\nrunsim: Ctrl-C Interrupt Detected. Shutting down gracefully...\n");
  }
  else if(signum == SIGALRM) {
    // is timer interrupt
    perror("\nrunsim: Info: The time for this program has expired. Shutting down gracefully...\n");
  }
  else {
    perror("\nrunsim: Warning: Only Ctrl-C and Timer signal interrupts are being handled.\n");
    return; // ignore the interrupt, do not exit
  }

  // do more cleanup of sharedmem

}

// Use bitvector to keep track of the process control blocks (18), use to simulate Process Table

// Process Table declared
extern struct ProcessTable *process_table;
int shmid;

int main(int argc, char *argv[]) {
  printf("Hello world!\n");

  // Parse CLI args:
  // -h for help
  // -s t for max seconds before termination
  // -l f specify a particular name for the log file
  int option;
  int seconds = -1;
  int sleepTime;
  char *logfileName = NULL;
  int val;
  while((option = getopt(argc, argv, "hsl:")) != -1) {
    switch(option) {
      case 'h':
        printf("is help\n");
        break;
      case 's':
        // if(!atoi(optarg)) {
        //   perror("cant convert optarg");
        // }
        // val = atoi(optarg);
        // printf("%d", val);
        printf("is secs: %s\n", optarg);
        break;
      case 'l':
        printf("is logfile name: %s\n", optarg);
        logfileName = optarg;
        break;
      default:
        printf("default option reached\n");
        break; // abort()
    }
  }

  // get the non option arguments
  for(int i = optind; i < argc; i++) {
    printf("Non-option argument: %s\n", argv[i]);
  }

  if(seconds != -1) {
    printf("seconds exists: %d\n", seconds);
  }
  else {
    printf("seconds is undefined. setting to default\n");
    sleepTime = SLEEP_TIME;
  }

  // assign logfile. will overwrite previous files so that it's new each time the proram is run
  if(logfileName == NULL) {
    // use default
    logfileName = DEFAULT_LOGFILE_NAME;
  }

  // Test that logfile can be used
  FILE *logfile = fopen(logfileName, "w");
  if(logfile == NULL) {
    perror("oss: Error: Could not open log file for writing.\n");
    return 1;
  }

  // Setup intterupt handler
  signal(SIGINT, myhandler);

  // Start program timer
  alarm(sleepTime);

  // Instantiate ProcessTable with shared memory
  int shmid = shmget(IPC_PRIVATE, sizeof(struct ProcessTable), IPC_CREAT | 0666); // (struct ProcessTable)
  if (shmid == -1) {
    perror("oss: Error: Failed to create shared memory segment for process table\n");
    return -1;
  }

  process_table = (struct ProcessTable *)shmat(shmid, NULL, 0);
  if (process_table == (void *) -1) {
    perror("oss: Error: Failed to attach to shared memory\n");
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
      perror("oss: Error: Failed to remove memory segment\n");
    return -1;
  }

  

  // I) Running System Clock...
  // II) Create User Processes at Random Intervals
  //    - random int between 0 and 2 seconds....
  //      - sec [0,2), ms [0, INT_MAX)



  // 1. Setup the System Clock
  // 2. Get a time for the next (first) process to spawn
  // 3. Add time to clock until process spawns, which happens when the time for process is reached (or exceeded)
  // 4. Start the User Process with "execl"
  // 5. Get Time from Child process when it finishes (child randomly generates it)
  //    - do this through shared memory or message queue
  //    - add that time back to the system clock (sec/ms)
  // 6. 

  /**
   * Ready To Start Program Logic. Remember:
   * - system clock (sec, ms) should only be advanced by oss
  **/



  // Define and Attatch Shared Memory
    // allocate shmem for simulated system clock - 2 unsigned integers:
    //  - one stores seconds
    //  - one stores milliseconds

    // allote space for the process control block

  // Begin Process Forking: Main Logic Loop
  //  get a time in the future for when the first process will launch (check if process is active yet)

  //  if no processes are in "ready state" to run, the system should incrememnt the clock until it is time to launch a new process

  //  setup the new process: (will use random functions more than not)
  //    IF: Process Table is full
  //      skip the generation, determine another time to try and generate a new process
  //      log to log file that process table is full ("OSS: Process table is full at time t")

  //    ELSE: define/create the new process
  //      generates by allocating and initializing the process control block for the process
  //      forks the process
  
  //    generate a new time where it will launch a process, and schedule that process by sending a message to the Message Queue (check if I/O or CPU)

  //    wait for a message back from the process that it has finished its task (transfer control to the child process code)

  //  advance the system clock by 1.xx in each iteration of the loop
  //    xx is the number of nanoseconds. xx is a random number from [0, 1000] to simulate overhead for activity


  return 0;
}


// message queue


// handlers and interrupts
static int setupitimer(int sleepTime) {
  struct itimerval value;
  value.it_interval.tv_sec = 0;
  value.it_interval.tv_usec = 0;
  value.it_value.tv_sec = sleepTime; // alarm
  value.it_value.tv_usec = 0;
  return (setitimer(ITIMER_PROF, &value, NULL));
}

static int setupinterrupt(void) {
  struct sigaction act;
  act.sa_handler = myhandler;
  act.sa_flags = 0;
  return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL) || sigaction(SIGALRM, &act, NULL));
}

static int timerHandler(int s) {
  int errsave;
  errsave = errno;
  write(STDERR_FILENO, "The time limit was reached\n", 1);
  errno = errsave;
}


// Helper Function ides

// int checkIfAnyRead();

// int checkIfProcessFull();


void destoryMemory() {

}