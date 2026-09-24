// poll_echo_server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_CLIENTS 1024
#define BUFFER_SIZE 1024
#define PORT 8080

// 设置文件描述符为非阻塞
int set_nonblocking(int fd){
    int flags;
    if((flags = fcntl(fd,F_GETFL,0)) < 0){
        perror("fcntl get");
        return -1;
    }

    flags |= O_NONBLOCK;
    if(fcntl(fd,F_SETFL,flags) < 0){
        perror("fcntl set");
        return -1;
    }

    return 0;
}


int main(){
    int listen_fd, conn_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct pollfd fds[MAX_CLIENTS];
    int nfds = 1, current_size = 0;
    char buffer[BUFFER_SIZE];

    // 创建监听套接字
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd == -1){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 设置套接字为非阻塞
    if(set_nonblocking(listen_fd) == -1){
        exit(EXIT_FAILURE);
    }

    // 绑定IP和端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(listen_fd,(struct sockaddr*)&server_addr,sizeof(server_addr)) < 0){
        perror("bind");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // 开始监听
    if(listen(listen_fd,SOMAXCONN) < 0){
        perror("listen");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // 初始化 pollfd 结构数组
    for (int i = 0; i < MAX_CLIENTS;i++){
        fds[i].fd = -1;// -1表示未使用
        fds[i].events = 0;
        fds[i].revents = 0;
    }

    // 添加监听套接字到 pollfd 数组
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;// 关注可读事件

    printf("Echo server is running on port %d...\n", PORT);

    // 事件循环
    while(1){
        int ret = poll(fds, nfds, -1);// 无限等待
        if(ret == -1){
            if(errno == EINTR){
                continue; // 被信号中断后继续
            }
            perror("poll");
            break;
        }

        for (int i = 0; i < nfds;i++){
            if(fds[i].fd == -1){ // 未使用的槽
                continue;
            }

            // 可处理其他事件，如错误事件、挂起事件等
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                int err_fd = fds[i].fd;
                fprintf(stderr, "Error on fd %d\n", err_fd);
                close(err_fd);
                fds[i].fd = -1;
                continue;
            }

            // 检查是否有事件发生
            if(fds[i].revents & POLLIN){
                if(fds[i].fd == listen_fd){
                    // 监听套接字有可读事件，接受新连接
                    while((conn_fd = accept(listen_fd,(struct sockaddr*)&client_addr,&client_len)) >= 0){
                        printf("Accepted connection from %s:%d, fd=%d\n",
                               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), conn_fd);
                        
                        // 设置新连接为非阻塞
                        if(set_nonblocking(conn_fd) < 0){
                            close(conn_fd);
                            continue;
                        }

                        // 在pollfd数组中添加新连接
                        int j = 1;
                        for (j = 1; j < MAX_CLIENTS;j++){
                            if(fds[j].fd == -1){
                                fds[j].fd = conn_fd;
                                fds[j].events = POLLIN;

                                if(j+1>nfds)
                                    nfds = j + 1;
                                break;
                            }
                        }

                        if(j==MAX_CLIENTS){
                            fprintf(stderr, "Too many clients\n");
                            close(conn_fd);
                        }
                    }
                    
                    if(errno != EAGAIN && errno != EWOULDBLOCK){
                        perror("accept");
                    }
                }else{
                    // 客户端套接字有可读事件，读取数据
                    int client_fd = fds[i].fd;
                    ssize_t count = read(client_fd, buffer, BUFFER_SIZE);
                    if(count == -1){
                        if(errno != EAGAIN && errno != EWOULDBLOCK){
                            perror("read");
                            close(client_fd);
                            fds[i].fd = -1;
                        }
                        continue;
                    }else if(count == 0){
                        // 客户端关闭连接
                        printf("Client on fd %d disconnected.\n", client_fd);
                        close(client_fd);
                        fds[i].fd = -1;
                        continue;
                    }else{
                        // 回写数据 echo
                        ssize_t written = 0;
                        while(written < count){
                            ssize_t w = write(client_fd, buffer + written, count - written);
                            if(w == -1){
                                if(errno == EAGAIN || errno == EWOULDBLOCK){
                                    break;
                                }
                                perror("write");
                                close(client_fd);
                                fds[i].fd = -1;
                                break;
                            }
                            written += w;
                        }
                    }
                }
            }
        }
    }

    // 清理资源
    for (int i = 0; i < nfds;i++){
        if(fds[i].fd != -1){
            close(fds[i].fd);
        }
    }

    return 0;
}

