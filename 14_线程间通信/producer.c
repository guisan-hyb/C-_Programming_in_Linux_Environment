// producer.c
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>    // For O_* constants
#include <sys/stat.h> // For mode constants
#include <unistd.h>
#include <errno.h>

#define SEM_NAME "/my_named_semaphore"
#define PRODUCE_COUNT 10

int main()
{
    // 在 main 函数开头，确保每次运行都是干净的状态 -> 测试代码中可用，在多进程协作的实际生产环境中，不能随便 unlink
    sem_unlink(SEM_NAME);

    // 创建有名信号量，初始值为0
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0644, 0);
    if (sem == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i <= PRODUCE_COUNT; i++)
    {
        // 模拟生产时间
        sleep(rand() % 3 + 1); // 1-3秒

        printf("生产者生产产品 %d\n", i);

        // 发送信号，表示有新产品可供消费
        if (sem_post(sem) == -1)
        {
            perror("sem_post");
            sem_close(sem);
            exit(EXIT_FAILURE);
        }
    }

    // 关闭信号量
    if (sem_close(sem) == -1)
    {
        perror("sem_close");
        exit(EXIT_FAILURE);
    }

    printf("生产者完成生产，退出。\n");
    return 0;
}