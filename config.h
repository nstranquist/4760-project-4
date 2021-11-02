
// Declare constants
#ifndef CONFIG_H
#define CONFIG_H

#define MAX_TIME 3 // 3 real-life seconds

#define MAX_PROCESSES 20 // 50

#define DEFAULT_TIME 2

#define LOGFILE_MAX_LINES 10000

#define MAX_MSG_SIZE 4096

#define NANOSECONDS 1000000000

#define MILISECONDS 1000

#define QUANTUM 10

#define NUMBER_PCBS 18

typedef struct {
  int sec;
  int ns;
} Time;

#endif