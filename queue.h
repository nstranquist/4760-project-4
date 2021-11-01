
#define MAX_MSG_SIZE 4096

typedef struct mymsg_t {
  long mtype;
  char mtext[MAX_MSG_SIZE];
} mymsg_t;

int remmsgqueue(int queueid);
int msgwrite(void *buf, int len, int msg_type, int queueid);
int msgprintf(char *fmt, int type, int queueid, ...);
int initqueue(int key);
