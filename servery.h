#ifndef SERVER_H
#define SERVER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>


//struct for the worker process
typedef struct worker{
    int pid;
    int pipe[2];
}Worker;

int recieveRequest(int fd, char *buffer, int size);
int sendResponse(int fd, char *buffer, int size);
int processRequest(int fd, char *buffer, int size);
void childProcess(const char *logfile, int sleepTime, int pipe[2]);
int *readFifo(const char *fifoname);
int **convertMatrix(int row, int *buf);
int isWorkerAvailable(int pid);
int isInvertible(int **matrix, int row);
int determinant(int **matrix, int row);


#endif