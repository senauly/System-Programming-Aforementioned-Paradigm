#ifndef SERVER_H
#define SERVER_H
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdint.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include "queue.h"
#include "common.h"

#define MATRIX_SIZE 8196
#define SHARED_MEMORY_SIZE 200000
#define BD_NO_CHDIR  01/* Don't chdir("/") */
#define BD_NO_CLOSE_FILES  02/* Don't close all open files */
#define BD_NO_REOPEN_STD_FDS  04 /* Don't reopen stdin, stdout, and stderr to       *   /dev/null */
#define BD_NO_UMASK0 010 /* Don't do a umask(0) */
#define BD_MAX_CLOSE 8192 /* Maximum file descriptors to close if* sysconf(_SC_OPEN_MAX) is indeterminate */

//struct for the worker process
typedef struct worker{
    int pid;
    int pipe[2];
}Worker;

typedef struct sharedmemory{
    struct queue queue;
    int size;
    int fd;
    sem_t empty;
    sem_t full;
    sem_t critical;
    sem_t interrupt;
    int busy;
    int invertible;
    int req_count;
}SharedMemory;

int prepareDaemon(int flags);
int sendResponse(int row, int client_id, int *buff);
void childProcessY(int sleepTime, int pipe[2], int busy, int poolSize);
int childProcessZ(int sleepTime, void *ptr, int poolSizeZ);
int **convertMatrix(int row, int *buf);
int isWorkerAvailable(int pid);
int isInvertible(int **matrix, int row);
int determinant(int **matrix, int row);
int serverZ_process(char *logfile, int poolSizeZ, int t, int pipe[2]);
int busyPoolCount(int size, Worker workers[]);
void killProcesses(int size, Worker workers[]);
void closePipes(int size, Worker workers[]);
void freeMatrix(int **matrix, int row);

//initalize shared memory
void *initSharedMemory(int size);
//find start place in shared memory
int findPlace(SharedMemory *sm, int reqSize);
//add to shared memory
int addToSharedMemory(SharedMemory *sm, int size, int *request, void *ptr);
//get from shared memory
int readFromSharedMemory(SharedMemory *sm, int **request, void *ptr);
//free shared memory
void freeSharedMemory(SharedMemory *sm);



#endif