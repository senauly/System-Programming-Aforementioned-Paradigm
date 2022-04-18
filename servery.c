#include "server.h"
#define NO_EINTR(stmt)                   \
    while ((stmt) < 0 && !sigintReceived2 && errno == EINTR ) \
        ;

sig_atomic_t sigintReceived2 = 0;

void handleInterrupt2(int signal_num)
{
    if (signal_num == SIGINT)
        sigintReceived2 = 1;
}

int main(int argc, char *argv[])
{
    struct sigaction interruptHandle;
    memset(&interruptHandle, 0, sizeof(interruptHandle));
    interruptHandle.sa_handler = handleInterrupt2;
    sigaction(SIGINT, &interruptHandle, NULL);

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

    /*if(prepareDaemon(BD_NO_CHDIR) == -1){
        fprintf(stderr, "Error happened while preparing daemon.\n");
        exit(EXIT_FAILURE);
    }*/

    int totalReq = 0;
    int reqForwarded = 0;
    const char *fifoname = argv[2];
    char *logfile = argv[4];
    int poolSizeY = atoi(argv[6]);
    int poolSizeZ = atoi(argv[8]);
    int t = atoi(argv[10]);
    Worker workers[poolSizeY];
    Worker serverZ;

    char buff[26];

    int log_fd = open(logfile, O_APPEND | O_CREAT | O_WRONLY, 0666);
    if (log_fd != STDERR_FILENO)
    {
        if (dup2(log_fd, STDERR_FILENO) < 0)
        {
            exit(EXIT_FAILURE);
        }
        close(log_fd);
    }

    if (poolSizeY < 2 || poolSizeZ < 2 || t < 0)
    {
        getTimeStamp(buff);
        fprintf(stderr, "%s Server Y PID#%d: Error: poolSize, poolSize2 and t must be positive.\n", buff, getpid());
        exit(EXIT_FAILURE);
    }

    getTimeStamp(buff);
    char *pathName = realpath(logfile, NULL);
    fprintf(stderr, "%s Server Y (%s) started, p=%d,t=%d\n", buff, pathName, poolSizeY, t);
    free(pathName);
    // create child processes
    pid_t pid;
    for (int i = 0; i < poolSizeY; i++)
    {
        // open pipe for each child
        if (pipe(workers[i].pipe) == -1)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Server Y PID#%d: Error opening pipe.\n", buff, getpid());
            perror("Opening pipe");
            exit(EXIT_FAILURE);
        }
        pid = fork();

        if (pid == 0)
        {
            // child process
            int busy = busyPoolCount(poolSizeY, workers) + 1;
            childProcessY(t, workers[i].pipe, 0, poolSizeY);
            exit(EXIT_SUCCESS);
        }
        else if (pid < 0)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Client PID#%d: Error opening pipe.\n", buff, getpid());
            perror("Forking child process");
            exit(EXIT_FAILURE);
        }
        else
        {
            // parent process
            workers[i].pid = pid;
        }
    }
    // create child process for server Z
    if (pipe(serverZ.pipe) < 0)
    {
        perror("Opening pipe");
        exit(EXIT_FAILURE);
    }
    pid = fork();
    if (pid == 0)
    {
        // serverz process
        getTimeStamp(buff);
        fprintf(stderr, "%s Instantiated server Z\n", buff);
        serverZ_process(logfile, poolSizeZ, t, serverZ.pipe);
        exit(EXIT_SUCCESS);
    }
    else if (pid < 0)
    {
        getTimeStamp(buff);
        fprintf(stderr, "%s Client PID#%d: Error opening pipe.\n", buff, getpid());
        perror("Error forking child process: ");
        exit(EXIT_FAILURE);
    }
    else
    {
        // parent process
        serverZ.pid = pid;
    }
    // get request from client with FIFO
    if (mkfifo(fifoname, 0666) < 0)
    {
        fprintf(stderr, "Error opening FIFO %s\n", fifoname);
        exit(EXIT_FAILURE);
    }
    int fifo;
    int read_byte;
    int file[1024];
    int size;
    while (1)
    {
        fifo = open(fifoname, O_RDONLY);
        read_byte = read(fifo, file, sizeof(int) * 1024);
        close(fifo);
        if(sigintReceived2) break;
        int row = file[1];
        int col = file[0];
        int client_id = file[2];
        printf("row %d  col %d  client_id %d\n", row, col, client_id);
        int count = 0;
        int flag = 0;
        totalReq++;

        if (read_byte < 0)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Worker PID#%d error happened while reading from fifo.\n", buff, getpid());
            perror("Error reading from FIFO: ");
            exit(EXIT_FAILURE);
        }

        // find available worker
        for (int i = 0; i < poolSizeY; i++)
        {
            printf("170\n");
            if (isWorkerAvailable(workers[i].pid))
            {
                // send request to child process
                printf("SERVERY %d\n", i);
                write(workers[i].pipe[1], file, sizeof(int) * (row * col + 3));
                flag = 1;
                break;
            }

            count++;
        }
        // if no worker is available, send request to server Z so it can handle it
        if (count == poolSizeY || !flag)
        {
            // write to server Z's pipe
            printf("SERVERZ\n");
            getTimeStamp(buff);
            fprintf(stderr, "%s Forwarding request of client PID#%d to serverZ, matrix size %dx%d, pool busy %d/%d\n", buff, client_id, row, row, poolSizeY, poolSizeY);
            write(serverZ.pipe[1], file, sizeof(int) * (row * col + 3));
            reqForwarded++;
        }
    }

    printf("sigint recevied\n");
    for (int i = 0; i < poolSizeY; i++)
    {
        close(workers[i].pipe[0]);
        close(workers[i].pipe[1]);
    }
    for (int i = 0; i < poolSizeY; i++)
    {
        kill(workers[i].pid, SIGINT);
        wait(NULL);
    }
    
    printf("servery child cleaned\n");
    write(serverZ.pipe[1], file, sizeof(int) * 5);
    kill(serverZ.pid, SIGINT);
    wait(NULL);
    close(serverZ.pipe[1]);
    close(serverZ.pipe[0]);
    printf("serverz child cleaned\n");
    unlink(fifoname);
    

    return 0;
}

