// 以读写方式打开文件test.txt， 如果文件不存在则创建，如果存在则追加，返回描述符后，关闭描述符并结束

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(){
    int fd;
    if((fd=open("test.txt",O_RDWR|O_CREAT|O_APPEND))<0){
        perror("文件打开失败");
        return -1;
    }
    close(fd);

    return 0;
}