#ifndef QUEUE_H
#define QUEUE_H

#define MAX_QUEUE_SIZE 1000

typedef struct node {
    int start_address;
    int end_address;
} Node;

typedef struct queue {
    int front;  //index of the first element
    int rear; //index of the last element
    int size;   //node size of the queue
    int capacity; //max size of the queue
    Node nodes[MAX_QUEUE_SIZE];   //array of nodes
} Queue;

//initialize queue
void initQueue(Queue *queue, int size);
//add element to the queue
int enqueue(Queue* queue, int dataStart, int dataEnd);
//remove element from the queue
int dequeue(Queue* queue, int *dataStart, int *dataEnd);
//check if the queue is empty
int is_empty(Queue* queue);

#endif 