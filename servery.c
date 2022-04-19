
#include "server.h"

#define FIFO_Y "fifo_y"
sig_atomic_t sigintReceivedY = 0;
sig_atomic_t invertible = 0;
sig_atomic_t busy = 0;

#define NO_EINTR(stmt)                                       \
    while ((stmt) < 0 && !sigintReceivedY && errno == EINTR) \
        ;

void handleInterruptY(int signal_num)
{
    if (signal_num == SIGINT)
        sigintReceivedY = 1;
    else if (signal_num == SIGUSR1)
        invertible++;
}

int main(int argc, char *argv[])
{
    
    

    if (mkfifo(FIFO_Y, 0666) == -1)
    {   
        
        if (errno == EEXIST)
        {
            fprintf(stderr, "There should be single instance of ServerY. It is already running!\n");
            exit(1);
        }
    }
    struct sigaction interruptHandle;
    memset(&interruptHandle, 0, sizeof(interruptHandle));
    interruptHandle.sa_handler = handleInterruptY;
    sigaction(SIGINT, &interruptHandle, NULL);
    sigaction(SIGUSR1, &interruptHandle, NULL);

    // get arguments

    if (argc != 11)
    {
        fprintf(stderr, " Usage: ./serverY -s pathToServerFifo -o pathToLogFile -p poolSize -r poolSize2 -t 2\n");
        remove(FIFO_Y);
        exit(EXIT_FAILURE);
    }

    // check if arguments are valid

    if (strcmp(argv[1], "-s") != 0 || strcmp(argv[3], "-o") != 0 || strcmp(argv[5], "-p") != 0 || strcmp(argv[7], "-r") != 0 || strcmp(argv[9], "-t") != 0)
    {
        fprintf(stderr, "Error: wrong arguments. Usage: ./serverY -s pathToServerFifo -o pathToLogFile -p poolSize -r poolSize2 -t 2\n");
        remove(FIFO_Y);
        exit(EXIT_FAILURE);
    }

    if (prepareDaemon(BD_NO_CHDIR) == -1)
    {
        fprintf(stderr, "Error happened while preparing daemon.\n");
        remove(FIFO_Y);
        exit(EXIT_FAILURE);
    }

    int totalReq = 0;
    int reqForwarded = 0;
    const char *fifoname = argv[2];
    char *logfile = argv[4];
    int poolSizeY = atoi(argv[6]);
    int poolSizeZ = atoi(argv[8]);
    int t = atoi(argv[10]);
    Worker *workers = (Worker *)malloc(sizeof(Worker) * poolSizeY);
    Worker serverZ;

    char buff[26];

    int log_fd;
    NO_EINTR(log_fd = open(logfile, O_APPEND | O_CREAT | O_WRONLY, 0666));
    if (log_fd != STDERR_FILENO)
    {
        if (dup2(log_fd, STDERR_FILENO) < 0)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s: ", buff);
            perror("dup2:");
        }
        close(log_fd);
    }


    if (poolSizeY < 2 || poolSizeZ < 2 || t < 0)
    {
        getTimeStamp(buff);


        fprintf(stderr, "%s Server Y Error: poolSize, poolSize2 and t must be positive.\n", buff);


        free(workers);
        remove(FIFO_Y);
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
    
    
            fprintf(stderr, "%s Server Y PID#%d ", buff, getpid());
    
    
            perror("Opening pipe");
            free(workers);
            remove(FIFO_Y);
            exit(EXIT_FAILURE);
        }
        pid = fork();

        if (pid == 0)
        {
            // child process
            
            childProcessY(t, workers[i].pipe, 0, poolSizeY);
            free(workers);
            exit(EXIT_SUCCESS);
        }
        else if (pid < 0)
        {
            getTimeStamp(buff);
    
    
            fprintf(stderr, "%s ", buff);
            perror("Forking child process");
    
    
            closePipes(i, workers);
            killProcesses(i, workers);
            close(serverZ.pipe[1]);
            close(serverZ.pipe[0]);
            free(workers);
            remove(FIFO_Y);
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
        getTimeStamp(buff);


        fprintf(stderr, "%s ", buff);
        perror("Opening pipe");


        closePipes(poolSizeY, workers);
        killProcesses(poolSizeY, workers);
        free(workers);
        remove(FIFO_Y);
        exit(EXIT_FAILURE);
    }
    pid = fork();
    if (pid == 0)
    {
        // serverz process
        free(workers);
        getTimeStamp(buff);


        fprintf(stderr, "%s Instantiated server Z\n", buff);


        if (serverZ_process(logfile, poolSizeZ, t, serverZ.pipe) == -1)
        {
            closePipes(poolSizeY, workers);
            killProcesses(poolSizeY, workers);
            free(workers);
            remove(FIFO_Y);
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }
    else if (pid < 0)
    {
        getTimeStamp(buff);


        fprintf(stderr, "%s ", buff);
        perror("Error forking child process: ");


        close(serverZ.pipe[1]);
        close(serverZ.pipe[0]);
        closePipes(poolSizeY, workers);
        killProcesses(poolSizeY, workers);
        kill(serverZ.pid, SIGINT);
        wait(NULL);
        free(workers);
        remove(FIFO_Y);
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


        fprintf(stderr, "%s ", fifoname);
        perror("Creating FIFO");
        close(serverZ.pipe[1]);
        close(serverZ.pipe[0]);
        closePipes(poolSizeY, workers);
        killProcesses(poolSizeY, workers);
        free(workers);
        remove(FIFO_Y);
        exit(EXIT_FAILURE);
    }

    int fifo;
    int read_byte;
    int file[MATRIX_SIZE];
    int size;

    while (1)
    {
        NO_EINTR(fifo = open(fifoname, O_RDONLY));
        NO_EINTR(read_byte = read(fifo, file, sizeof(int) * MATRIX_SIZE));
        if(read_byte < 0)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s ", buff);
            perror("Error reading from FIFO ");
            break;
        }
        close(fifo);
        if (sigintReceivedY)
        {
            break;
        }

        int row = file[1];
        int col = file[0];
        int client_id = file[2];
        int count = 0;
        int flag = 0;

        if (read_byte < 0)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Worker PID#%d ", buff, getpid());
            perror("Error reading from FIFO ");
            break;
        }

        int busyCount = busyPoolCount(poolSizeY, workers);

        // find available worker
        for (int i = 0; i < poolSizeY; i++)
        {
            if (isWorkerAvailable(workers[i].pid))
            {
                // send request to child process
                getTimeStamp(buff);
                fprintf(stderr, "%s Worker PID#%d is handling client PID%d, matrix size %dx%d, pool busy %d/%d\n", buff, workers[i].pid, client_id, row, col, busyCount + 1, poolSizeY);
                int write_bytes;
                NO_EINTR(write_bytes = write(workers[i].pipe[1], file, sizeof(int) * (row * col + 3)));
                flag = 1;
                break;
            }

            count++;
        }
        // if no worker is available, send request to server Z so it can handle it
        if (count == poolSizeY || !flag)
        {
            // write to server Z's pipe
            getTimeStamp(buff);
            fprintf(stderr, "%s Forwarding request of client PID#%d to serverZ, matrix size %dx%d, pool busy %d/%d\n", buff, client_id, row, row, busyCount, poolSizeY);
            int write_bytes;
            NO_EINTR(write_bytes =  write(serverZ.pipe[1], file, sizeof(int) * (row * col + 3)));
            reqForwarded++;
        }

        totalReq++;
    }


    getTimeStamp(buff);
    fprintf(stderr, " %s SIGINT received, terminating Z and exiting server Y. Total request handled: %d, %d invertible, %d not. %d requests were forwarded.\n", buff, totalReq, invertible, totalReq - invertible - reqForwarded, reqForwarded);
    int write_bytes = 0;
    NO_EINTR(write_bytes = write(serverZ.pipe[1], "close", 5));
    kill(serverZ.pid, SIGINT);
    wait(NULL);
    for (int i = 0; i < poolSizeY; i++)
    {
        kill(workers[i].pid, SIGINT);
        wait(NULL);
    }
    close(serverZ.pipe[1]);
    close(serverZ.pipe[0]);
    closePipes(poolSizeY, workers);
    unlink(fifoname);
    free(workers);
    remove(FIFO_Y);
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

        NO_EINTR(fd = open("/dev/null", O_RDWR));
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
        int read_bytes;
        NO_EINTR(read_bytes = read(pipe[0], req, sizeof(int) * (3)));
        if (read_bytes < 0)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Worker PID#%d ", buff, getpid());
            perror("Error reading from FIFO ");
            break;
        }
        if (sigintReceivedY)
            break;
        if (mkfifo(active_fname, 0666) < 0)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Worker PID#%d ", buff, getpid());
            perror("Error opening FIFO");
            break;
        }
        int row = req[1];
        int col = req[0];
        int client_id = req[2];

        

        int *temp = (int *)malloc(sizeof(int) * (row * col + 10));
        
        NO_EINTR(read(pipe[0], temp, sizeof(int) * (row * col + 3)));
        sleep(sleepTime);
        if (sendResponse(row, client_id, temp) == -1)
        {
            free(temp);

            break;
        }

        else if (sendResponse(row, client_id, temp) == 1)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Worker PID#%d responding to client PID#%d: the matrix is invertible.\n", buff, getpid(), client_id);
            kill(getppid(), SIGUSR1);
        }

        else if (sendResponse(row, client_id, temp) == 0)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Worker PID#%d responding to client PID#%d: the matrix is not invertible.\n", buff, getpid(), client_id);
        }
        free(temp);

        unlink(active_fname);
        remove(active_fname);
    }
    close(pipe[0]);
    close(pipe[1]);
}

