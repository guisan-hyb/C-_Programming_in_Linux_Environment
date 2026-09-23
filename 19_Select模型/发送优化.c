#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h> // fcntl()
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1024

typedef struct send_buffer_node
{
    char data[BUFFER_SIZE];
    size_t len;
    size_t offset;
    struct send_buffer_node *next;
} send_buffer_node_t;

typedef struct client
{
    int sockfd;
    send_buffer_node_t *send_queue_head;
    send_buffer_node_t *send_queue_tail;
} client_t;


// 设置套接字为非阻塞
int set_nonblocking(int sockfd){
    int flags;
    // 获取当前套接字的标志
    if((flags = fcntl(sockfd,F_GETFL,0)) < 0){
        perror("fcntl F_GETFL");
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

// 将数据添加到客户端的发送队列 -> push
int enqueue_send(client_t* client, const char* data, size_t len){
    send_buffer_node_t *node = (send_buffer_node_t *)malloc(sizeof(send_buffer_node_t));
    if(!node){
        perror("malloc error");
        return -1;
    }

    memcpy(node->data, data, len);
    node->len = len;
    node->offset = 0;
    node->next = NULL;

    if(client->send_queue_tail){
        client->send_queue_tail->next = node;
    }else{
        client->send_queue_head = node;
    }
    client->send_queue_tail = node;

    return 0;
}

// 从客户端的发送队列中移除已发送完成的节点 -> pop
void dequeue_sent(client_t* client){
    send_buffer_node_t *node = client->send_queue_head;
    if(node){
        client->send_queue_head = node->next;
        if(!client->send_queue_head){
            client->send_queue_tail = NULL;
        }
        free(node);
    }
}

int main(){
    int server_fd, new_socket;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);
    fd_set read_fds, write_fds;

    // 初始化所有客户端
    client_t clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS;i++){
        clients[i].sockfd = 0;
        clients[i].send_queue_head = NULL;
        clients[i].send_queue_tail = NULL;
    }

    // 创建服务器Socket
    if((server_fd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 设置非阻塞模式
    if(set_nonblocking(server_fd) < 0){
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 设置Socket选项
    int opt = 1;
    if(setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)) < 0){
        perror("setsocketopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 绑定Socket到指定端口
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    if(bind(server_fd,(struct sockaddr*)&address,sizeof(address)) < 0){
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 监听
    if(listen(server_fd,3) < 0){
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Optimized Non-blocking Select Server with Send Queue is listening on port %d...\n", PORT);

    while(1){
        // 清空文件描述符集合
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        // 将服务器Socket添加到读集合中
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        // 添加客户端Socket到读和写集合中
        for (int i = 0; i < MAX_CLIENTS;i++){
            int sd = clients[i].sockfd;
            if(sd > 0){
                FD_SET(sd, &read_fds);

                // 如果有待发送数据，则监听写事件
                if(clients[i].send_queue_head != NULL){
                    FD_SET(sd, &write_fds);
                }
            }

            if(sd>max_fd)
                max_fd = sd;
        }

        // 调用select, 等待I/O事件
        int activity = select(max_fd + 1, &read_fds, &write_fds, NULL, NULL);
        if((activity < 0)&&(errno != EINTR)){
            perror("select error");
        }

        // 如果有新连接请求，则处理
        if(FD_ISSET(server_fd,&read_fds)){
            while((new_socket = accept(server_fd,(struct sockaddr*)&address,&addr_len)) >= 0){
                printf("New connection: socket fd %d, IP %s, Port %d\n",
                       new_socket,
                       inet_ntoa(address.sin_addr),
                       ntohs(address.sin_port));
                
                // 设置新套接字为非阻塞
                if(set_nonblocking(new_socket) < 0){
                    close(new_socket);
                    continue;
                }

                // 将新Socket添加至客户端列表中
                int i = 0;
                for (i = 0; i < MAX_CLIENTS;i++){
                    if(clients[i].sockfd == 0){
                        clients[i].sockfd = new_socket;
                        clients[i].send_queue_head = NULL;
                        clients[i].send_queue_tail = NULL;
                        printf("Added to list of clients at index %d\n", i);
                        break;
                    }
                }

                // 如果没有空闲位置，关闭套接字
                if(i == MAX_CLIENTS){
                    printf("Too many clients. Connection refused\n");
                    close(new_socket);
                }
            }

            if(new_socket == -1 && errno != EAGAIN && errno != EWOULDBLOCK){
                perror("accept");
            }
        }

        // 遍历所有客户端，处理读和写事件
        for (int i = 0; i < MAX_CLIENTS;i++){
            int sd = clients[i].sockfd;
            if(sd <= 0) continue;

            // 处理可写事件
            if(FD_ISSET(sd,&write_fds)){
                while(clients[i].send_queue_tail){
                    send_buffer_node_t *node = clients[i].send_queue_head;
                    ssize_t bytes_sent = send(sd, node->data + node->offset, node->len - node->offset, 0);
                    if(bytes_sent < 0){
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            // 发送缓冲区已满，等下次可写时再试
                            break;
                        }else{
                            perror("send faild");
                            close(sd);
                            // 清空发送队列
                            while(clients[i].send_queue_head){
                                dequeue_sent(&clients[i]);
                            }
                            clients[i].sockfd = 0;
                            break;
                        }
                    }else{
                        node->offset += bytes_sent;
                        if(node->offset >= node->len){
                            // 当前数据已全部发送完毕，从队列中移除
                            dequeue_sent(&clients[i]);
                        }
                        // 如果部分数据发送完毕，等待下次可写继续发送
                        if(bytes_sent <= (node->len - node->offset)){
                            break;
                        }
                    }
                }
            }

            // 处理可读事件
            if(FD_ISSET(sd,&read_fds)){
                int valread = 0;
                while((valread = recv(sd,buffer,BUFFER_SIZE,0)) > 0){
                    printf("Received from client [%d]: %.*s\n", sd, valread, buffer);

                    // 回显数据
                    if(enqueue_send(&clients[i],buffer,valread) < 0){
                        // 如果发送队列已满或内存分配失败，断开连接
                        perror("enqueue_send falied");
                        close(sd);
                        // 清空发送队列
                        while(clients[i].send_queue_head){
                            dequeue_sent(&clients[i]);
                        }
                        clients[i].sockfd = 0;
                        break;
                    }
                }

                if(valread == 0){
                    // 客户端断开连接
                    getpeername(sd, (struct sockaddr *)&address, &addr_len);
                    printf("Host disconnected: IP %s, Port %d\n",
                           inet_ntoa(address.sin_addr),
                           ntohs(address.sin_port));
                    close(sd);

                    // 清空发送队列
                    while(clients[i].send_queue_head){
                        dequeue_sent(&clients[i]);
                    }
                    clients[i].sockfd = 0;
                }else if(valread < 0 && errno != EAGAIN && errno != EWOULDBLOCK){
                    perror("recv failed");
                    close(sd);
                    // 清空发送队列
                    while(clients[i].send_queue_head){
                        dequeue_sent(&clients[i]);
                    }
                    clients[i].sockfd = 0;
                }
                // 如果是EAGAIN，则无需处理，等待下次select通知
            }
        }
    }

    // 关闭服务器Socket（实际上，这行代码不会被执行到）
    close(server_fd);

    return 0;
}

