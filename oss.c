#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <getopt.h>

#include "config.h"

// Setup Interrupt Handler

// Setup timer handler


int main(int argc, char *argv[]) {
  printf("Hello world!\n");

  // Parse CLI args:
  // -h for help
  // -s t for max seconds before termination
  // -l f specify a particular name for the log file
  int option;
  int seconds;
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


  return 0;
}