int sendResponse(int row, int client_id, int *buff)
{
    int **matrix = convertMatrix(row, buff);
    char uniqFifo[100];
    char time[26];
    sprintf(uniqFifo, "%s%d", "fifo_p", client_id);
    // open FIFO
    int fifo = mkfifo(uniqFifo, 0666);
    if (fifo < 0 && errno != EEXIST)
    {
        getTimeStamp(time);
        fprintf(stderr, "%s Worker PID#%d ", time, getpid());
        perror("Error opening FIFO");
        freeMatrix(matrix, row);
        return -1;
    }

    int fd;
    NO_EINTR(fd = open(uniqFifo, O_RDWR, 0666));
    if (fd == -1)
    {
        getTimeStamp(time);
        fprintf(stderr, "%s Worker PID#%d ", time, getpid());
        perror("Opening FIFO");
        freeMatrix(matrix, row);
        return -1;
    }

    int res[2];
    res[0] = 0;
    res[1] = 1;

    int write_bytes = 0;
    if (isInvertible(matrix, row))
    {
        NO_EINTR(write_bytes = write(fd, &res[1], sizeof(int)));
        close(fifo);
        freeMatrix(matrix, row);
        return 1;
    }

    else
    {
        NO_EINTR(write_bytes = write(fd, &res[0], sizeof(int)));
        close(fifo);
        freeMatrix(matrix, row);
        return 0;
    }
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
            fprintf(stderr, "%s Worker PID#%d ", buff, getpid());
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
// prevent semaphore waits if sigint
void serverzHandler(int sig_num)
{
    if (sig_num == SIGINT)
    {
        if (sm != NULL)
        {
            sem_post(&sm->empty);
            sem_post(&sm->full);
            sem_post(&sm->critical);
        }
        sigintReceivedY = 1;
    }
}

int serverZ_process(char *logfile, int poolSizeZ, int t, int pipe[2])
{
    struct sigaction interruptHandle;
    memset(&interruptHandle, 0, sizeof(interruptHandle));
    interruptHandle.sa_handler = &serverzHandler;
    sigaction(SIGINT, &interruptHandle, NULL);
    char buff[26];

    getTimeStamp(buff);
    char *pathName = realpath(logfile, NULL);
    fprintf(stderr, "%s Z:Server Z (%s) started t=%d, r=%d.\n", buff, pathName, t, poolSizeZ);
    free(pathName);
    Worker *workers = (Worker *)malloc(sizeof(Worker) * poolSizeZ);
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
            free(workers);
            if (childProcessZ(t, sm_addr, poolSizeZ) == -1)
            {
                getTimeStamp(buff);
                fprintf(stderr, "%s Z:Server Z child process failed.\n", buff);
            }
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
            fprintf(stderr, "%s Z: ", buff);
            perror("Fork error:");
            for (int j = 0; j < i; j++)
            {
                kill(workers[j].pid, SIGINT);
                wait(NULL);
            }

            sem_destroy(&sm_mem->empty);
            sem_destroy(&sm_mem->full);
            sem_destroy(&sm_mem->critical);
            close(pipe[0]);
            close(pipe[1]);
            munmap(sm_addr, SHARED_MEMORY_SIZE);
            free(workers);
            return -1;
        }
    }
    sm = (SharedMemory *)sm_addr;
    int readByte;
    while (1)
    {
        int req[3];
        NO_EINTR(readByte = read(pipe[0], req, sizeof(int) * (3)));
        if(readByte < 0) {
            break;
        }
        if (sigintReceivedY)
        {
            break;
        }

        int row = req[1];
        int col = req[0];
        int size = (row * col + 3);
        int *temp = (int *)malloc(sizeof(int) * (size + 10));
        NO_EINTR(readByte = read(pipe[0], &temp[3], sizeof(int) * size));
        if(readByte < 0) {
            break;
        }
        if (sigintReceivedY)
        {
            free(temp);
            break;
        }
        temp[0] = row;
        temp[1] = col;
        temp[2] = req[2];
        sem_wait(&sm->empty);
        sem_wait(&sm->critical);
        if (sigintReceivedY)
        {
            free(temp);
            break;
        }

        if (addToSharedMemory(sm_addr, sizeof(int) * size, temp, sm_addr) == -1)
        {
            free(temp);
            break;
        }

        sem_post(&sm->critical);
        sem_post(&sm->full);
        free(temp);
    }

    getTimeStamp(buff);
    fprintf(stderr, " %s SIGINT received, exiting server Z. Total request handled: %d, %d invertible, %d not.\n", buff, sm->req_count, sm->invertible, sm->req_count - sm->invertible);

    for (int i = 0; i < poolSizeZ; i++)
    {
        kill(workers[i].pid, SIGINT);
        wait(NULL);
    }
    free(workers);
    
    sem_destroy(&sm_mem->empty);
    sem_destroy(&sm_mem->full);
    sem_destroy(&sm_mem->critical);
    close(pipe[0]);
    close(pipe[1]);
    munmap(sm_addr, SHARED_MEMORY_SIZE);
    
    
    return 0;
}

