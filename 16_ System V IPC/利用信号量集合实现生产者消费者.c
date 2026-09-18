#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>

#define BUFFER_SIZE 5 // 缓冲区大小
#define NUM_ITEMS 10 // 生产和消费的总项目数

int buffer[BUFFER_SIZE];
int in = 0;
int out = 0;

int semid; // 信号量集合ID

// 定义信号量操作
void sem_wait(int semid, int index){
    struct sembuf sb;
    sb.sem_num = index;
    sb.sem_op = -1; // P操作
    sb.sem_flg = 0;
    semop(semid, &sb, 1);
}

void sem_post(int semid,int index){
    struct sembuf sb;
    sb.sem_num = index;
    sb.sem_op = 1; // V操作
    sb.sem_flg = 0;
    semop(semid, &sb, 1);
}

// 生产者线程函数
void* producer(void* arg){
    for (int i = 0; i < NUM_ITEMS;i++){
        int item = rand() & 100;
        sem_wait(semid, 0); // 等待空位

        buffer[in] = item;
        in = (in + 1) % BUFFER_SIZE;

        sem_post(semid, 1); // 增加已填充项的数量
        sleep(1);
    }
    return NULL;
}

// 消费者线程函数
void* consumer(void* arg){
    for (int i = 0; i < NUM_ITEMS;i++){
        sem_wait(semid, 1); // 等待已填充项

        int item = buffer[out];
        printf("Consumer consumed: %d\n", item);
        out = (out + 1) % BUFFER_SIZE;

        sem_post(semid, 0); // 增加空位的数量
        sleep(1);
    }
    return NULL;
}

int main(){
    pthread_t producer_thread, consumer_thread;

    // 创建信号量集合
    key_t key = ftok("semaphore_key", 65);
    semid = semget(key, 2, IPC_CREAT | 0666);

    // 初始化信号量
    unsigned short values[2] = {BUFFER_SIZE, 0}; // empty=BUFFER_SIZE, full=0
    semctl(semid, 0, SETALL, values);

    // 创建生产者和消费者线程
    pthread_create(&producer_thread, NULL, producer, NULL);
    pthread_create(&consumer_thread, NULL, consumer, NULL);

    // 等待线程完成
    pthread_join(producer_thread, NULL);
    pthread_join(consumer_thread, NULL);

    // 删除信号量集合
    semctl(semid, 0, IPC_RMID);

    return 0;
}