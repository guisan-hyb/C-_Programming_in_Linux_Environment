// epoll_echo_server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define MAX_EVENTS 1000
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
    int listen_fd, conn_fd, epoll_fd, nfds;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct epoll_event event, events[MAX_EVENTS];
    char buffer[BUFFER_SIZE];

    // 1. 创建监听套接字
    if((listen_fd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. 设置套接字为非阻塞
    if(set_nonblocking(listen_fd) < 0){
        exit(EXIT_FAILURE);
    }

    // 3. 绑定IP和端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;// 监听所有IP

    if(bind(listen_fd,(struct sockaddr*)&server_addr,sizeof(server_addr)) < 0){
        perror("bind");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // 4. 开始监听
    if(listen(listen_fd,SOMAXCONN) < 0){
        perror("listen");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // 5. 创建epoll实例
    epoll_fd = epoll_create1(0);
    if(epoll_fd < 0){
        perror("epoll_create1");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    // 6. 添加监听套接字到epoll
    event.events = EPOLLIN;// 监听可读事件
    event.data.fd = listen_fd;
    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,listen_fd,&event) < 0){
        perror("epoll_ctl: listen_fd");
        close(listen_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    printf("Echo server is running on port %d...\n", PORT);

    // 7. 事件循环
    while(1){
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if(nfds == -1){
            if(errno == EINTR){
                continue; // 中断后继续
            }
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds;i++){
            if(events[i].data.fd == listen_fd){
                // 有新的连接到来
                while((conn_fd = accept(listen_fd,(struct sockaddr*)&client_addr,&client_len)) >= 0){
                    printf("Accepted connection from %s:%d\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    
                    // 设置新连接为非阻塞
                    if(set_nonblocking(conn_fd) < 0){
                        close(conn_fd);
                        continue;
                    }

                    // 将新连接添加到epoll
                    event.events = EPOLLIN | EPOLLET;
                    event.data.fd = conn_fd;
                    if(epoll_ctl(epoll_fd,EPOLL_CTL_ADD,conn_fd,&event) < 0){
                        perror("epoll_ctl: conn_fd");
                        close(conn_fd);
                        continue;
                    }
                }

                if(errno != EAGAIN && errno != EWOULDBLOCK){
                    perror("accept");
                }
            }else{
                // 处理客户端数据
                int client_fd = events[i].data.fd;
                ssize_t count;

                while((count = read(client_fd,buffer,BUFFER_SIZE)) > 0){
                    // 回写数据
                    ssize_t written = 0;
                    while(written < count){
                        ssize_t w = write(client_fd, buffer + written, count - written);
                        if(w < 0){
                            if(errno == EAGAIN || errno == EWOULDBLOCK){
                                break;
                            }
                            perror("write");
                            close(client_fd);
                            break;
                        }
                        written += w;
                    }
                }

                if(count == -1){
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break; // 读完了，正常退出
                    if (errno == EINTR)
                        continue;         // 被信号中断，继续读
                    perror("read error"); // 真实错误
                    close(client_fd);
                    break;
                }

                if(count == 0){
                    // 客户端关闭连接
                    printf("Client on fd %d disconnected\n", client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL); // 显式从 epoll 移除
                    close(client_fd);
                }
            }
        }
    }

    close(listen_fd);
    close(epoll_fd);

    return 0;
}