int prepareDaemon(int flags)
{
    int maxfd;
    int fd;

    // FIRST FORK
    switch (fork())
    {
    case -1:
        return -1;
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }

    if (setsid() < 0)
    {
        return -1;
    }

    // SECOND FORK
    switch (fork())
    {
    case -1:
        return -1;
    case 0:
        break;
    default:
        _exit(EXIT_SUCCESS);
    }

    if (!(flags & BD_NO_UMASK0))
        umask(0);
    if (!(flags & BD_NO_CHDIR))
        chdir("/");
    if (!(flags & BD_NO_CLOSE_FILES))
    {
        maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd == -1)
            maxfd = BD_MAX_CLOSE;
        for (int i = 0; i < maxfd; ++i)
            close(i);
    }

    if (!(flags & BD_NO_REOPEN_STD_FDS))
    {
        close(STDERR_FILENO);

        fd = open("/dev/null", O_RDWR);
        if (fd != STDIN_FILENO)
            return -1;
        if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
            return -1;
        if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
            return -1;
    }

    return 0;
}

void childProcessY(int sleepTime, int pipe[2], int busy, int poolSize)
{
    int req[3];
    char buff[26];
    char active_fname[64];
    sprintf(active_fname, "activep_%d", getpid());
    while (1)
    {
        NO_EINTR(read(pipe[0], req, sizeof(int) * (3)));
        if(sigintReceived2) break;
        if (mkfifo(active_fname, 0666) < 0)
        {
            perror("Erroreheh opening FIFO");
            exit(EXIT_FAILURE);
        }

        int row = req[1];
        int col = req[0];
        int client_id = req[2];

        getTimeStamp(buff);
        fprintf(stderr, "%s Worker PID#%d is handling client PID%d, matrix size %dx%d, pool busy %d/%d\n", buff, getpid(), client_id, row, col, busy, poolSize);

        int *temp = (int *)malloc(sizeof(int) * (row * col + 10));
        NO_EINTR(read(pipe[0], temp, sizeof(int) * (row * col + 3)));
        sleep(sleepTime);
        sendResponse(row, client_id, temp);
        free(temp);

        if (unlink(active_fname) < 0)
        {
            perror("Error unlinking FIFO");
            exit(EXIT_FAILURE);
        }
        remove(active_fname);
    }
    close(pipe[0]);
    close(pipe[1]);
}

