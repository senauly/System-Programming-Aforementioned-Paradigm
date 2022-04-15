#include "servery.h"

int main(int argc, char const *argv[])
{
    // get arguments
    if (argc != 11)
    {
        fprintf(stderr, " Usage: ./serverY -s pathToServerFifo -o pathToLogFile -p poolSize -r poolSize2 -t 2\n");
        exit(EXIT_FAILURE);
    }

    // check if arguments are valid

    if (strcmp(argv[1], "-s") != 0 || strcmp(argv[3], "-o") != 0 || strcmp(argv[5], "-p") != 0 || strcmp(argv[7], "-r") != 0 || strcmp(argv[9], "-t") != 0)
    {
        fprintf(stderr, "Error: wrong arguments. Usage: ./serverY -s pathToServerFifo -o pathToLogFile -p poolSize -r poolSize2 -t 2\n");
        exit(EXIT_FAILURE);
    }

    const char *fifoname = argv[2];
    const char *logfile = argv[4];
    int poolSizeY = atoi(argv[6]);
    int poolSizeZ = atoi(argv[8]);
    int t = atoi(argv[10]);
    Worker workers[poolSizeY];

    if (poolSizeY < 2 || poolSizeZ < 2 || t < 0)
    {
        fprintf(stderr, "Error: poolSize, poolSize2 and t must be positive\n");
        exit(EXIT_FAILURE);
    }

    // create child processes
    pid_t pid;
    for (int i = 0; i < poolSizeY; i++)
    {
        // open pipe for each child
        if (pipe(workers[i].pipe) == -1)
        {
            fprintf(stderr, "Error opening pipe\n");
            exit(EXIT_FAILURE);
        }
        pid = fork();

        if (pid == 0)
        {
            // child process
            childProcess(logfile, t, workers[i].pipe);
        }
        else if (pid < 0)
        {
            fprintf(stderr, "Error forking child process\n");
            exit(EXIT_FAILURE);
        }
        else
        {
            // parent process
            workers[i].pid = pid;
        }
    }

    // get request from client with FIFO
    int fd = mkfifo(fifoname, 0666);
    if (fd == -1)
    {
        fprintf(stderr, "Error opening FIFO %s\n", fifoname);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        int *buffer = readFifo(fifoname);
        int row = buffer[1];
        int col = buffer[0];

        // find available worker
        for (int i = 0; i < poolSizeY; i++)
        {
            if (isWorkerAvailable(workers[i].pid))
            {
                // send request to child process
                write(workers[i].pipe[1], buffer, sizeof(int) * (row * col + 3));
                break;
            }
        }

        
    }
    return 0;
}

void childProcess(const char *logfile, int sleepTime, int pipe[2])
{
    // READ FROM PIPE
    int req[3];
    while (1)
    {
        read(pipe[0], req, sizeof(int) * (3));
        int row = req[1];
        int col = req[0];
        int client_id = req[2];
        int *temp = (int *)malloc(sizeof(int) * (row * col + 3));
        read(pipe[0], temp, sizeof(int) * (row * col + 3));
        int **matrix = convertMatrix(row, temp);
        free(temp);
        
        char uniqFifo[100];
        sprintf(uniqFifo, "%s%d", "fifo_p", client_id);

        // open FIFO
        int fd = open(uniqFifo, O_WRONLY, 0644);
        if (fd == -1)
        {
            fprintf(stderr, "Error opening FIFO %s\n", uniqFifo);
            exit(EXIT_FAILURE);
        }

        int res[2];
        res[0] = 0;
        res[1] = 1;

        if (isInvertible(matrix, row))
        {
            write(fd, &res[1], sizeof(int));
        }
        else
        {
            write(fd, &res[0], sizeof(int));
        }

        sleep(sleepTime);
    }

}

int *readFifo(const char *fifoname)
{
    // open fifo for reading
    int fifo = open(fifoname, O_RDONLY);
    if (fifo == -1)
    {
        fprintf(stderr, "Error opening FIFO %s\n", fifoname);
        exit(EXIT_FAILURE);
    }

    int size[1];
    if (read(fifo, size, sizeof(int)) == -1)
    {
        fprintf(stderr, "Error reading from FIFO %s\n", fifoname);
        exit(EXIT_FAILURE);
    }

    int *buffer = (int *)malloc(sizeof(int) * (size[0] * size[0] + 3));
    buffer[0] = size[0];
    if (read(fifo, &buffer[1], sizeof(int) * (size[0] * size[0] + 2)) == -1)
    {
        fprintf(stderr, "Error reading from FIFO %s\n", fifoname);
        exit(EXIT_FAILURE);
    }

    close(fifo);
    return buffer;
}

int **convertMatrix(int row, int *buf)
{
    // allocate memory for matrix
    int **matrix = (int **)malloc(sizeof(int *) * row);
    for (int i = 0; i < row; i++)
    {
        matrix[i] = (int *)malloc(sizeof(int) * row);
    }

    for (int i = 0; i < row; i++)
    {
        for (int j = 0; j < row; j++)
        {
            matrix[i][j] = buf[i * row + j];
        }
    }

    return matrix;
}

int isWorkerAvailable(int pid)
{
    // TODO
    return 1;
}

int isInvertible(int **matrix, int row){
    int det = determinant(matrix, row);
    if (det == 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}


//calculate cofactor of a matrix
int **cofactor(int **matrix, int row, int col, int p, int q, int size){

    int **cofactorMatrix = (int **)malloc(sizeof(int *) * (size));
    for (int i = 0; i < size; i++)
    {
        cofactorMatrix[i] = (int *)malloc(sizeof(int) * (size));
    }

    int i = 0, j = 0;
    for (int row = 0; row < size; row++)
    {
        for (int col = 0; col < size; col++)
        {
            if (row != p && col != q)
            {
                cofactorMatrix[i][j] = matrix[row][col];
                
                j++;

                if (j == size - 1) {
                    j = 0;
                    i++;
                }
            }
            
        }
    }

    return cofactorMatrix;
}


//calculate determinant with cofactor expansion
int determinant(int **matrix, int size)
{
    int det = 0;
    int sign = 1;
    if (size == 1)
    {
        return matrix[0][0];
    }
    else if (size == 2)
    {
        return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
    }
    else
    {
        for(int i = 0; i < size; ++i){
            int **cofactorMatrix = cofactor(matrix, size, size, 0, i, size);
            det += sign * matrix[0][i] * determinant(cofactorMatrix, size - 1);
            printf("%d det\n", det);
            sign = -sign;
            for (int j = 0; j < size; j++){
                free(cofactorMatrix[j]);
            }
            free(cofactorMatrix);
        }
        
        
        return det;
    }
}