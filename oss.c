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
#include <assert.h>
#include <stdarg.h>
#include <math.h>

#include "config.h"
// #include "user.h"
#include "process_table.h"
#include "utils.h"
#include "queue.h"
#include "circular_queue.h"

#define maxTimeBetweenNewProcsNS 1000000000 // nanoseconds/s, to ensure average ~1s between executions
#define maxTimeBetweenNewProcsSecs 2
#define DEFAULT_LOGFILE_NAME "oss.log"
#define BLOCK_READY_OVERHEAD 10 // ms to add as overhead for moving process from blocked to ready

// a constant representing the percentage of time a process launched an I/O-bound process or CPU-bound process. It should be weighted to generate more CPU-bound processes
#define CPU_BOUND_PROCESS_PERCENTAGE 75
#define MAXLINE 1024
#define MAX_MSG_SIZE 4096

#define HIGH_PRIORITY_PERCENTAGE 50

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
int getNextPid();
void freePid(int pid);
int getPriority();
void init_pids();
void generate_next_time();
Time getOverheadTime();
// char** parseUserMessage(char *msg);

// Use available_pids to keep track of the process control blocks (18), use to simulate Process Table

extern struct ProcessTable *process_table;
mymsg_t mymsg;

Queue ready_queue;
Queue blocked_queue;
Queue low_priority_queue;
Queue high_priority_queue;
// Queue *terminated_queue;

int shmid;

int next_sec = 0;
int next_ns = 0;

char *logfileName = NULL;

// will be used to keep track of process id's already taken. not a queue
int available_pids[50];

