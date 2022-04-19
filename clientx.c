
#include "client.h"
#define NO_EINTR(stmt)                   \
    while ((stmt) < 0 && errno == EINTR) \
        ;

sig_atomic_t sigintReceived = 0;

void handleInterrupt(int signal_num)
{
    if (signal_num == SIGINT)
        sigintReceived = 1;
}

int main(int argc, char const *argv[])
{
    char ts[26];

    struct sigaction interruptHandle;
    memset(&interruptHandle, 0, sizeof(interruptHandle));
    interruptHandle.sa_handler = handleInterrupt;

    if (sigaction(SIGINT, &interruptHandle, NULL) == -1)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // get arguments
    if (argc != 5)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s Client PID#%d: Error. Usage: ./client -s pathToServerFifo -o pathToDataFile\n", ts, getpid());
        exit(EXIT_FAILURE);
    }

    // create fifo with pid
    char uniqFifo[100];
    sprintf(uniqFifo, "%s%d", "fifo_p", getpid());

    int fd = mkfifo(uniqFifo, 0666);
    if (fd == -1)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s Client PID#%d Error, ", ts, getpid());
        perror("Opening FIFO");
        exit(EXIT_FAILURE);
    }
    int uniqFifoFd = open(uniqFifo, O_RDWR);
    const char *filename = argv[4];
    const char *fifoname = argv[2];
    char *str = realpath(filename, NULL);
    if (sigintReceived)
    {
        fprintf(stderr, "%s SIGINT received, exiting Client PID#%d\n.", ts, getpid());
        exit(EXIT_SUCCESS);
    }

    struct Matrix *matrix = readMatrix(filename);
    if (matrix == NULL)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s Client PID#%d Error, ", ts, getpid());
        unlink(uniqFifo);
        exit(EXIT_FAILURE);
    }

    time_t start = time(NULL);
    getTimeStamp(ts);
    fprintf(stdout, "%s Client PID#%d (%s) is submitting a %dx%d matrix.\n", ts, getpid(), str, matrix->row, matrix->col);
    free(str);

    if (sigintReceived)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s SIGINT received, exiting Client PID#%d\n.", ts, getpid());
        freeMatrix(matrix);
        exit(EXIT_SUCCESS);
    }

    if (sigintReceived)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s SIGINT received, exiting Client PID#%d\n.", ts, getpid());
        freeMatrix(matrix);
        unlink(uniqFifo);
        exit(EXIT_SUCCESS);
    }

    writeFIFO(fifoname, matrix, getpid());
    freeMatrix(matrix);
    if (sigintReceived)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s SIGINT received, exiting Client PID#%d\n.", ts, getpid());
        unlink(uniqFifo);
        exit(EXIT_SUCCESS);
    }
    printResponse(uniqFifoFd, uniqFifo, start);

    exit(EXIT_SUCCESS);
}

int **createMatrix(int row)
{
    int **matrix = (int **)malloc(row * sizeof(int *));
    for (int i = 0; i < row; i++)
    {
        matrix[i] = (int *)malloc(row * sizeof(int));
    }

    return matrix;
}

struct Matrix *readMatrix(const char *fileName)
{
    FILE *file;
    char ts[26];

