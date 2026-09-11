#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// ● 当in和out相当时不可取出，因为没有数据可取
// ● 当(in + 1) % BUFF_SIZE == out时，不可写入，因为没有可用空间写入数据

#define BUFFER_SIZE 10
#define PRODUCE_COUNT 20
#define CONSUME_COUNT 20

int buffer[BUFFER_SIZE];// 共享缓冲区
int in = 0;// 下一个生产的位置
int out = 0;// 下一个消费的位置

// 互斥锁和条件变量
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_full = PTHREAD_COND_INITIALIZER;// 缓冲区满的条件
pthread_cond_t cond_empty = PTHREAD_COND_INITIALIZER;// 缓冲区空的条件

void* producer(void* arg){
    for (int i = 1; i <= PRODUCE_COUNT;i++){
        // 模拟生产时间
        sleep(rand() % 3);

        pthread_mutex_lock(&mutex);

        while(((in+1)%BUFFER_SIZE) == out){
            // 缓冲区满，等待消费者消费
            printf("生产者等待，缓冲区满\n");
            pthread_cond_wait(&cond_empty, &mutex);
        }

        // 生产一个产品
        buffer[in] = i;
        printf("生产者生产：%d 放入位置：%d\n", i, in);
        in = (in + 1) % BUFFER_SIZE;

        pthread_cond_signal(&cond_full);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void* consumer(void* arg){
    for (int i = 1; i <= CONSUME_COUNT;i++){
        // 模拟消费时间
        sleep(rand() % 3);

        pthread_mutex_lock(&mutex);
        while(in==out){
            // 缓冲区空，等待生产者生产
            printf("消费者等待，缓冲区空\n");
            pthread_cond_wait(&cond_full, &mutex);
        }

        // 消费一个产品
        int item = buffer[out];
        printf("消费者消费：%d 从位置：%d\n", item, out);
        out = (out + 1) % BUFFER_SIZE;

        // 通知生产者缓冲区有空位
        pthread_cond_signal(&cond_empty);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(){
    pthread_t prod_thread, cons_thread;

    // 初始化缓冲区
    for (int i = 0; i < BUFFER_SIZE;i++){
        buffer[i] = 0;
    }

    // 创建生产者和消费者线程
    pthread_create(&prod_thread, NULL, producer, NULL);
    pthread_create(&cons_thread, NULL, consumer, NULL);

    // 等待线程结束
    pthread_join(prod_thread, NULL);
    pthread_join(cons_thread, NULL);

    // 销毁互斥锁与条件变量
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_full);
    pthread_cond_destroy(&cond_empty);

    return 0;
}

