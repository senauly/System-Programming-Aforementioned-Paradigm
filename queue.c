#include "queue.h"
#include <stdio.h>

void initQueue(Queue *queue, int size){
    queue->size = 0;
    queue->front = 0;
    queue->rear = 0;
    queue->capacity = MAX_QUEUE_SIZE;
}

int enqueue(Queue* queue, int dataStart, int dataEnd){
    printf("rear %d\n", queue->rear);
    
    if(queue->rear == queue->capacity){
        queue->rear = 0;
    }

    if(queue->size >= queue->capacity){
        return -1;
    }

    if(queue->size == 0){
        queue->rear = -1;
        queue->front = 0;
    }
    queue->rear++;
    queue->nodes[queue->rear].start_address = dataStart;
    queue->nodes[queue->rear].end_address = dataEnd;
    
    queue->size++;
    return 1;
}

int dequeue(Queue* queue, int *dataStart, int *dataEnd){
    printf("front%d\n", queue->front);
    if(queue->front == queue->capacity){
        queue->front = 0;
    }
    if(queue->size == 0){
        return -1;
    }
  

    if(queue->front == queue->capacity){
        queue->front = 0;
    }
    *dataStart = queue->nodes[queue->front].start_address;
    *dataEnd = queue->nodes[queue->front].end_address;
    queue->front++;
    queue->size--;

    return 1;
}

int is_empty(Queue* queue){
    return (queue->size == 0);
}
