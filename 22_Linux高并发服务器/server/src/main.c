#include <stdio.h>
#include "../include/hash_map.h"
#include "assert.h"
#include "../include/server.h"
#include "signal.h"
#include <pthread.h>
void test_hash()
{
    // 创建 16 桶的哈希表
    hashmap_t *map = hashmap_create(16);
    assert(map != NULL);

    // --- 字符串键测试 ---
    printf("=== String key tests ===\n");
    assert(hashmap_get(map, "alpha") == NULL);
    assert(hashmap_put(map, "alpha", (void *)"A") == 0);
    assert(hashmap_put(map, "beta", (void *)"B") == 0);
    assert(hashmap_put(map, "gamma", (void *)"G") == 0);

    // 重复插入同一个 key，覆盖旧值
    assert(hashmap_put(map, "beta", (void *)"BB") == 0);

    // 查询并打印
    printf("alpha -> %s\n", (char *)hashmap_get(map, "alpha")); // A
    printf("beta  -> %s\n", (char *)hashmap_get(map, "beta"));  // BB
    printf("gamma -> %s\n", (char *)hashmap_get(map, "gamma")); // G

    // 删除并确认
    assert(hashmap_remove(map, "alpha") == 0);
    assert(hashmap_get(map, "alpha") == NULL);

    // --- 整数键测试 ---
    printf("\n=== Integer key tests ===\n");
    assert(hashmap_get_int(map, 10) == NULL);
    assert(hashmap_put_int(map, 10, (void *)"ten") == 0);
    assert(hashmap_put_int(map, 20, (void *)"twenty") == 0);
    assert(hashmap_put_int(map, 30, (void *)"thirty") == 0);

    // 覆盖测试
    assert(hashmap_put_int(map, 20, (void *)"XX") == 0);

    // 查询并打印
    printf("10 -> %s\n", (char *)hashmap_get_int(map, 10)); // ten
    printf("20 -> %s\n", (char *)hashmap_get_int(map, 20)); // XX
    printf("30 -> %s\n", (char *)hashmap_get_int(map, 30)); // thirty

    // 删除并确认
    assert(hashmap_remove_int(map, 30) == 0);
    assert(hashmap_get_int(map, 30) == NULL);

    // --- 混合测试 ---
    printf("\n=== Mixed key tests ===\n");
    // 同时存在字符串 key 和 整数 key
    assert(hashmap_put(map, "100", (void *)"str100") == 0);
    assert(hashmap_put_int(map, 100, (void *)"int100") == 0);

    printf("\"100\" -> %s\n", (char *)hashmap_get(map, "100")); // str100
    printf("100   -> %s\n", (char *)hashmap_get_int(map, 100)); // int100

    // 清理
    hashmap_destroy(map);
    printf("\nAll tests passed!\n");
}

static size_t event_count = 1024;
// 全局标志，信号处理后置为 1
static volatile sig_atomic_t stop_server = 0;

// 信号处理器：收到 SIGINT/SIGTERM 时设置退出标志
void handle_signal(int signo)
{
    (void)signo;
    stop_server = 1;
}

bool expanded_once = false;

