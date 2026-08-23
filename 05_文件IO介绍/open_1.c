// #include <fcntl.h>
// int open(const char *path, int oflag, ...);
//
// ● 成功时返回文件描述符，出错时返回EOF
// ● 打开文件时使用两个参数
// ● 创建文件时使用第三个参数指定新文件的权限
// ● 设备文件只能通过open打开，而不能通过open创建

#include <stdio.h>
#include <fcntl.h>

// 以只写方式打开文件test.txt， 如果文件不存在则创建，如果存在则清空
int main(){
    int fd;
    if((fd = open("test.txt",O_WRONLY|O_CREAT|O_TRUNC,0666))<0){
        perror("open");
        return -1;
    }

    return 0;
}

