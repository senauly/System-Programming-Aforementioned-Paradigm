#ifndef CLIENT_H
#define CLIENT_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include "common.h"


struct Matrix{
    int row;
    int col;
    int **matrix;
} Matrix;

int **createMatrix(int row);
struct Matrix* readMatrix(const char *fileName);
int writeFIFO(const char *fifoName, struct Matrix *m, int client_id);
void freeMatrix(struct Matrix *matrix);
int printResponse(int uniqFifoFd, char* uniqFifo, time_t start);
int separeteValues(char *line, int **matrix, int row, int count);

#endif