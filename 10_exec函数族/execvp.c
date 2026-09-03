#include <stdio.h>
#include <unistd.h>

int main(){
    char *arg[] = {"ls", "-a", "-l", "/etc", NULL};
    if(execv("/bin/ls",arg) < 0){
        perror("execv");
        return -1;
    }

    if(execvp("ls",arg) < 0){
        perror("execvp");
        return -1;
    }

    return 0;
}

