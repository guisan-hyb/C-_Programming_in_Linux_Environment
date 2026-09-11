#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_ITERATIONS 1000000

int shared_counter = 0;

// 互斥锁
pthread_mutex_t mutex;

void *increment_counter(void* arg){
    for (int i = 0; i < NUM_ITERATIONS;i++){
        pthread_mutex_lock(&mutex);// 加锁
        shared_counter++;// 访问共享变量
        pthread_mutex_unlock(&mutex);// 解锁
    }
    return NULL;
}

int main(){
    pthread_t thread1, thread2;

    // 初始化互斥锁
    pthread_mutex_init(&mutex, NULL);

    // 创建两个线程
    pthread_create(&thread1, NULL, increment_counter, NULL);
    pthread_create(&thread2, NULL, increment_counter, NULL);

    // 等待线程结束
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    // 输出结果
    printf("Final value of shared_counter: %d\n", shared_counter);

    // 销毁互斥锁
    pthread_mutex_destroy(&mutex);

    return 0;
}

