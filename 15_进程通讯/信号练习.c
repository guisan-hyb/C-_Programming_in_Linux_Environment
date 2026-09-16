// 实现一个程序，
// ● 主函数捕获SIGALRM信号，回调函数里打印 "receive SIGALRM signal"。
// ● 捕获SIGCONT信号，打印 "receive SIGCONT signal"
// ● 主函数启动定时器10s倒计时，并且调用pause() 暂停等待
// ● 主函数被唤醒后，打印“I am week up.”。

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

void sig_handler(int sig){
    if(sig == SIGCONT){
        printf("receive SIGCONT signal\n");
    }
    if(sig == SIGALRM){
        printf("receive SIGALRM signal\n");
    }
}

int main(){
    signal(SIGCONT, sig_handler);
    signal(SIGALRM, sig_handler);
    printf("pid: %d\n", getpid());
    alarm(10);
    pause();
    printf("I am wake up\n");

    return 0;
}

