#include <semaphore.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

char buf[32];
sem_t sem;
void *function(void *arg);

int main(){
    pthread_t read_thread;
    if(sem_init(&sem,0,0) < 0){
        perror("sem_init");
        exit(-1);
    }

    if(pthread_create(&read_thread,NULL,function,NULL) != 0){
        printf("failed to pthread create");
        exit(-1);
    }

    printf("input 'quit' to exit\n");
    // 主线程从输入读取内容写入buf
    do{
        fgets(buf, 32, stdin);
        sem_post(&sem);
    } while (strncmp(buf, "quit", 4) != 0);

    return 0;
}

void* function(void* arg){
    while(1){
        sem_wait(&sem);
        printf("your enter %ld characters\n", strlen(buf));
        memset(buf, 0, 32);
    }
}

