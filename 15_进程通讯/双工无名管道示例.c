#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

int main(){
    int pid, pfd1[2], pfd2[2];
    char buf1[1024], buf2[1024];

    if(pipe(pfd1) == EOF) {
        perror("pipe1: ");
        return -1;
    }

    if(pipe(pfd2) == EOF){
        perror("pipe2: ");
        return -1;
    }

    pid = fork();

    if(pid == 0){
        strncpy(buf1, "sub process write", 1024);
        write(pfd1[1], buf1, 1024);

        read(pfd2[0], buf2, 1024);
        printf("%s\n", buf2);
    }else{
        read(pfd1[0], buf1, 1024);
        printf("%s\n", buf1);

        strncpy(buf2, "father process write", 1024);
        write(pfd2[1], buf2, 1024);
        waitpid(pid, NULL, 0);
    }

    return 0;
}