#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(){
    char buf[1024];
    int pfd[2], count = 0;

    if(pipe(pfd) == EOF) {
        perror("pipe:");
        return -1;
    }

    while(1){
        // 如果读端pfd[0] 没有close则有读端，有读端，如果空间不够写，则阻塞。
        write(pfd[1], buf, 1024);
        printf("write %dk bytes\n", ++count);
    }

    return 0;
}
