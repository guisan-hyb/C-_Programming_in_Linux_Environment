#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345
#define TEST_ROUNDS 10         // 测试轮数
#define PAYLOAD_FMT "Ping #%d" // 消息格式

// 从 fd 上完整地读 count 字节到 buf，直到读满或出错
static ssize_t read_full(int fd, void *buf, size_t count)
{
    size_t off = 0;
    ssize_t n;
    while (off < count)
    {
        n = read(fd, (char *)buf + off, count - off);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
        {
            // 对端关闭
            return off;
        }
        off += n;
    }
    return off;
}

int main()
{
    int sock;
    struct sockaddr_in srv_addr;

    // 1. 创建 socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. 连接服务器
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &srv_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sock);
        exit(EXIT_FAILURE);
    }
    if (connect(sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
    {
        perror("connect");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Connected to %s:%d\n", SERVER_IP, SERVER_PORT);

    // 3. 循环 TEST_ROUNDS 轮自动发送和接收
    for (int round = 1; round <= TEST_ROUNDS; ++round)
    {
        // 构造 payload
        char payload[128];
        int body_len = snprintf(payload, sizeof(payload), PAYLOAD_FMT, round);
        if (body_len < 0 || body_len >= (int)sizeof(payload))
        {
            fprintf(stderr, "payload snprintf error\n");
            break;
        }

        // 消息头：2 字节 type + 2 字节 length（网络序）
        uint16_t msg_type = htons(1); // 示例类型 1
        uint16_t net_len = htons(body_len);
        size_t total = sizeof(msg_type) + sizeof(net_len) + body_len;

        // 分配缓冲区并拷贝
        uint8_t *buf = malloc(total);
        if (!buf)
        {
            perror("malloc");
            break;
        }
        memcpy(buf, &msg_type, sizeof(msg_type));
        memcpy(buf + sizeof(msg_type), &net_len, sizeof(net_len));
        memcpy(buf + sizeof(msg_type) + sizeof(net_len), payload, body_len);

        // 发送
        if (write(sock, buf, total) != (ssize_t)total)
        {
            perror("write");
            free(buf);
            break;
        }
        free(buf);
        printf("[Sent ] round=%2d, type=%u, len=%d, data=\"%s\"\n",
               round, ntohs(msg_type), body_len, payload);

        // 接收回复头
        uint16_t resp_type_n, resp_len_n;
        if (read_full(sock, &resp_type_n, sizeof(resp_type_n)) != sizeof(resp_type_n) ||
            read_full(sock, &resp_len_n, sizeof(resp_len_n)) != sizeof(resp_len_n))
        {
            fprintf(stderr, "[Error] connection closed or header read error\n");
            break;
        }
        uint16_t resp_type = ntohs(resp_type_n);
        uint16_t resp_len = ntohs(resp_len_n);

        // 接收回复体
        uint8_t *resp_body = malloc(resp_len + 1);
        if (!resp_body)
        {
            perror("malloc");
            break;
        }
        if (read_full(sock, resp_body, resp_len) != resp_len)
        {
            fprintf(stderr, "[Error] body read failed\n");
            free(resp_body);
            break;
        }
        resp_body[resp_len] = '\0';

        printf("[Recv ] round=%2d, type=%u, len=%u, data=\"%s\"\n",
               round, resp_type, resp_len, (char *)resp_body);
        free(resp_body);

        // 可选：每轮之间短暂延时，避免过快
        usleep(100000); // 100ms
    }

    printf("Closing connection.\n");
    close(sock);
    return 0;
}

