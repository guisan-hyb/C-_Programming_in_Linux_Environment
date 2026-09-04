// 主进程创建子进程，子进程睡眠1s然后退出，退出码为2，
// 主进程通过waipid阻塞等待子进程结束，主进程打印子进程返回值和结束方式

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(){
    pid_t pid;
    int status = 0;
    if((pid = fork()) < 0){
        perror("fork");
        return -1;
    }

    if(pid == 0){
        sleep(1);
        exit(2);
        return 0;
    }
    else{
        waitpid(pid, &status, 0);
        printf("子进程返回值: %d\n", WEXITSTATUS(status));
        printf("子进程是否正常结束: %d\n", WIFEXITED(status));
        exit(0);
        return 0;
    }

}