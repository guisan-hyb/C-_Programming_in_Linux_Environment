#ifndef CSPROJECT_SERVER_H
#define CSPROJECT_SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "hash_map.h"
#include <stdbool.h>

#define SERVER_PORT 8080
#define BUFF_SIZE 2048 // 缓冲区大小
#define HEAD_LEN 4 // 头部长度4字节
#define HEAD_ID_LEN 2 // 头部id长度2字节
#define HEAD_DATA_LEN 2 // 头部长度字段2字节

// 接收状态
enum RecvStage{
    NO_RECV = 0, // 未接受
    HEAD_RECVING,
    BODY_RECVING
};

// 头部buffer
typedef struct 
{
    char *buf;// 缓存接受的头部信息
    size_t data_len;// 头部总长度
    size_t offset;// 头部已经接受的偏移量
} head_buf_t;

// 数据buffer,用来存储接受或者发送的数据
typedef struct 
{
    uint16_t type;// 数据类型id, 比如1001表示登录，1002表示聊天等
    char *buf;// 接受或发送缓存
    size_t data_len;// 数据总长度
    size_t offset;// 已接受或发送的偏移量
} data_buf_t;



// 发送数据节点
typedef struct send_node_t
{
    data_buf_t *send_buf;// 数据域
    struct send_node_t *next;// 下一个节点指针
} send_node_t;

// 发送队列
typedef struct 
{
    send_node_t *head;// 发送队列头部
    send_node_t *tail;// 发送队列尾部
    pthread_mutex_t send_que_lock;// 互斥锁，保证多线程访问安全
} send_que_t;

// 会话节点表示一个连接
typedef struct 
{
    int fd;// 文件描述符
    head_buf_t *head_buf;// 存储未接受完整的头部
    data_buf_t *data_buf;// 存储接受的数据包体内容
    send_que_t *send_que;// 发送队列
    enum RecvStage recv_stage; // 接受阶段,0未开始后接受，1接受头部，2接受包体
} session_t;

extern int set_nonblocking(int fd);

extern int create_and_bind();

extern void session_destroy(session_t *conn, int epoll_fd);

extern session_t *session_create(int fd);

extern data_buf_t *malloc_data_buf(int type_id, int data_len);

extern void dealloc_data_buf(data_buf_t *data_buf);

extern void dealloc_session(session_t *session);

extern ssize_t async_send(session_t *sess, int epoll_fd, uint32_t evs, size_t msg_type, size_t msg_len, char *buf);

extern void handle_epollout(int epoll_fd, int fd, send_que_t *q);

#endif