// client
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1" // 本地服务器

int main(){
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    const char *message = "Hello, UDP Server!";
    socklen_t addr_len = sizeof(server_addr);
    ssize_t bytes_sent, bytes_received;

    // 创建UDP套接字
    if((sockfd = socket(AF_INET,SOCK_DGRAM,0)) < 0){
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 填充服务器信息
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    // 转换服务器ip地址
    if(inet_pton(AF_INET,SERVER_IP,&server_addr.sin_addr) <= 0){
        perror("Invalid address not supported");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // 发送消息至服务器
    bytes_sent = sendto(sockfd, message, strlen(message), 0,
                        (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(bytes_sent < 0){
        perror("sendto failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Sent to Server: %s\n", message);

    // 接收服务器的响应
    bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, NULL, NULL); // 不关心源地址信息
    if(bytes_sent < 0){
        perror("recvfrom failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    buffer[BUFFER_SIZE] = '\0';
    printf("Received from server: %s\n", buffer);

    // 关闭套接字
    close(sockfd);

    return 0;
}

