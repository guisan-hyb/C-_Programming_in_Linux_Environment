// write函数用来向文件写入数据：
// #include <unistd.h>
//     ssize_t write(int fd, void *buf, size_t count);
//
// ● 成功时返回实际写入的字节数，出错时返回EOF
// ● buf是发送数据的缓冲区
// ● count不应超过buf大小


#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#define BUFF_SIZE 20

// 将键盘输入的内容写入文件，直到输入quit

int main(int argc, char* argv[]){
    int fd;// 文件描述
    char buf[BUFF_SIZE]; // 每次从标准输入流读取的内容

    if(argc<2){
        printf("Usage: %s <file> \n", argv[0]);
        return -1;
    }

    if((fd = open(argv[1],O_WRONLY|O_CREAT|O_TRUNC,0666)) < 0){
        perror("open");
        return -1;
    }

    while(fgets(buf,BUFF_SIZE,stdin) != NULL){
        if(strcmp("quit\n",buf) == 0){
            break;
        }

        // 写入文件，写的长度取决于buf内容
        write(fd, buf, strlen(buf));
        // 清空buf内容
        memset(buf, 0, BUFF_SIZE);
    }
    close(fd);

    return 0;
}

