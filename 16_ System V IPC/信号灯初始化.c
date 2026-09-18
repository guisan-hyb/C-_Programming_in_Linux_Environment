#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

int main(){
    key_t key = ftok("semaphore_key", 65);
    int semid = semget(key, 2, IPC_CREAT | IPC_EXCL | 0666);

    if(semid == -1){
        if(errno == EEXIST){
            semid = semget(key, 2, 0666); // 如果信号量集合已经存在，则打开它
            if(semid == -1){
                perror("semget: ");
                exit(EXIT_FAILURE);
            }
        }else{
            perror("semget: ");
            exit(EXIT_FAILURE);
        }
    }else{
        // 初始化信号量集合
        unsigned short values[2] = {2, 0}; // 第一个信号量初始化为 2，第二个信号量初始化为 0
        semctl(semid, 0, SETALL, values);
    }

    // 删除信号量集合
    semctl(semid, 0, IPC_RMID);

    return 0;
}