// oss will select process to run, then schedule it for execution
// - select with round robin with some priority
// - ready queue will not be FIFO, will use priority scheduling that looks at age of processes, time process has been waiting
// - 2-level feedback queue: "high priority queue" and "low priority queue"
//    - attempt to put I/O bound in high priority queue
//    - attempt to put CPU bound in low priority queue
//    idea: allocate equally between low/high queues to start

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

  fprintf(stderr, "interrupt handler\n");

  generateReport();

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
  int result;
  // msg values
  char **msg_tokens;
  const char delim = '-'; // char used to split message text for data
  char test_msg_text[] = "DISPATCH-PROCESS-BLOCKED-20-500"; // test message to use until messages work
  // message data info:
  int msg_action; // token[0] - action: dispatch/receive, "0" / "1"
  int msg_pid; // token[1] - process / pid
  int msg_is_blocked; // token[2] - blocked or empty / != "BLOCKED", "0" / "1"
  int msg_sec; // token[3] - timeslice in seconds
  int msg_ns; // token[4] - timeslice in nanoseconds

  while((option = getopt(argc, argv, "hs:l:")) != -1) {
    switch(option) {
      case 'h':
        printf("\nProgram Help:\n");
        printf("Usage: ./oss [-h] [-s t] [-l f]\n");
        printf("Where h is help, t is the time in seconds, and f is the logfile name\n");
        printf("If none are specified, it will resort to the defaults: t=50 seconds and f=oss.log\n\n");
        return 0;
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
        break;
      case 'l':
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
  shmid = shmget(IPC_PRIVATE, sizeof(struct ProcessTable), IPC_CREAT | 0666); // (struct ProcessTable)
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
  int queueid = initqueue(IPC_PRIVATE);
  if(queueid == -1) {
    perror("oss: Error: Failed to initialize message queue\n");
    cleanup();
    return -1;
  }
  process_table->queueid = queueid;

  // Initialize Scheduling Queues
  ready_queue = init_circular_queue();
  blocked_queue = init_circular_queue();
  low_priority_queue = init_circular_queue();
  high_priority_queue = init_circular_queue();

  init_pids();

  // fill process table
  init_process_table_pcbs();

  generate_next_time();

  // Start Process Loop
  while(process_table->total_processes < MAX_PROCESSES) {
    Time round_diff = incrementClockRound();

    if(waitTimeIsUp() == -1) {
      // cpu in idle state, skip until time for next process to be scheduled
      printf("oss in idle state\n");

      // add time_diff to total_idle_time
      process_table->total_idle_time.sec += round_diff.sec;
      process_table->total_idle_time.ns += round_diff.ns;

      continue;
    }

    printf("next sec: %d, next ns: %d\n", next_sec, next_ns);

    // schedule the process into ready queue
    int next_pid;
    if(process_table->total_processes < MAX_PROCESSES) {
      next_pid = getNextPid();
      if(next_pid == -1) {
        perror("oss: Error: Failed to get next pid\n");
      }
      printf("scheduling pid: %d\n", next_pid);
      printf("queue size before: %d\n", ready_queue.size);
      int result = enqueue(&ready_queue, next_pid);
      if(result == 0) {
        printf("queue is full. skipping the generation\n");
        process_table->total_processes--;
      }
      printf("queue size after: %d\n", ready_queue.size);
      process_table->total_processes++;

      // add message to log that next_pid was added to ready queue at time sec:ns
      char *msg_text = malloc(sizeof(char) * MAX_MSG_SIZE);
      sprintf(msg_text, "OSS: Generating process with PID %d and putting it in ready queue at time %d:%d", next_pid, process_table->sec, process_table->ns);
      logmsg(msg_text);
    }
    else {
      printf("max processes has been reached\n");
      continue;
    }

    generate_next_time();

    // ready for next process
    printf("ready for next process\n");

    // setup the next process
    int next_table_index = getNextTableIndex();
    printf("next table index: %d\n", next_table_index);
    if(next_table_index == -1) {
      printf("process table is full\n");
      //      skip the generation, determine another time to try and generate a new process
      //      log to log file that process table is full ("OSS: Process table is full at time t")
      char *msg;
      asprintf(&msg, "OSS: Process table is full at time %d:%d", process_table->sec, process_table->ns);
      logmsg(msg);

      // // add time_diff to total_wait_time
      // process_table->total_wait_time.sec += round_diff.sec;
      // process_table->total_wait_time.ns += round_diff.ns;
      continue;
    }
    else
      printf("table not full\n");

    // initialize the process control block
    int ready_pid = dequeue(ready_queue);
    if(ready_pid == -1) {
      printf("ready queue is empty\n");
      // log that ready queue is empty to logfile
      char *msg;
      asprintf(&msg, "OSS: Ready queue is empty at time %d:%d", process_table->sec, process_table->ns);
      logmsg(msg);

      // add time_diff to idle_time
      process_table->total_idle_time.sec += round_diff.sec;
      process_table->total_idle_time.ns += round_diff.ns;
      continue;
    }
    printf("initializing pcb for pid: %d\n", ready_pid);

    double process_type_temp = (rand() % 100) + 1;
    int process_type;
    if(process_type < CPU_BOUND_PROCESS_PERCENTAGE) {
      printf("is cpu process\n");
      // process_table->pcb->type = 1;
      process_type = 1;
    }
    else {
      printf("is io process\n");
      // process_table->pcb->type = 2;
      process_type = 2;
    }

    int next_priority = getPriority();
    initPCB(next_table_index, ready_pid, next_priority);

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

      char *shmid_str;
      asprintf(&shmid_str, "%i", shmid);

      // execl
      execl("./user", "./user", shmid_str, (char *) NULL); // 1 arg: pass shmid
      perror("oss: Error: Child failed to execl");
      cleanup();
      exit(0);
    }
    else {
      // in parent

      // send message to queue
      char *buf_str;
      asprintf(&buf_str, "DISPATCH-%d-_-%d-%d", child_pid, process_table->sec, process_table->ns);

      // get size of buf_str
      int buf_size = strlen(buf_str);

      // send message to queue
      if(msgwrite(buf_str, buf_size, process_type, process_table->queueid, ready_pid) == -1) {
        perror("oss: Error: Failed to send message to queue\n");
        cleanup();
        return -1;
      }
      else {
        printf("sent message to queue\n");
      }

      // add time for overhead of context switching
      Time overhead = getOverheadTime();

      // add overhead time to process_table
      process_table->sec = process_table->sec + overhead.sec;
      process_table->ns = process_table->ns + overhead.ns;

      // declare string
      char msg[120];
      snprintf(msg, sizeof(msg), "OSS: Process %d Dispatched at time: %d:%d", ready_pid, process_table->sec, process_table->ns);
      fprintf(stderr, "msg: %s\n", msg);
      logmsg(msg);

      // TODO: make WNOHANG?
      pid_t wpid = wait(NULL);
      if (wpid == -1) {
        perror("oss: Error: Failed to wait for child\n");
        cleanup();
        return 1;
      }

      fprintf(stderr, "out of msgwrite. queueid: %d\n", process_table->queueid);

      int msg_size = msgrcv(process_table->queueid, &mymsg, MAX_MSG_SIZE, 0, 0);
      if(msg_size == -1) {
        perror("oss: Error: Could not receive message from child\n");
        cleanup();
        return 1;
      }

      // cleanup process control table
      process_table->pcb_array[next_table_index].pid = -1;
      process_table->pcb_array[next_table_index].priority = 0;

      // TODO: add timeslice to mymsg, parse here


      // if terminated, or finished, then take stats and add to totals, then remove from table

      // parse message result (using '-' as delimiter)
      printf("message data: %s, message type: %ld\n", mymsg.mtext, mymsg.mtype);
      int msg_action; // token[0] - action: dispatch/receive, "0" / "1"
      int msg_pid; // token[1] - process / pid
      int msg_is_blocked; // token[2] - blocked or empty / != "BLOCKED", "0" / "1"
      int msg_sec; // token[3] - timeslice in seconds
      int msg_ns; // token[4] - timeslice in nanoseconds
      char *msg_action_str = strtok(mymsg.mtext, "-");
      char *msg_pid_str = strtok(NULL, "-");
      char *msg_is_blocked_str = strtok(NULL, "-");
      char *msg_sec_str = strtok(NULL, "-");
      char *msg_ns_str = strtok(NULL, "-");

      printf("Parsed msg tokens:\n");
      printf("strings:\nmsg_action: %s msg_pid: %s msg_is_blocked: %s msg_sec: %s msg_ns: %s\n", msg_action_str, msg_pid_str, msg_is_blocked_str, msg_sec_str, msg_ns_str);

      // convert int data from message
      if(msg_action_str != NULL && msg_action_str != "DISPATCH")
        msg_action = 1;
      else
        msg_action = 0;

      if(msg_pid_str != NULL) {
        msg_pid = atoi(msg_pid_str);
        printf("msg_pid: %d\n", msg_pid);
      }

      if(msg_is_blocked_str != NULL && msg_is_blocked_str != "BLOCKED")
        msg_is_blocked = 1;
      else
        msg_is_blocked = 0;

      if(msg_sec_str != NULL) {
        msg_sec = atoi(msg_sec_str);
        printf("msg_sec: %d\n", msg_sec);
      }

      if(msg_ns_str != NULL) {
        msg_ns = atoi(msg_ns_str);
        printf("msg_ns: %d\n", msg_ns);
      }

      // print int results
      printf("msg int results:\nmsg_action: %d msg_pid: %d pid: %d msg_is_blocked: %d msg_sec: %d msg_ns: %d\n", msg_action, msg_pid, mymsg.pid, msg_is_blocked, msg_sec, msg_ns);

      // add time difference to clock
      process_table->sec = process_table->sec + msg_sec;
      process_table->ns = process_table->ns + msg_ns;

      // log message
      char results_int_msg[120];
      if(msg_is_blocked == 0) {
        snprintf(results_int_msg, sizeof(results_int_msg), "OSS: Process %d Blocked at time: %d:%d", mymsg.pid, process_table->sec, process_table->ns);
      }
      else {
        snprintf(results_int_msg, sizeof(results_int_msg), "OSS: Process %d Terminated at time: %d:%d", mymsg.pid, process_table->sec, process_table->ns);
        // if terminated, reset the available_pids at index
        freePid(mymsg.pid);
        // remove PCB from process table
        int pcb_index = getPCBIndexByPid(mymsg.pid);
        resetPCB(pcb_index);
      }
      logmsg(results_int_msg);
    }
  }

  // Wait for all children to finish, after the main loop is complete
  while(wait(NULL) > 0) {
    printf("oss: Info: Waiting for all children to finish...\n");
  }

  // End Program if:
  // 1. >3 seconds have passed
  // 2. >50 processes have been generated

  // On Program End:
  //    Generate a report with the info:
  //    - avg wait time, avg time in system, avg cpu time, avg time a process waited in blocked queue for each type of process (cpu or io)
  //    - total time cpu spent in idle (waiting)
  fprintf(stderr, "program ending. total processes: %d\n", process_table->total_processes);

  generateReport();

  // Gracefully Shutdown
  // - remove shared memory
  // - remove message queue(s)
  cleanup();

  return 0;
}