    int count = 0, row = 0;
    int **matrix;
    int lineCap = 100;
    char *line = malloc(sizeof(char) * lineCap);
    struct Matrix *matrixStruct;
    file = fopen(fileName, "r");
    if (file == NULL)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s Client PID#%d: Error. File %s not found.\n", ts, getpid(), fileName);
        return NULL;
    }
    // read the first line
    char ch = ',';
    while ((ch = fgetc(file)) != '\n' && ch != EOF)
    {
        if (count > 100)
        {
            line = (char *)realloc(line, sizeof(char) * (lineCap *= 2));
        }
        line[count] = ch;
        if (ch == ',')
        {
            row++;
        }

        count++;
    }

    row++;

    if (row < 2)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s Client PID#%d: Error. Matrix must have at least 2 row.\n", ts, getpid());
        return NULL;
    }

    // create the matrix
    matrix = createMatrix(row);
    if (separeteValues(line, matrix, row, 0) == -1)
    {
        fprintf(stderr, "Matrix must be square.\n");
        return NULL;
    }

    // read the rest of the file
    int i = 1;
    while (i < row)
    {
        count = 0;
        ch = ',';

        while ((ch = fgetc(file)) != EOF && ch != '\n')
        {
            if (count > lineCap)
            {
                line = (char *)realloc(line, sizeof(char) * (lineCap *= 2));
            }
            line[count] = ch;
            count++;
        }
        line[count] = '\0';
        if (separeteValues(line, matrix, row, i) == -1)
        {
            getTimeStamp(ts);
            fprintf(stderr, "%s Client PID#%d: Error. Matrix must be square.\n", ts, getpid());
            return NULL;
        }
        i++;
    }

    if (fgetc(file) != EOF)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s Client PID#%d: Error. Matrix must be square.\n", ts, getpid());
        return NULL;
    }

    // create matrix struct
    matrixStruct = (struct Matrix *)malloc(sizeof(struct Matrix));
    matrixStruct->row = row;
    matrixStruct->col = row;
    matrixStruct->matrix = matrix;

    fclose(file);
    free(line);
    return matrixStruct;
}

// to do -1 case
int separeteValues(char *line, int **matrix, int row, int count)
{
    // seperate string by comma
    char *token;
    int i = 0;
    token = strtok(line, ",");
    while (token != NULL)
    {
        if (i >= row)
            return -1;
        matrix[count][i] = atoi(token);
        token = strtok(NULL, ",");
        i++;
    }

    if (i < row)
        return -1;

    return 0;
}

int writeFIFO(const char *fifoName, struct Matrix *m, int client_id)
{
    char ts[26];
    int write_byte;
    // open FIFO
    int fd = open(fifoName, O_WRONLY);
    if (fd == -1)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s Client PID#%d: Error, ", ts, getpid());
        perror("Opening FIFO");
        return -1;
    }

    // write to FIFO
    int *res = (int *)malloc(sizeof(int) * (m->row * m->col + 3));
    res[2] = client_id;
    res[1] = m->row;
    res[0] = m->col;

    for (int i = 0; i < m->row; i++)
    {
        for (int j = 0; j < m->col; j++)
        {
            res[i * m->col + j + 3] = m->matrix[i][j];
        }
    }

    NO_EINTR(write_byte = write(fd, res, sizeof(int) * (m->row * m->col + 3)));

    if (write_byte < 0)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s Client PID#%d: Error, ", ts, getpid());
        perror("Writing to FIFO");
        close(fd);
        free(res);
        return -1;
    }
    close(fd);
    free(res);
    return 0;
}

void freeMatrix(struct Matrix *matrix)
{
    for (int i = 0; i < matrix->row; i++)
    {
        free(matrix->matrix[i]);
    }
    free(matrix->matrix);
    free(matrix);
}

int printResponse(int uniqFifoFd, char *uniqFifo, time_t start)
{
    // open fifo for reading
    char ts[26];
    int read_byte;
    // read from fifo
    int res;
    read_byte = read(uniqFifoFd, &res, sizeof(int));
    if (read_byte < 0)
    {
        getTimeStamp(ts);
        fprintf(stderr, "%s Client PID#%d: Error, ", ts, getpid());
        perror("Reading from FIFO");
        close(uniqFifoFd);
        unlink(uniqFifo);
        return -1;
    }

    time_t end = time(NULL);
    getTimeStamp(ts);
    if (res)
    {
        fprintf(stdout, "%s Client PID#%d: matrix is invertible, total time %ld seconds, goodbye.\n", ts, getpid(), (end - start));
    }

    else
    {
        fprintf(stdout, "%s Client PID#%d: matrix is non-invertible, total time %ld seconds, goodbye.\n", ts, getpid(), (end - start));
    }

    close(uniqFifoFd);
    unlink(uniqFifo);
    remove(uniqFifo);
    return 0;
}
