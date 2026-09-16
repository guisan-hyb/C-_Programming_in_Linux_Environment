#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>


int main(){
    char buf[1024];
    int pid, status, pfd[2];

    if(pipe(pfd) == EOF) {
        perror("pipe:");
        return -1;
    }

    close(pfd[0]); // 关闭读端，同时也让fork后的子进程的读端是关闭的

    pid = fork();
    if(pid==0){
        write(pfd[1], buf, 1024);
    }else{
        wait(&status);
    }

    printf("sub process status: %x\n", status);

    return 0;
}
