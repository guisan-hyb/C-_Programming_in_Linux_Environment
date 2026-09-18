#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>

#define N 1024

int main(){
    int shmid;
    key_t key;
    char *buf;// shm addr pointer

    key = ftok("/tmp", 's');
    if(key==EOF){
        perror("ftok: ");
        return -1;
    }

    shmid = shmget(key, N, IPC_CREAT | 0666);
    if(shmid==EOF){
        perror("shmid: ");
        return -1;
    }

    buf = (char *)shmat(shmid, NULL, 0);
    if(buf == (char*)-1) {
        perror("shmat: ");
        return -1;
    }

    printf("please input message, enter quit to exit\n");

    do{
        fgets(buf, N, stdin);
        if(strncmp(buf,"quit",4)==0) break;
    } while (strncmp(buf, "quit", 4) != 0);

    printf("writer exit\n");

    shmdt(buf);

    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}

