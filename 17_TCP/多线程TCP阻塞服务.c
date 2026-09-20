// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define BACKLOG 10 // 增加监听队列长度

void *handle_client(void *arg);

int main(){
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    // 创建socket
    if((server_fd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 设置 Socket 选项
    int opt = 1;
    if(setsockopt(server_fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt)) < 0){
        perror("setsocket failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 绑定
    if(bind(server_fd,(struct sockaddr*)&address,sizeof(address)) < 0){
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 监听
    if(listen(server_fd,BACKLOG) < 0){
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on Port: %d...\n", PORT);

    while(1){
        // 接收连接
        if((new_socket = accept(server_fd,(struct sockaddr*)&address,&addr_len)) < 0){
            perror("accept failed");
            continue;// 继续接收下一个连接，不退出
        }

        // 输出连接信息
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        printf("Accept connection from %s:%d\n", client_ip, ntohs(address.sin_port));

        // 为每个客户端创建一个新的线程
        pthread_t thread_id;
        int *pclient = malloc(sizeof(int));
        if(pclient == NULL){
            perror("Failed to allocate memory for client socket");
            close(new_socket);
            continue;
        }

        *pclient = new_socket;

        if(pthread_create(&thread_id,NULL,handle_client,pclient) != 0){
            perror("Failed to creat thread");
            free(pclient);
            close(new_socket);
            continue;
        }

        // 分离线程，确保资源在线程结束后被释放
        pthread_detach(thread_id);
    }

    // 关闭服务器 Socket（虽然这行代码通常不会被执行到）
    close(server_fd);

    return 0;
}

void* handle_client(void* arg){
    int client_socket = *((int *)arg);
    free(arg); // 释放动态分配的内存

    char buffer[BUFFER_SIZE];
    const char *response = "Hello, Client!";
    
    while(1){
        // 接收数据
        ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if(bytes_received < 0){
            perror("recv failed");
            break;
        }else if(bytes_received == 0){
            printf("Client disconnected\n");
            break;
        }else{
            buffer[bytes_received] = '\0';
            printf("Received from client [%d]: %s\n", client_socket, buffer);
        }

        // 发送数据
        if(send(client_socket,response,strlen(response),0) < 0){
            perror("send failed");
            break;
        }else{
            printf("Response sent to client [%d]\n", client_socket);
        }
    }

    // 关闭客户端连接
    close(client_socket);
    printf("Connection with client [%d] closed\n", client_socket);
    pthread_exit(NULL);
}

