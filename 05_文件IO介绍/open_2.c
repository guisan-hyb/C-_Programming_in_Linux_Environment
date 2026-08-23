#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

// 以读写方式打开文件test.txt，如果文件不存在则创建，如果文件存在则报错。
int main(){
    int fd;
    if((fd=open("test.txt",O_RDWR|O_CREAT|O_EXCL,0666))<0){
        if(errno == EEXIST){
            perror("exist error!");
        }else{
            perror("other error!");
        }
    }

    return 0;
}

