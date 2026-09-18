#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include <errno.h>

int main(){
    key_t key = ftok("semaphore_key", 65);
    int semid = semget(key, 2, IPC_CREAT | IPC_EXCL | 0666);

    if(semid == -1){
        if(errno == EEXIST){ // 如果信号量集合已经存在，则打开它
            semid = semget(key, 2, 0666);
            if(semid == -1){
                perror("semget: ");
                exit(EXIT_FAILURE);
            }
        }else{
            perror("semget: ");
            exit(EXIT_FAILURE);
        }
    }else{ // 正常创建

    }

    return 0;
}