int childProcessZ(int sleepTime, void *ptr, int poolSizeZ)
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

        if (sigintReceivedY)
        {
            break;
        }

        

        
        if (readFromSharedMemory(sm, &req, ptr) == -1)
        {
            break;
        }

        sem_post(&sm->critical);
        sem_post(&sm->empty);
        int row = req[1];
        int client_id = req[2];

        getTimeStamp(buff);
        fprintf(stderr, "%s Z: Worker PID#%d is handling client PID#%d, matrix size %dx%d, pool busy %d/%d\n", buff, getpid(), client_id, row, row, ++sm->busy, poolSizeZ);
        sleep(sleepTime);

        if (sendResponse(row, client_id, req) == -1)
        {
            free(req);
            break;
        }

        else if (sendResponse(row, client_id, req) == 1)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Worker PID#%d responding to client PID#%d: the matrix is invertible.\n", buff, getpid(), client_id);
            sm->invertible++;
            sm->req_count++;
        }

        else if (sendResponse(row, client_id, req) == 0)
        {
            getTimeStamp(buff);
            fprintf(stderr, "%s Worker PID#%d responding to client PID#%d: the matrix is not invertible.\n", buff, getpid(), client_id);
            sm->req_count++;
        }

        free(req);
        sm->busy--;
    }
    munmap(ptr, SHARED_MEMORY_SIZE);
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

void killProcesses(int size, Worker workers[])
{
    for (int i = 0; i < size; i++)
    {
        kill(workers[i].pid, SIGINT);
        wait(NULL);
    }
}

void closePipes(int size, Worker workers[])
{
    for (int i = 0; i < size; i++)
    {
        close(workers[i].pipe[0]);
        close(workers[i].pipe[1]);
    }
}

void freeMatrix(int **matrix, int row)
{
    for (int i = 0; i < row; i++)
    {
        free(matrix[i]);
    }
    free(matrix);
}