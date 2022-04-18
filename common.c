#include "common.h"

void getTimeStamp(char *ts){
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    strftime(ts, 26, "%Y-%m-%d %H:%M:%S", tm);
}