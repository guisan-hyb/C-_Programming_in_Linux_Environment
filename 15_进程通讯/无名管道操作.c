// 子进程1和子进程2负责向管道写入
// 父进程负责读管道
// 涉及3个进程
// 一定要先创建管道，后创建子进程，否则无法继承

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int main(){
    int pid1, pid2;
    char buf[1024]; // 缓冲区
    int pfd[2]; // 存放管道读写描述符
    if(pipe(pfd) == EOF) {
        perror("pipe: ");
        return -1;
    }

    pid1 = fork();
    if(pid1 == 0){
        // 子进程1执行
        strncpy(buf, "I am process 1.", 1024);
        write(pfd[1], buf, 1024);
    }else{
        // 父进程执行
        pid2 = fork(); // 创建子进程2
        if(pid2 == 0){
            // 子进程2执行
            sleep(1);
            strncpy(buf, "I am process 2.", 1024);
            write(pfd[1], buf, 1024);
        }else{
            // 父进程执行
            // 等待任意一个子进程结束, wait函数在<sys/wait.h>内
            wait(NULL);
            read(pfd[0], buf, 1024);
            printf("%s\n", buf);
            wait(NULL);
            read(pfd[0], buf, 1024);
            printf("%s\n", buf);
        }
    }

    return 0;
}