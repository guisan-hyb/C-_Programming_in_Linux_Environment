#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void handler(int signo){
    if(signo == SIGINT){
        printf("I got signal: SIGINT\n");
    }
    if(signo == SIGQUIT){
        printf("I got signal: SIGQUIT\n");
    }
}

int main(){
    printf("pid: %d\n", getpid());
    signal(SIGINT, handler);
    signal(SIGQUIT, handler);

    while(1)
        pause();

    return 0;
}

