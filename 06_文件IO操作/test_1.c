// 利用文件IO实现文件的复制
// ● 文件名通过命令行参数指定
// ● 打开文件的方式，源文件和目的文件不同，源文件只读，目的文件不存在则创建。
// ● 判断读到源文件末尾，当读取字节数为0说明读完


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define N 1024

int main(int argc, char* argv[]){
    // 源文件描述符，目的文件描述符，读取字节数
    int fds, fdt, n;
    char buf[N];

    if(argc < 3){
        printf("Usage: %s <src_file> <dst_file> \n", argv[0]);
        return -1;
    }

    //打开源文件
    if((fds = open(argv[1],O_RDONLY|O_CREAT,0666)) < 0){
        fprintf(stderr, "open %s : %s \n", argv[1], strerror(errno));
        return -1;
    }

    //打开目的文件
    if((fdt = open(argv[2],O_WRONLY|O_CREAT,0666)) < 0){
        fprintf(stderr, "open %s : %s \n", argv[2], strerror(errno));
        close(fds);
        return -1;
    }

    //读取源文件内容
    while((n = read(fds,buf,N)) > 0){
        //向目的文件写
        write(fdt, buf, n);
    }

    close(fds);
    close(fdt);

    return 0;
}

