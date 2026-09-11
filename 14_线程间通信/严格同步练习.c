#include <semaphore.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

char buf[32];
sem_t sem_r, sem_w;
void *function(void *arg);

int main(){
    pthread_t read_thread;
    if(sem_init(&sem_r,0,0) < 0){
        perror("sem_init");
        exit(-1);
    }

    // 因为默认要有一个写信号量用来写
    if(sem_init(&sem_w,0,1) < 0){
        perror("sem init");
        exit(-1);
    }

    if(pthread_create(&read_thread,NULL,function,NULL) != 0){
        printf("failed to pthread create");
        exit(-1);
    }

    printf("input 'quit' to exit \n");
    do{
        // 申请写信号量，与其他写线程互斥
        sem_wait(&sem_w);
        fgets(buf, 32, stdin);
        // 通知唤醒其他读线程
        sem_post(&sem_r);
    } while (strncmp(buf, "quit", 4) != 0);

    return 0;
}

void* function(void* arg){
    while(1){
        // 申请读信号量，与其他读线程互斥
        sem_wait(&sem_r);
        printf("your enter %ld characters\n", strlen(buf));
        memset(buf, 0, 32);
        // 通知唤醒其他写线程
        sem_post(&sem_w);
    }
}

