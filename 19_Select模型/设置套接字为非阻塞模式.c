// 利用 fcntl 函数修改套接字的文件描述符属性

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>

// 设置套接字为非阻塞模式
int set_nonblocking(int sockfd) {
    int flags;

    // 获取当前套接字的标志
    if((flags = fcntl(sockfd,F_GETFL,0)) < 0){
        perror("fcnt F_GETFL");
        return -1;
    }

    // 设置非阻塞标志
    flags |= O_NONBLOCK;
    if(fcntl(sockfd,F_SETFL,flags) < 0){
        perror("fcntl F_SETFL");
        return -1;
    }

    return 0;
}

// 在创建套接字后设置为非阻塞
int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0){
        perror("socket");
        return -1;
    }

    if(set_nonblocking(sockfd) < 0){
        perror("sockfd");
        return -1;
    }

    // 现在 sockfd 是非阻塞的
    // 后续操作...

    close(sockfd);

    return 0;
}

