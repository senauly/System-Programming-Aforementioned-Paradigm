#ifndef CLIENT_H
#define CLIENT_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

struct Matrix{
    int row;
    int col;
    int **matrix;
} Matrix;

int **createMatrix(int row);  //++
struct Matrix* readMatrix(const char *fileName);  //++
void writeFIFO(const char *fifoName, struct Matrix *m, int client_id); //++
void freeMatrix(struct Matrix *matrix); //++
void printResponse(char *response); 
int separeteValues(char *line, int **matrix, int row, int count); //++

#endif