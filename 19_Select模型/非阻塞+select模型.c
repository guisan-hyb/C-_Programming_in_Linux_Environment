#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1024

int set_nonblocking(int sockfd){
    int flags;
    if((flags = fcntl(sockfd,F_GETFL,0)) < 0){
        perror("fcntl F_GETFL");
        return -1;
    }

    flags |= O_NONBLOCK;

    if(fcntl(sockfd,F_SETFL,flags) < 0){
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}

int main(){
    int server_fd, new_socket, client_socket[MAX_CLIENTS];
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE];
    fd_set read_fds, write_fds;
    socklen_t addr_len = sizeof(address);

    // 初始化客户端套接字为0
    for (int i = 0; i < MAX_CLIENTS;i++){
        client_socket[i] = 0;
    }

    // 创建服务器Socket
    if((server_fd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("socket");
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
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 绑定Socket
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

    printf("Server listening on port %d...\n", PORT);

    while(1){
        // 清空文件描述符集合
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        // 添加服务器Socket到读集合
        FD_SET(server_fd, &read_fds);
        int max_sd = server_fd;

        // 添加客户端Socket到集合
        for (int i = 0; i < MAX_CLIENTS;i++){
            int sd = client_socket[i];
            if(sd > 0){
                FD_SET(sd, &read_fds);
                // 可选：根据需要添加写集合
                // FD_SET(sd, &write_fds);
            }
            if(sd > max_sd)
                max_sd = sd;
        }

        // 等待I/O事件
        int activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);
        if((activity < 0)&&(errno!=EINTR)){
            perror("select error");
        }

        // 如果有新连接
        if(FD_ISSET(server_fd,&read_fds)){
            while((new_socket = accept(server_fd,(struct sockaddr*)&address,&addr_len)) >= 0){
                printf("New Connection: socket fd %d, IP %s, Port %d\n",
                       new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                // 设置新套接字为非阻塞
                if(set_nonblocking(new_socket) < 0){
                    close(new_socket);
                    continue;
                }

                // 添加到客户端列表
                int i = 0;
                for (i = 0; i < MAX_CLIENTS; i++)
                {
                    if(client_socket[i]==0){
                        client_socket[i] = new_socket;
                        printf("Added to list of client's at index of %d\n", i);
                        break;
                    }
                }

                // 如果未找到空位，关闭连接
                if(i == MAX_CLIENTS){
                    printf("Too many clients. Connection refused\n");
                    close(new_socket);
                }
            }

            if (new_socket == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                perror("accept");
            }
        }

        // 处理客户端Socket的I/O
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            int sd = client_socket[i];
            if (sd <= 0)
                continue;

            if (FD_ISSET(sd, &read_fds))
            {
                // 非阻塞读，必须用 while 循环榨干缓冲区
                while (1)
                {
                    int valread = recv(sd, buffer, BUFFER_SIZE, 0);
                    if (valread > 0)
                    {
                        buffer[valread] = '\0'; 
                        printf("Received from client [%d]: %s\n", sd, buffer);

                        // 回显数据 (注意：非阻塞 send 需要更复杂的处理，这里仅作简单演示)
                        int sent = send(sd, buffer, valread, 0);
                        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                        {
                            // 实际工程中：这里要把未发送的数据存入该客户端的发送缓冲区
                            // 并将 fd 加入 write_fds 集合
                            printf("Send buffer full, data might be lost in this demo.\n");
                        }
                        else if (sent < 0)
                        {
                            perror("send failed");
                            close(sd);
                            client_socket[i] = 0;
                            break;
                        }
                    }
                    else if (valread == 0)
                    {
                        // 客户端断开连接
                        getpeername(sd, (struct sockaddr *)&address, &addr_len);
                        char client_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN); // ✅ 线程安全
                        printf("Host disconnected: IP %s, Port %d\n", client_ip, ntohs(address.sin_port));
                        close(sd);
                        client_socket[i] = 0;
                        break; 
                    }
                    else // valread < 0
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break; // 数据读完了，正常退出 while 循环
                        }
                        else
                        {
                            perror("recv failed");
                            close(sd);
                            client_socket[i] = 0;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    close(server_fd);

    return 0;
}

