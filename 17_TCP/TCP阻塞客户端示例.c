// client
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
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};
    const char *message = "Hello, Server!";

    // 创建Socket
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 定义服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // 将IPv4地址从文本转换为二进制
    if(inet_pton(AF_INET,"127.0.0.1",&server_addr.sin_addr) <= 0){
        perror("invalid address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 连接服务器
    if(connect(sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr)) < 0){
        perror("connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 发送数据
    if(send(sockfd,message,strlen(message),0) < 0){
        perror("send failed");
    }else{
        printf("Message sent to server\n");
    }

    // 接收数据
    ssize_t bytes_received = recv(sockfd,buffer,BUFFER_SIZE-1,0);
    if(bytes_received < 0){
        perror("recv failed");
    }else if(bytes_received == 0){
        printf("Connection closed by server");
    }else{
        buffer[bytes_received] = '\0';
        printf("Received from server: %s\n", buffer);
    }

    // 关闭连接
    close(sockfd);

    return 0;
}

