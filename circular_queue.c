// help from: https://codeforwin.org/2018/08/queue-implementation-using-array-in-c.html
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "circular_queue.h"


Queue init_circular_queue() {
  Queue q;
  q.front = 0;
  q.rear = CAPACITY - 1;
  q.size = 0;
  // q.queue should already be good
  return q;
}

int enqueue(Queue *q, int pid) {
  if (isFull(*q)) {
    printf("Queue is full\n");
    return 0;
  }
  q->rear = (q->rear + 1) % CAPACITY;
  q->size++;
  q->queue[q->rear] = pid;
  return 1;
}

int dequeue(Queue q) {
  int data;
  if (isEmpty(q)) {
    printf("Queue is empty\n");
    return -1;
  }

  data = q.queue[q.front];
  q.front = (q.front + 1) % CAPACITY;
  q.size--;
  return data;
}

int isFull(Queue q) {
  return q.size == CAPACITY;
}

int isEmpty(Queue q) {
  return q.size == 0;
}

int getFront(Queue q) {
  if (isEmpty(q)) {
    printf("Queue is empty\n");
    return INT_MIN;
  }
  return q.queue[q.front];
}

int getRear(Queue q) {
  if (isEmpty(q)) {
    printf("Queue is empty\n");
    return INT_MIN;
  }
  return q.queue[q.rear];
}