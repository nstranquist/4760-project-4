#define _GNU_SOURCE  // for asprintf
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>
// #include <sys/shm.h> // shouldn't need to use shared memory
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h> // use the message queues
#include <getopt.h>
#include <stdarg.h>

#include "config.h"
// #include "user.h"
#include "process_table.h"
#include "utils.h"
#include "queue.h"

#define maxTimeBetweenNewProcsNS 20
#define maxTimeBetweenNewProcsSecs 20
#define DEFAULT_LOGFILE_NAME "oss.log"
#define BLOCK_READY_OVERHEAD 10 // ms to add as overhead for moving process from blocked to ready

// a constant representing the percentage of time a process launched an I/O-bound process or CPU-bound process. It should be weighted to generate more CPU-bound processes
#define CPU_BOUND_PROCESS_PERCENTAGE 0.75

#define PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)


// Make a Blocked Queue of processes that are blocked
// - oss checks this every time it makes a decision on scheduling to see if it should put the processes back in the ready queue
// - when putting processes back in the reqdy queue, increment clock by BLOCK_READY_OVERHEAD amount

void generateUserProcessInterval();
int detachandremove(int shmid, void *shmaddr);
void logmsg(const char *msg);
int waitTimeIsUp();
void generateReport();
void cleanup();
char* format_string(char*msg, int data);

// Use bitvector to keep track of the process control blocks (18), use to simulate Process Table

extern struct ProcessTable *process_table;
int shmid;
int queueid;

int next_sec = 0;
int next_ns = 0;

char *logfileName = NULL;

static void myhandler(int signum) {
  // is ctrl-c interrupt
  if(signum == SIGINT)
    perror("\noss: Ctrl-C Interrupt Detected. Shutting down gracefully...\n");
  // is timer interrupt
  else if(signum == SIGALRM)
    perror("\noss: Info: The time for this program has expired. Shutting down gracefully...\n");
  else {
    perror("\noss: Warning: Only Ctrl-C and Timer signal interrupts are being handled.\n");
    return; // ignore the interrupt, do not exit
  }

  cleanup();
  
  pid_t group_id = getpgrp();
  if(group_id < 0)
    perror("oss: Info: group id not found\n");
  else
    killpg(group_id, signum);


  kill(getpid(), SIGKILL);
	exit(0);
  signal(SIGQUIT, SIG_IGN);
}
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

