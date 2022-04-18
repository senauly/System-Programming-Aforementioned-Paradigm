#include <stdio.h>
#include <stdlib.h>
#include <time.h>

char* getTimeStamp(){
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char *timeStamp = malloc(sizeof(char) * 26);
    strftime(timeStamp, 26, "%Y-%m-%d %H:%M:%S", tm);
    return timeStamp;
}

char* getCurrentPath(char *filename){
    return realpath(filename, NULL);
}
