#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int main(){
    key_t key;
    int shmid;

    key = ftok(".", 'm');
    if(key==EOF) {
        perror("ftok: ");
        return -1;
    }
    printf("key: %x\n", key);

    shmid = shmget(key, 1024, IPC_CREAT | 0666);
    if(shmid == EOF){
        perror("shmget: ");
        return -1;
    }
    printf("shmid: %d\n", shmid);

    return 0;
}