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
#include <sys/stat.h>
#include <sys/msg.h> // use the message queues
#include <getopt.h>

#include "config.h"

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

  // do more cleanup

}

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


  // Setup intterupt handler
  signal(SIGINT, myhandler);

  // Start program timer
  alarm(sleepTime);


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