#include <stdio.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#define N 1024

int main(){
    key_t key;
    int shmid;
    char *addr;// 共享内存映射指针

    if((key = ftok("/tmp",'a')) == EOF) {
        perror("ftok: ");
        return -1;
    }

    if((shmid = shmget(key,N,IPC_CREAT|0666)) == EOF){
        perror("shmget: ");
        return -1;
    }

    printf("key: %x\tshmid: %d\n", key, shmid);

    addr = (char *)shmat(shmid, NULL, 0);
    if(addr == (char*)-1){
        perror("shmat: ");
        return -1;
    }

    fgets(addr, N, stdin);

    printf("shm content: %s\n", addr);
    printf("batch 2,shm content: %s\n", addr);

    return 0;
}