int main(int argc, char *argv[]) {
  // Parse CLI args:
  // -h for help
  // -s t for max seconds before termination
  // -l f specify a particular name for the log file
  int option;
  int seconds = -1;
  int val;
  while((option = getopt(argc, argv, "hs:l:")) != -1) {
    switch(option) {
      case 'h':
        printf("is help\n");
        break;
      case 's':
        if(!atoi(optarg)) {
          perror("oss: Error: Cant convert seconds to number");
          return 1;
        }
        seconds = atoi(optarg);
        if(seconds < 0) {
          perror("oss: Error: Seconds cannot be less than 0\n");
          return 1;
        }
        printf("is secs: %d\n", seconds);
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


  if(seconds == -1) {
    seconds = MAX_TIME;
  }

  // assign logfile. will overwrite previous files so that it's new each time the proram is run
  if(logfileName == NULL) {
    // use default
    logfileName = DEFAULT_LOGFILE_NAME;
  }

  // Test that logfile can be used
  FILE *fp = fopen(logfileName, "w");
  if(fp == NULL) {
    perror("oss: Error: Could not open log file for writing.\n");
    return 1;
  }
  fprintf(fp, "Log Info for OSS Program:\n"); // clear the logfile to start
  fclose(fp);

  // seed the random function
  srand(time(NULL) + getpid());

  // Set up timers and interrupt handler
  if (setupinterrupt() == -1) {
    perror("oss: Error: Could not run setup the interrupt handler.\n");
    return -1;
  }
  if (setupitimer(seconds) == -1) {
    perror("oss: Error: Could not setup the interval timer.\n");
    return -1;
  }

  // Setup intterupt handler
  signal(SIGINT, myhandler);

  // Start program timer
  alarm(seconds);

  // Instantiate ProcessTable with shared memory (allocate and attach)
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

  // Initialize Message Queue
  if(initqueue(IPC_PRIVATE) == -1) {
    perror("oss: Error: Failed to initialize message queue\n");
    
    cleanup();

    return -1;
  }

  // Start Process Loop
  while(process_table->total_processes < MAX_PROCESSES) {
    printf("\n");
    printf("process #%d\n", process_table->total_processes);

    // Generate Next Time for child - rand: sec [0,2), ns[0, INT_MAX)
    if(waitTimeIsUp() == 0) {
      // cpu in idle state, waiting for next process
      printf("oss in idle state\n");
      process_table->total_processes++; // just for now

      incrementClockRound(1);
    }
    else {
      // ready for next process
      printf("ready for next process\n");

      // setup the next process
      int is_table_full = 1;
      if(is_table_full == 0) {
        printf("process table is full\n");
        //      skip the generation, determine another time to try and generate a new process
        //      log to log file that process table is full ("OSS: Process table is full at time t")
        char *msg = format_string("OSS: Process table is full at time: ", process_table->sec);
        strcat(msg, ":");
        msg = format_string(msg, process_table->ns);
        logmsg(msg);

        cleanup();
        continue;
      }

      printf("table not full. forking\n");


      // Fork, then return
      pid_t child_pid = fork();
      if (child_pid == -1) {
        perror("oss: Error: Failed to fork a child process\n");
        cleanup();
        return -1;
      }

      if (child_pid == 0) {
        // attach memory again as child
        process_table = (struct ProcessTable *)shmat(shmid, NULL, 0);

        printf("in child\n");


        // execl
        execl("./user", "./user", (char*)NULL); // no args
        perror("oss: Error: Child failed to execl\n");
        exit(0);
      }
      else {
        // in parent
        // int status;
        // pid_t wpid = waitpid(child_pid, &status, 0);
        pid_t wpid = wait(NULL);
        if (wpid == -1) {
          perror("runsim: Error: Failed to wait for child\n");
          
        }
        // child has finished. generate next time
        printf("exited child\n");
      }

      // increment total processes that have ran
      process_table->total_processes++;
      
      incrementClockRound(0);

      //  setup the new process: (will use random functions more than not)
      //    IF: Process Table is full
      //      skip the generation, determine another time to try and generate a new process
      //      log to log file that process table is full ("OSS: Process table is full at time t")

      //    ELSE: define/create the new process
      //      generates by allocating and initializing the process control block for the process
      //      forks the process
      
      //    generate a new time where it will launch a process, and schedule that process by sending a message to the Message Queue (check if I/O or CPU)

      //    wait for a message back from the process that it has finished its task (transfer control to the child process code)
    }

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



  // End Program if:
  // 1. >3 seconds have passed
  // 2. >50 processes have been generated

  // On Program End:
  //    Generate a report with the info:
  //    - avg wait time, avg time in system, avg cpu time, avg time a process waited in blocked queue for each type of process (cpu or io)
  //    - total time cpu spent in idle (waiting)
  generateReport();

  // Gracefully Shutdown
  // - remove shared memory
  // - remove message queue(s)
  cleanup();


  return 0;
}

void cleanup() {
  process_table = (struct ProcessTable *)shmat(shmid, NULL, 0);
  if(detachandremove(shmid, process_table) == -1) {
    perror("oss: Error: Failure to detach and remove memory\n");
  }
  else printf("success detatch\n");
  if(remmsgqueue() == -1) {
    perror("oss: Error: Failed to remove message queue");
  }
  else printf("success remove msgqueue\n");
}

// From textbook
int detachandremove(int shmid, void *shmaddr) {
  int error = 0;

  if (shmdt(shmaddr) == -1) {
    fprintf(stderr, "oss: Error: Can't detach memory\n");
    error = errno;
  }
  
  if ((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
    fprintf(stderr, "oss: Error: Can't remove shared memory\n");
    error = errno;
  }

  if (!error)
    return 0;

  errno = error;

  return -1;
}



// Helper Functions:
int waitTimeIsUp() {
  // compare next_sec and next_ns with what's in the process table
  if(next_sec <= process_table->sec) {
    if(next_ns <= process_table->ns) {
      return 1;
    }
  }

  return 0; // 0 means not
}

void generateUserProcessInterval() {
  int rand_sec = getRandom(2);
  int rand_ns = getRandom(INT_MAX);

  // set ints
  next_sec = rand_sec;
  next_ns = rand_ns;
}

// log program results to file
void generateReport() {
  if(process_table->total_processes == 0) {
    perror("oss: Warning: total processes was 0, skipping the report\n");
    return;
  }

  int average_wait = process_table->total_wait_time / process_table->total_processes;
  int average_time_in_system = process_table->total_time_in_system / process_table->total_processes;
  int average_cpu_time = process_table->total_cpu_time / process_table->total_processes;
  int average_process_wait = process_table->total_wait_time / process_table->total_processes;

  int report[4] = {average_wait, average_time_in_system, average_cpu_time, average_process_wait};
  char report_str[4][25] = {
    "Average Wait: ",
    "Average Time in System:",
    "Average CPU Time: ",
    "Average Process Wait: "
  };

  logmsg("\nFinal Report:");
  
  char *report_info;

  for(int i=0; i<4; i++) {
    char buf[100];
    report_info = format_string(report_str[i], report[i]);
    logmsg(report_info);
  }

  // calculate total idle time
  char *idle_info;
  char *total_idle_time_str = "Total Idle Time: ";
  if (asprintf(&idle_info, "sec: %d\tns: %d", process_table->sec, process_table->ns) == -1) {
    perror("oss: Warning: asprintf failed\n");
  } else {
    char buf[100];
    strcat(strcpy(buf, total_idle_time_str), idle_info);
    printf("%s\n", buf);
    logmsg(buf);
    free(idle_info);
  }
}

void logmsg(const char *msg) {
  FILE *fp = fopen(logfileName, "a+");
  if(fp == NULL) {
    perror("oss: Error: Could not use log file.\n");
    return;
  }

  // Check if lines in the file >10,000
  int linecount = 0;
  char c;
  while(1) {
    if(feof(fp))
      break;
    c = fgetc(fp);
    if(c == '\n')
      linecount++;
  }

  // printf("file line count: %d\n", linecount);

  if(linecount > LOGFILE_MAX_LINES) {
    perror("oss: Error: logfile has exceeded max lines\n");
    fclose(fp);
    return;
  }

  fprintf(fp, "%s\n", msg);
  fclose(fp);
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