void priority() {
  // priority scheduler for round robin
  // - age of process
  // - time the process has been waiting for since last cpu burst


}

void cleanup() {
  if(remmsgqueue(process_table->queueid) == -1) {
    perror("oss: Error: Failed to remove message queue");
  }
  else printf("success remove msgqueue\n");
  if(detachandremove(shmid, process_table) == -1) {
    perror("oss: Error: Failure to detach and remove memory\n");
  }
  else printf("success detatch\n");
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
  if(next_sec < process_table->sec) {
    return 0;
  }
  if(next_sec == process_table->sec) {
    if(next_ns < process_table->ns) {
      return 0;
    }
  }

  return -1; // -1 means not
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

  int average_wait_sec = process_table->total_wait_time.sec / process_table->total_processes;
  int average_wait_ns = process_table->total_wait_time.ns / process_table->total_processes;
  int average_time_in_system_sec = process_table->total_time_in_system.sec / process_table->total_processes;
  int average_time_in_system_ns = process_table->total_time_in_system.ns / process_table->total_processes;
  int average_cpu_time_sec = process_table->total_cpu_time.sec / process_table->total_processes;
  int average_cpu_time_ns = process_table->total_cpu_time.ns / process_table->total_processes;
  int average_process_wait_sec = process_table->total_wait_time.sec / process_table->total_processes;
  int average_process_wait_ns = process_table->total_wait_time.ns / process_table->total_processes;

  char avg_wait_str[100];
  char avg_time_in_system_str[100];
  char avg_cpu_time_str[100];
  char avg_process_wait_str[100];
  char total_idle_time_str[100];

  snprintf(avg_wait_str, sizeof(avg_wait_str), "Average Wait: %d:%d", average_wait_sec, average_wait_ns);
  snprintf(avg_time_in_system_str, sizeof(avg_time_in_system_str), "Average Time in System: %d:%d", average_time_in_system_sec, average_time_in_system_ns);
  snprintf(avg_cpu_time_str, sizeof(avg_cpu_time_str), "Average CPU Time: %d:%d", average_cpu_time_sec, average_cpu_time_ns);
  snprintf(avg_process_wait_str, sizeof(avg_process_wait_str), "Average Process Wait: %d:%d", average_process_wait_sec, average_process_wait_ns);
  snprintf(total_idle_time_str, sizeof(total_idle_time_str), "Total Idle Time: %d:%d", process_table->sec, process_table->ns);

  logmsg("\nFinal Report:");
  logmsg(avg_wait_str);
  logmsg(avg_time_in_system_str);
  logmsg(avg_cpu_time_str);
  logmsg(avg_process_wait_str);
  logmsg(total_idle_time_str);
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

int getNextPid() {
  for(int i=0; i<18; i++) {
    if(available_pids[i] == -1) {
      available_pids[i] = i;
      return i;
    }
  }
  return -1;
}

void freePid(int pid) {
  for(int i=0; i<18; i++) {
    if(available_pids[i] == pid) {
      available_pids[i] = -1;
      return;
    }
  }
}

int getPriority() {
  // get priority from low/high priority queue
  return 1;
}

void init_pids() {
  for(int i=0; i<50; i++) {
    available_pids[i] = -1;
  }
}

void generate_next_time() {
  // Generate Next Time for child - 0-2 secs, should be avg 1. second overall
  int next_sec_diff = getRandom(maxTimeBetweenNewProcsSecs);
  int next_ns_diff = getRandom(maxTimeBetweenNewProcsNS);

  next_sec = process_table->sec + next_sec_diff;
  next_ns = process_table->ns + next_ns_diff;
}

Time getOverheadTime() {
  Time overhead_time;
  overhead_time.sec = 0;
  int rand_ns = getRandom(1000);
  overhead_time.ns = rand_ns;
  return overhead_time;
}