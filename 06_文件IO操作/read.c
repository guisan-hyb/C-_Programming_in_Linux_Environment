// read函数用来从文件中读取数据：
// #include <unistd.h>
//     ssize_t read(int fd, void *buf, size_t count);

// ● 成功时返回实际读取的字节数，出错时返回EOF
// ● 读到文件末尾时返回0
// ● buf是接受数据的缓冲区
// ● count不应超过buf大小


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

// 从指定的文件(文本文件)中读取内容，并统计大小

int main(int argc,char* argv[]){
    // fd为文件描述符, n为读取字节数,total为总字节数
    int fd, n, total = 0;
    char buf[64];

    if(argc<0){
        printf("Usage: %s <file> \n", argv[0]);
        return -1;
    }

    if((fd = open(argv[1],O_RDONLY))<0){
        perror("open");
        return -1;
    }

    while((n = read(fd,buf,64))>0){
        total += n;
    }

    printf("文件大小为: %d\n", total);

    return 0;
}

