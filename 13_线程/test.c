// 定义一个字符数组"Hello World!"，通过线程修改这个字符数组内容为”Hello IT Heima“，
// 然后返回修改后的值，要求打印返回值和字符数组的内容

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

extern void *thread_func(void *arg);

int main(){
    char message[32] = "Hello World";
    pthread_t work_thread;
    void *result;

    if(pthread_create(&work_thread,NULL,thread_func,message) != 0){
        printf("failed to pthread_func");
        exit(-1);
    }

    pthread_join(work_thread, &result);

    printf("result is %s\n", (char *)result);
    printf("message is %s\n", message);

    return 0;
}

void* thread_func(void* arg){
    char *msg = (char *)arg;
    memset(msg, 0, strlen(msg));
    strcpy(msg, "Hello IT HeiMa");
    pthread_exit(msg);
}