void sendResponse(int row, int client_id, int *buff)
{
    int **matrix = convertMatrix(row, buff);
    char uniqFifo[100];
    char time[26];
    sprintf(uniqFifo, "%s%d", "fifo_p", client_id);

    // open FIFO
    int fifo = mkfifo(uniqFifo, 0666);
    if(fifo < 0 && errno != EEXIST)
    {
        perror("Error opening FIFO");
        exit(EXIT_FAILURE);
    }
    int fd = open(uniqFifo, O_RDWR, 0666);
    if (fd == -1)
    {
        getTimeStamp(time);
        fprintf(stderr, "%s Worker PID#%d error happened while opening fifo %s.\n", time, getpid(), uniqFifo);
        perror("Opening FIFO");
        return;
    }

    int res[2];
    res[0] = 0;
    res[1] = 1;

    if (isInvertible(matrix, row))
    {
        getTimeStamp(time);
        fprintf(stderr, "%s Worker PID#%d responding to client PID#%d: the matrix is invertible.\n", time, getpid(), client_id);
        write(fd, &res[1], sizeof(int));
    }
    else
    {
        getTimeStamp(time);
        fprintf(stderr, "%s Worker PID#%d responding to client PID#%d: the matrix is NOT invertible.\n", time, getpid(), client_id);
        write(fd, &res[0], sizeof(int));
    }
    close(fifo);

    for(int i = 0; i < row; i++)
    {
        free(matrix[i]);
    }
    free(matrix);
}

int **convertMatrix(int row, int *buf)
{
    // allocate memory for matrix
    int **matrix = (int **)malloc(sizeof(int *) * row);
    for (int i = 0; i < row; i++)
    {
        matrix[i] = (int *)malloc(sizeof(int) * (row + 5));
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
    char active_fname[256];
    char buff[26];

    sprintf(active_fname, "activep_%d", pid);

    // do file exist
    int fd;
    if (fd = mkfifo(active_fname, 0666) < 0)
    {
        if (errno == EEXIST)
        {
            return 0;
        }
        else
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Worker PID#%d error happened while %d opening fifo.\n", buff,errno,  getpid());
            perror("File error");
        }
    }
    else
    {
        close(fd);
        remove(active_fname);
        return 1;
    }
}

int isInvertible(int **matrix, int row)
{
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

// calculate cofactor of a matrix
int **cofactor(int **matrix, int row, int col, int p, int q, int size)
{

    int **cofactorMatrix = (int **)malloc(sizeof(int *) * (size));
    for (int i = 0; i < size; i++)
    {
        cofactorMatrix[i] = (int *)malloc(sizeof(int) * (size + 5));
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

                if (j == size - 1)
                {
                    j = 0;
                    i++;
                }
            }
        }
    }

    return cofactorMatrix;
}

// calculate determinant with cofactor expansion
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
        for (int i = 0; i < size; ++i)
        {
            int **cofactorMatrix = cofactor(matrix, size, size, 0, i, size);
            det += sign * matrix[0][i] * determinant(cofactorMatrix, size - 1);
            sign = -sign;
            for (int j = 0; j < size; j++)
            {
                free(cofactorMatrix[j]);
            }
            free(cofactorMatrix);
        }

        return det;
    }
}



SharedMemory *sm = NULL;

void serverzHandler(int sig_num){
    if(sig_num == SIGINT){
        if(sm != NULL){
            sem_post(&sm->empty);
            sem_post(&sm->full);
            sem_post(&sm->critical);
        }
        sigintReceived2 = 1;
    }
}

