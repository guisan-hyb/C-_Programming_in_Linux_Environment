// 创建守护进程，每隔1s将系统时间写入文件time.log中

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

int main(){
    pid_t pid;
    FILE *fp;
    time_t t;
    
    if((pid = fork()) < 0){
        perror("fork");
        exit(-1);
        return -1;
    }

    if(pid > 0){
        exit(0);
        return 0;
    }

    setsid();
    umask(0);
    chdir("/tmp");
    // getdtablesize 是一个用于获取进程文件描述符表大小的函数。
    // 它返回进程可以打开的最大文件数量，比文件描述符的最大可能值多一个
    for (int i = 0; i < getdtablesize(); i++)
    {
        close(i);
    }

    if((fp = fopen("time.log","a")) == NULL){
        perror("open");
        exit(-1);
        return 0;
    }

    while(1){
        time(&t);
        fprintf(fp, "%s", ctime(&t));
        fflush(fp);
        sleep(1);
    }

    return 0;
}
