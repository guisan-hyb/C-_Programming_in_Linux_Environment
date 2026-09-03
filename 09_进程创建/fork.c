#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>

int main(){
    pid_t pid;// 进程号
    if((pid = fork()) < 0){
        perror("fork");
        return -1;
    }else if(pid == 0){ // 当前处于子进程
        printf("Child process: my pid is %d\n", getpid());
    }else{ // 当前处于父进程
        printf("Parent process: my pid is %d\n", getpid());
    }

    return 0;
}

