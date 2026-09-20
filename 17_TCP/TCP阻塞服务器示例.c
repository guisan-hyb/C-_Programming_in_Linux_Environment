// server
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int main(){
    int server_fd, new_socket;
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE] = {0};
    const char *response = "Hello, Client";
    socklen_t addr_len = sizeof(address);

    // 创建socket
    if((server_fd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 绑定
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;// 监听所有端口

    if(bind(server_fd,(struct sockaddr*)&address,sizeof(address)) < 0){
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 监听
    if(listen(server_fd,3) < 0){ // 队列长度设置为3
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on Port: %d...\n", PORT);

    // 接收连接
    if((new_socket = accept(server_fd,(struct sockaddr*)&address,&addr_len)) < 0){
        perror("accept failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 接收数据
    ssize_t bytes_received = recv(new_socket, buffer, BUFFER_SIZE - 1, 0);
    if(bytes_received < 0){
        perror("recv failed");
    }else{
        buffer[BUFFER_SIZE] = '\0';
        printf("Received from client: %s\n", buffer);
    }

    // 发送数据
    if(send(new_socket,response,strlen(response),0) < 0){
        perror("send failed");
    }else{
        printf("Response sent to client\n");
    }

    // 关闭连接
    close(server_fd);
    close(new_socket);

    return 0;
}