void serverZ_process(char *logfile, int poolSizeZ, int t, int pipe[2])
{
    struct sigaction interruptHandle;
    memset(&interruptHandle, 0, sizeof(interruptHandle));
    interruptHandle.sa_handler = &serverzHandler;
    sigaction(SIGINT, &interruptHandle, NULL);
    char buff[26];

    // create workers
    getTimeStamp(buff);
    char *pathName = realpath(logfile, NULL);
    fprintf(stderr, "%s Z:Server Z (%s) started t=%d, r=%d.\n", buff, pathName, t, poolSizeZ);
    free(pathName);
    Worker workers[poolSizeZ];
    void *sm_addr = initSharedMemory(SHARED_MEMORY_SIZE);
    SharedMemory *sm_mem = (SharedMemory *)sm_addr;
    sem_init(&sm_mem->empty, 1, MAX_QUEUE_SIZE);
    sem_init(&sm_mem->full, 1, 0);
    sem_init(&sm_mem->critical, 1, 1);

    // create child processes for workers
    for (int i = 0; i < poolSizeZ; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            childProcessZ(t, sm_addr, poolSizeZ);
            exit(EXIT_SUCCESS);
        }
        else if (pid > 0)
        {
            // parent process
            workers[i].pid = pid;
        }
        else
        {
            // error
            getTimeStamp(buff);
            fprintf(stderr, "%s Z:Server Z (%s) error happened while creating child process.\n", buff, pathName);
            perror("Fork error:");
            exit(EXIT_FAILURE);
        }
    }
    sm = (SharedMemory *)sm_addr;
    while (1)
    {
        printf("test541\n");
        
        int req[3];
        NO_EINTR(read(pipe[0], req, sizeof(int) * (3)));
        printf("test545\n");
        if(sigintReceived2) break;
        int row = req[1];
        int col = req[0];
        int size = (row * col + 3);
        int *temp = (int *)malloc(sizeof(int) * (size + 10));
        NO_EINTR(read(pipe[0], &temp[3], sizeof(int) * size));
        if(sigintReceived2) break;
        temp[0] = row;
        temp[1] = col;
        temp[2] = req[2];
        sem_wait(&sm->empty);
        sem_wait(&sm->critical);
        if(sigintReceived2) break;
        addToSharedMemory(sm_addr, sizeof(int) * size, temp, sm_addr);
        sem_post(&sm->critical);
        sem_post(&sm->full);
        free(temp);
    }
    printf("test561\n");
    for(int i = 0; i < poolSizeZ; i++)
    {
        kill(workers[i].pid, SIGINT);
        wait(NULL);
    }
    sem_destroy(&sm_mem->empty);
    sem_destroy(&sm_mem->full);
    sem_destroy(&sm_mem->critical);
    close(pipe[0]);
    close(pipe[1]);
    munmap(sm_addr, SHARED_MEMORY_SIZE);
}




void childProcessZ(int sleepTime, void *ptr, int poolSizeZ)
{   
    struct sigaction interruptHandle;
    memset(&interruptHandle, 0, sizeof(interruptHandle));
    interruptHandle.sa_handler = &serverzHandler;
    sigaction(SIGINT, &interruptHandle, NULL);
    sm = (SharedMemory *)ptr;
    char buff[26];
    
    while (1)
    {
        int *req = NULL;
        sem_wait(&sm->full);
        sem_wait(&sm->critical);
        if(sigintReceived2) {
            printf("sigint received 574\n");
            break;
        }
        sm->busy++;
        if(readFromSharedMemory(sm, &req, ptr) < 0) printf("AAAAAAAA\n");
        
        sem_post(&sm->critical);
        sem_post(&sm->empty);
        int row = req[1];
        int client_id = req[2];
        getTimeStamp(buff);
        fprintf(stderr, "%s Z: Worker PID#%d is handling client PID#%d, matrix size %dx%d, pool busy %d/%d\n", buff, getpid(), client_id, row, row, sm->busy, poolSizeZ);
        sleep(sleepTime);
        sendResponse(row, client_id, req);
        free(req);
        sm->busy--;
    }
    printf("609\n");
    
    munmap(ptr, SHARED_MEMORY_SIZE);
    printf("611\n");
}

int busyPoolCount(int size, Worker workers[])
{
    int count = 0;
    for (int i = 0; i < size; i++)
    {
        if (!isWorkerAvailable(workers[i].pid))
        {
            count++;
        }
    }

    return count;
}
