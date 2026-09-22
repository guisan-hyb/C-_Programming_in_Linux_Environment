#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS FD_SETSIZE // 通常为 1024

int main()
{
    int server_fd, new_socket, client_socket[MAX_CLIENTS];
    struct sockaddr_in address;
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    int max_sd, sd, activity, valread;
    socklen_t addr_len = sizeof(address);

    // 初始化所有客户端套接字为0
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_socket[i] = 0;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 128) < 0)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Select Server is listening on port %d...\n", PORT);

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_socket[i];
            if (sd > 0)
            {
                FD_SET(sd, &read_fds);
            }
            if (sd > max_sd)
            {
                max_sd = sd;
            }
        }

        activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR))
        {
            perror("select error");
            // 实际工程中不应直接 exit，应记录日志并尝试恢复或优雅退出
        }

        // 处理新连接
        if (FD_ISSET(server_fd, &read_fds))
        {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addr_len)) < 0)
            {
                perror("accept");
                continue; 
            }

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
            printf("New connection: socket fd %d, IP %s, Port %d\n",
                   new_socket, client_ip, ntohs(address.sin_port));

            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_socket[i] == 0)
                {
                    client_socket[i] = new_socket;
                    printf("Added to list of clients at index %d\n", i);
                    break;
                }
            }
        }

        // 处理客户端 IO
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            sd = client_socket[i];

            if (FD_ISSET(sd, &read_fds))
            {
                valread = read(sd, buffer, BUFFER_SIZE);

                // 全面处理 read 的返回值
                if (valread == 0)
                {
                    // 客户端主动断开
                    getpeername(sd, (struct sockaddr *)&address, &addr_len);
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
                    printf("Host disconnected: IP %s, Port %d\n",
                           client_ip, ntohs(address.sin_port));

                    close(sd);
                    client_socket[i] = 0;
                }
                else if (valread < 0)
                {
                    // 发生错误 (如对端暴力断开导致 RST)
                    perror("read error");
                    close(sd);
                    client_socket[i] = 0;
                }
                else
                {
                    // 正常收到数据
                    buffer[valread] = '\0';
                    printf("Received from client [%d]: %s\n", sd, buffer);
                    send(sd, buffer, valread, 0);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}