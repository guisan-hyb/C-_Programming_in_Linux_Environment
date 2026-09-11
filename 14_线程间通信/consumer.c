#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define SEM_NAME "/my_named_semaphore"
#define CONSUME_COUNT 10

int main(){
    // 打开已存在的有名信号量
    sem_t *sem = sem_open(SEM_NAME, 0);
    if(sem==SEM_FAILED){
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i <= CONSUME_COUNT;i++){
        // 等待信号，表示有新产品可消费
        if(sem_wait(sem)==-1){
            perror("sem_wait");
            sem_close(sem);
            exit(EXIT_FAILURE);
        }

        // 模拟消费时间
        sleep(rand() % 2 + 1);

        printf("消费者消费产品: %d\n", i);
    }

    // 关闭并解除信号量
    if(sem_close(sem)==-1){
        perror("sem_close");
        exit(EXIT_FAILURE);
    }

    // 解除信号量链接，建议由生产者或主控进程执行
    if(sem_unlink(SEM_NAME)==-1){
        perror("sem_unlink");
        // 不退出程序，因为解除链接失败不影响当前操作
    }

    printf("消费者完成消费，退出\n");

    return 0;
}

