#include <unistd.h>
#include <stdio.h>

// 执行ls命令，显示/etc目录下所有文件的详细信息

int main(){
    if(execl("/bin/ls","-a","-l","/etc",NULL) < 0){
        perror("execl");
        return -1;
    }

    if(execlp("ls","ls","-a","-l","/etc",NULL) < 0){
        perror("execlp");
        return -1;
    }

    return 0;
}

