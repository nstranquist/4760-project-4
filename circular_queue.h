#define CAPACITY 100

typedef struct {
  int front;
  int rear;
  int size;
  int queue[CAPACITY];
} Queue;

Queue init_circular_queue();
int enqueue(Queue *q, int pid);
int dequeue(Queue q);
int isFull(Queue q);
int isEmpty(Queue q);
int getRear(Queue q);
int getFront(Queue q);