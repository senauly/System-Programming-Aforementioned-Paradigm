#include "server.h"

#define SHARED_MEMORY_NAME "/shared_memory"
#define NO_EINTR(stmt)                   \
    while ((stmt) < 0 && errno == EINTR) \
        ;

sig_atomic_t sigintReceived = 0;

void handleInterrupt(int signal_num){
    if(signal_num == SIGINT)
        sigintReceived = 1;
}

void *initSharedMemory(int size){
    struct sigaction interruptHandle;
    interruptHandle.sa_handler = handleInterrupt;
    char buff[26];

    if(sigaction(SIGINT, &interruptHandle, NULL) == -1){
        getTimeStamp(buff);
        printf("%s ", buff);
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    int fd;
    fd = shm_open(SHARED_MEMORY_NAME, O_RDWR | O_CREAT, 0666);

    
    if(fd == -1){
        getTimeStamp(buff);
        fprintf(stderr,"%s ", buff);
        perror("shm_open:");
        exit(EXIT_FAILURE);
    }
    if(ftruncate(fd, size) == -1){
        getTimeStamp(buff);
        fprintf(stderr,"%s ", buff);
        perror("ftruncate:");
        close(fd);
        exit(EXIT_FAILURE);
    } 
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(addr == MAP_FAILED){
        getTimeStamp(buff);
        fprintf(stderr,"%s ", buff);
        perror("mmap:");
        close(fd);
        shm_unlink(SHARED_MEMORY_NAME);
        exit(EXIT_FAILURE);
    }

    SharedMemory sm;
    sm.size = size;
    sm.fd = fd;
    sm.busy = 0;
    sm.invertible = 0;
    sm.req_count = 0;
    initQueue(&(sm.queue),0);
    
    memcpy(addr, &sm, sizeof(SharedMemory));
    
    if(sigintReceived){
        getTimeStamp(buff);
        fprintf(stderr,"%s: SIGINT received, exiting server Z. Total request handled 0.\n", buff);
        munmap(addr, size);
        close(fd);
        shm_unlink(SHARED_MEMORY_NAME);
        
        exit(EXIT_SUCCESS);
    }

    return addr;
}

int findPlace(SharedMemory *sm, int reqSize){
    char buff[26];
    if(sm->size < reqSize){
        getTimeStamp(buff);
        return -1;
    }

    if(is_empty(&(sm->queue))){
        return sizeof(SharedMemory);
    }

    //total size - rear node address = available place for new node
    if(sm->size - sm->queue.nodes[sm->queue.rear].end_address >= reqSize){
        return sm->queue.nodes[sm->queue.rear].end_address;
    }

    //front node address - total size = available place for new node
    if(sm->queue.nodes[sm->queue.front].start_address - sm->size >= reqSize){
        return sm->queue.nodes[sm->queue.front].start_address - reqSize;
    }

    else{
        return -1;
    }    
}

int addToSharedMemory(SharedMemory *sm, int size, int *request, void *ptr){
    int place = findPlace(sm, size);
    char buff[26];

    
    if(place == -1){
        getTimeStamp(buff);
        fprintf(stderr,"%s: There is no available space in the shared memory for this request.\n", buff);
        return -1;
    }
   
    //add to queue
    int dataStart = place;
    int dataEnd = place + size;
    enqueue(&(sm->queue), dataStart, dataEnd);

    //copy data to shared memory
    if(memcpy(ptr + dataStart, request, size) < 0){
        getTimeStamp(buff);
        fprintf(stderr,"%s ", buff);
        perror("memcopy");
        return -1;
    }
    return 1;
}

int readFromSharedMemory(SharedMemory *sm, int **request, void *ptr){
    int dataStart, dataEnd;
    //remove from queue

    if(dequeue(&(sm->queue), &dataStart, &dataEnd) != -1){
        //copy data from shared memory
        *request = (int *)malloc(sizeof(int) * (dataEnd - dataStart));
        memcpy(*request, ptr + dataStart, dataEnd - dataStart);
        return 1;
    }
    
    else{
        return -1;
    }
}

