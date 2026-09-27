#pragma once

#include <queue>
#include <memory>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <string>
#include <unordered_map>

enum class TaskType{
    RegisterConn,
    SendData,
    ShutDown
};

class IOTask{
public:
    IOTask(int fd, TaskType type, std::string data = "", int msgtype = 0)
        : _fd(fd), _type(type), _data(data), _msgtype(msgtype) {}

    ~IOTask() = default;

    TaskType _type;
    int _fd;
    //以下字段仅发送时生效
    std::string _data;
    int _msgtype;
};


class NoneCopy{
protected:
    NoneCopy() = default;

private:
    NoneCopy(const NoneCopy &) = delete;
    NoneCopy &operator=(const NoneCopy &) = delete;
};


class Session;
class IOThread : public NoneCopy {
public:
    IOThread(int index);
    ~IOThread();

    int set_nonblocking(int fd);
    void enqueue_new_conn(int fd);
    void catch_new_conn(int fd);
    void start();
    void wakeup();
    void stop();
    void join();
    void enqueue_task(std::shared_ptr<IOTask> task);
    void enqueue_send_data(int fd, const std::string &msg, int msgtype);
    void loop();

private:
    bool deal_enque_task();
    bool add_fd(int fd, int events);
    bool mod_fd(int fd, int events);
    bool del_fd(int fd);
    void clear_fd(int fd);

    int read_head_data(std::shared_ptr<Session> sess);
    int read_body_data(std::shared_ptr<Session> sess);
    void handle_epollout(std::shared_ptr<Session> sess);

private:
    int _event_fd;// eventfd，用于唤醒 epoll_wait
    int _epoll_fd;// epoll 实例

    std::mutex _task_mtx;// 保护任务队列
    std::queue<std::shared_ptr<IOTask>> _tasks;// 跨线程任务队列

    struct epoll_event *_event_addr; // epoll_wait 返回事件数组
    int _event_count;// 事件数组容量，初始 1024

    std::thread _thread;// IO 线程
    std::atomic<bool> _stop;// 停止标志

    int _idx;// IOThread 编号
    std::unordered_map<int, std::shared_ptr<Session>> _sessions; // fd -> Session
    bool _expanded_once;// 事件数组是否已经扩容过一次
};

