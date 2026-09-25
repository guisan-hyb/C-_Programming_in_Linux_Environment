#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>


#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

// 从 fd 上完整地读 count 字节到 buf，直到读满或出错
static ssize_t read_full(int fd, void*buf, size_t count){
    size_t offset = 0;
    while(offset < count){
        ssize_t n = read(fd, (char *)buf + offset, count - offset);
        if(n < 0){
            if(errno == EINTR) continue;
            return -1;
        }

        if(n == 0){
            // 对端关闭
            return offset;
        }

        offset += n;
    }
    return offset;
}

int main(){
    int sockfd;
    struct sockaddr_in srv_addr;

    // 1. 创建 socket
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. 连接服务器
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(SERVER_PORT);
    if(inet_pton(AF_INET,SERVER_IP,&srv_addr.sin_addr) <= 0){
        perror("inet_pton");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd,(struct sockaddr*)&srv_addr,sizeof(srv_addr)) < 0){
        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to %s:%d\n", SERVER_IP, SERVER_PORT);

    // 3. 读用户输入-发送-接收-显示 循环
    while (1)
    {
        char *line = NULL;
        size_t len = 0;
        printf("> ");
        fflush(stdout);

        // getline 会分配或扩展 buffer
        if(getline(&line,&len,stdin) < 0){
            // EOF 或出错
            break;
        }

        // 去掉末尾换行
        size_t body_len = strcspn(line, "\r\n");
        line[body_len] = '\0';

        // 4 字节头 + payload
        uint16_t msg_type = 1;// 可根据协议修改
        uint16_t net_type = htons(msg_type);
        uint16_t net_len = htons(body_len);
        size_t total_len = sizeof(net_type) + sizeof(net_len) + body_len;
        uint8_t *buf = malloc(total_len);
        if(!buf){
            perror("malloc");
            free(line);
            break;
        }

        memcpy(buf, &net_type, sizeof(net_type));
        memcpy(buf + sizeof(net_type), &net_len, sizeof(net_len));
        memcpy(buf + 4, line, body_len);

        // 4. 发送
        if(write(sockfd,buf,total_len) != (ssize_t)total_len){
            perror("write");
            free(buf);
            free(line);
            break;
        }

        free(buf);


        // 5. 接收回复头
        uint16_t rsp_type, rsp_len;
        if(read_full(sockfd,&rsp_type,sizeof(rsp_type)) != sizeof(rsp_type) ||
            read_full(sockfd,&rsp_len,sizeof(rsp_len)) != sizeof(rsp_len)){
            fprintf(stderr, "connection closed or read error\n");
            free(line);
            break;
        }

        rsp_type = ntohs(rsp_type);
        rsp_len = ntohs(rsp_len);

        // 6. 接收回复 body
        uint8_t *body = malloc(rsp_len + 1);
        if(!body){
            perror("malloc body");
            free(line);
            break;
        }

        if(read_full(sockfd,body,rsp_len) != rsp_len){
            fprintf(stderr, "failed to read full body\n");
            free(body);
            free(line);
            break;
        }

        body[rsp_len] = '\0';

        // 7. 显示
        printf("<< type=%u, len=%u, data=\"%s\"\n",
               rsp_type, rsp_len, (char *)body);

        free(line);
        free(body);
    }

    printf("Closing connection\n");
    close(sockfd);

    return 0;
}