int main(void)
{

    //test_hash();
    // 1. 注册信号处理
    struct sigaction sa = {0};
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // 创建 16 桶的哈希表
    hashmap_t *session_map = hashmap_create(16);

    // 2. 创建监听 socket & epoll
    int listen_fd = create_and_bind();
    set_nonblocking(listen_fd);

    int epoll_fd = epoll_create1(0);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    // 构造事件数组
    struct epoll_event *event_addr = (struct epoll_event *)malloc(sizeof(struct epoll_event) * event_count);

    while (!stop_server)
    {
        int nfds = epoll_wait(epoll_fd, event_addr, event_count, -1);
        if (nfds < 0)
        {
            if (errno == EINTR)
                continue; // 被信号打断，重试 epoll_wait
            perror("epoll_wait");
            break;
        }

        // 如果命中上限，动态扩容一倍
        if ((size_t)nfds == event_count && !expanded_once)
        {
            size_t new_count = event_count * 2U;
            struct epoll_event *new_addr = realloc(event_addr, sizeof(*new_addr) * new_count);
            if (!new_addr)
            {
                // realloc 失败时继续用旧的数组，或根据需要退出
                perror("realloc event_addr");
            }
            else
            {
                event_addr = new_addr;
                event_count = new_count;
                printf(">>> expanded event_addr to %zu entries\n", event_count);
            }

            expanded_once = true;
        }

        for (int i = 0; i < nfds; i++)
        {
            int fd = event_addr[i].data.fd;
            uint32_t evs = event_addr[i].events;

            if (fd == listen_fd)
            {
                // todo 编写获取新连接得函数
                struct sockaddr_in cli;
                socklen_t len = sizeof(cli);
                while (1)
                {
                    int conn_fd = accept(listen_fd, (struct sockaddr *)&cli, &len);
                    if (conn_fd < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        if (errno == EINTR)
                            continue;
                        perror("accept");
                        break;
                    }

                    // 创建session
                    session_t *session = session_create(conn_fd);
                    if (!session)
                    {
                        perror("session create failed!\n");
                        break;
                    }

                    set_nonblocking(conn_fd);
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.fd = conn_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev);

                    // 将fd放入map
                    hashmap_put_int(session_map, conn_fd, session);
                    printf("Accepted fd=%d\n", conn_fd);
                }
                continue;
            }

            session_t *sess = hashmap_get_int(session_map, fd);
            if (sess == NULL)
            {
                printf("can't found fd %d\n", fd);
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                continue;
            }

            if (evs & (EPOLLERR | EPOLLHUP))
            {
                int err = 0, errlen = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, (socklen_t *)&errlen);
                fprintf(stderr, "fd=%d error: %s\n", fd, strerror(err));
                // 关闭连接
                session_destroy(sess, epoll_fd);
                // 从map中移除描述符关联的session
                hashmap_remove_int(session_map, fd);
                continue;
            }

            // 有读或者写事件
            if (evs & (EPOLLIN | EPOLLOUT))
            {

                if (evs & EPOLLIN)
                {
                    // todo 编写读取数据逻辑

                    while (1)
                    {
                        // 获取当前状态
                        if (sess->recv_stage == NO_RECV || sess->recv_stage == HEAD_RECVING)
                        {
                            // 获取剩余需要读取的数据长度
                            int remain = HEAD_LEN - sess->head_buf->offset;
                            // 读取头部
                            ssize_t read_len = read(fd, sess->head_buf->buf + sess->head_buf->offset,
                                                    remain);
                            if (read_len < 0)
                            {
                                if (errno == EAGAIN || errno == EWOULDBLOCK)
                                {
                                    // 数据已经读完
                                    break;
                                }

                                if (errno == EINTR)
                                {
                                    // 被信号打断，重试 read
                                    continue;
                                }

                                // 真正的错误
                                perror("read header");
                                goto net_error;
                            }

                            // 对端关闭
                            if (read_len == 0)
                            {
                                goto net_error;
                            }

                            // 正常读取数据
                            if (read_len < remain)
                            {
                                // 未读出完整头部
                                sess->recv_stage = HEAD_RECVING;
                                sess->head_buf->offset += read_len;
                                continue;
                            }

                            // 读完整了头部
                            // 处理头部信息
                            unsigned char *hdr = (unsigned char *)sess->head_buf->buf;

                            uint16_t t, l;
                            memcpy(&t, hdr, sizeof(t));
                            memcpy(&l, hdr + 2, sizeof(l));
                            uint16_t msg_type = ntohs(t);
                            uint16_t body_len = ntohs(l);

                            // 校验对方发送长度字段是否合理
                            if (body_len > BUFF_SIZE)
                            {
                                printf("msg body too big, throw!\n");
                                session_destroy(sess, epoll_fd);
                                hashmap_remove_int(session_map, fd);
                                break;
                            }

                            // 数据接收正常
                            //                            printf("receiv head id is %d", msg_type);
                            //                            printf("receiv head data len is %d", body_len);
                            sess->recv_stage = BODY_RECVING;
                            sess->head_buf->offset += read_len;
                            sess->data_buf = malloc_data_buf(msg_type, body_len);
                            continue;
                        }

                        if (sess->recv_stage == BODY_RECVING)
                        {
                            // 剩余要读取的长度
                            int remain = sess->data_buf->data_len - sess->data_buf->offset;

                            ssize_t read_len = read(fd,
                                                    sess->data_buf->buf + sess->data_buf->offset,
                                                    remain);

                            if (read_len < 0)
                            {
                                if (errno == EAGAIN || errno == EWOULDBLOCK)
                                {
                                    // 数据已经读完
                                    break;
                                }

                                if (errno == EINTR)
                                {
                                    // 被信号打断，重试 read
                                    continue;
                                }

                                // 真正的错误
                                perror("read header");
                                session_destroy(sess, epoll_fd);
                                hashmap_remove_int(session_map, fd);
                                break;
                            }

                            // 对端关闭
                            if (read_len == 0)
                            {
                                session_destroy(sess, epoll_fd);
                                hashmap_remove_int(session_map, fd);
                                break;
                            }

                            sess->data_buf->offset += read_len;
                            // 包体没读完
                            if (read_len < remain)
                            {
                                continue;
                            }

                            // todo... 处理消息
                            async_send(sess, epoll_fd, evs, sess->data_buf->type,
                                       sess->data_buf->data_len, sess->data_buf->buf);
                            // 回收消息
                            dealloc_data_buf(sess->data_buf);
                            sess->data_buf = NULL;
                            sess->recv_stage = NO_RECV;
                            memset(sess->head_buf->buf, 0, HEAD_LEN);
                            sess->head_buf->offset = 0;
                            // 判断读取
                            continue;
                        }
                    }
                }

                if (evs & EPOLLOUT)
                {
                    // todo 编写写数据逻辑
                    handle_epollout(epoll_fd, fd, sess->send_que);
                }
                continue;
            }

        net_error:
            // 4) 其它
            fprintf(stderr, "fd=%d unexpected event: 0x%x\n", fd, evs);
            session_destroy(sess, epoll_fd);
            hashmap_remove_int(session_map, fd);
        }
    }

    // 遍历回收内存
    for (size_t i = 0; i < session_map->bucket_count; i++)
    {
        hashmap_entry_t *entry = session_map->buckets[i];
        while (entry)
        {
            // 取出会话并销毁
            session_t *sess = (session_t *)entry->value;
            session_destroy(sess, epoll_fd);
            entry = entry->next;
        }
    }

    free(event_addr);
    hashmap_destroy(session_map);
    return 0;
}
