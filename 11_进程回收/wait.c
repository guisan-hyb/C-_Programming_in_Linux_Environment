#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>

int main(){
    int status;
    pid_t pid;

    if((pid = fork()) < 0){
        perror("fork");
        return -1;
    }else if(pid == 0){
        sleep(1);
        exit(2);
    }else{
        wait(&status);
        printf("%x\n", status);
    }

    return 0;
}