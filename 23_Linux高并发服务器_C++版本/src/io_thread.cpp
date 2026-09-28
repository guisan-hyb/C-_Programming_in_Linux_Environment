#include "io_thread.h"
#include "session.h"
#include "defer.h"

IOThread::IOThread(int index)
    : _event_count(1024), _stop(true), _idx(index), _expanded_once(false)
{
    _event_fd = eventfd(0, EFD_NONBLOCK);// 非阻塞，用于唤醒
    if(_event_fd == -1){
        perror("eventfd");
        std::exit(1);
    }

    _epoll_fd = epoll_create1(0);
    if(_epoll_fd == -1){
        perror("epoll_create1");
        std::exit(1);
    }

    // 把 _event_fd 加入 epoll，监听 EPOLLIN：
    // _event_fd 是 IO 线程的“门铃”，加入 epoll 监听 EPOLLIN，是为了让其他线程能按门铃把 IO 线程从 epoll_wait 中叫醒，及时处理跨线程任务
    auto add_res = add_fd(_event_fd, EPOLLIN);
    if(!add_res){
        perror("epoll add eventfd failed");
        std::exit(1);
    }

    // 分配 epoll_event 数组：
    _event_addr = (struct epoll_event *)malloc(sizeof(struct epoll_event) * _event_count);
    if(!_event_addr){
        perror("malloc events falied");
        std::exit(1);
    }
}

IOThread::~IOThread(){
    std::cout << "IOThread 【" << _idx << "】exit" << std::endl;
    close(_event_fd);
    close(_epoll_fd);
    free(_event_addr);
    join();
}



// epoll 与 fd 管理
bool IOThread::add_fd(int fd, int events){
    struct epoll_event ev2;
    ev2.events = events;
    ev2.data.fd = fd;

    if(epoll_ctl(_epoll_fd,EPOLL_CTL_ADD,fd,&ev2) == 0){
        return true;
    }

    perror("fd add to epoll failed, try to modify");
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev2) == -1)
    {
        perror("fd modify failed: ");
        return false;
    }

    return true;
}

bool IOThread::mod_fd(int fd, int events){
    struct epoll_event ev2;
    ev2.events = events;
    ev2.data.fd = fd;

    if(epoll_ctl(_epoll_fd,EPOLL_CTL_MOD,fd,&ev2) == -1){
        perror("fd modify failed: ");
        return false;
    }

    return true;
}

bool IOThread::del_fd(int fd){
    if(epoll_ctl(_epoll_fd,EPOLL_CTL_DEL,fd,NULL) == -1){
        perror("fd del failed: ");
        return false;
    }
    return true;
}

void IOThread::clear_fd(int fd){
    _sessions.erase(fd);
    del_fd(fd);
    close(fd);
}

int IOThread::set_nonblocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}



// 线程控制
void IOThread::start(){
    _stop = false;
    _thread = std::thread([this](){
        this->loop();
    });
}

void IOThread::wakeup(){ // 向 eventfd 写入 8 字节，使 epoll_wait 返回。未检查返回值。
    uint64_t one = 1;
    auto n = write(_event_fd, &one, sizeof(one));
}

void IOThread::stop(){
    _stop = true;
    auto task = std::make_shared<IOTask>(_event_fd, TaskType::ShutDown);
    enqueue_task(task);
}

void IOThread::join(){
    if(_thread.joinable()){
        _thread.join();
    }

    std::cout << "IOThread join exit" << std::endl;
}



// 任务投递
void IOThread::enqueue_task(std::shared_ptr<IOTask> task){ // 加锁入队，然后唤醒 IO 线程
    {
        std::lock_guard<std::mutex> lk(_task_mtx);
        _tasks.push(task);
    }
    wakeup();
}

void IOThread::enqueue_new_conn(int fd){ // 投递“注册新连接”任务，会唤醒线程
    auto task = std::make_shared<IOTask>(fd, TaskType::RegisterConn);
    enqueue_task(task);
}

void IOThread::enqueue_send_data(int fd, const std::string &msg, int msgtype){ // 投递发送数据任务
    auto task = std::make_shared<IOTask>(fd, TaskType::SendData, msg, msgtype);
    enqueue_task(task);
}

void IOThread::catch_new_conn(int fd){
    auto task = std::make_shared<IOTask>(fd, TaskType::RegisterConn);
    {
        std::lock_guard<std::mutex> lk(_task_mtx);
        _tasks.push(task);
    }
}



// 任务处理
bool IOThread::deal_enque_task(){
    std::queue<std::shared_ptr<IOTask>> q;
    {
        std::lock_guard<std::mutex> lk(_task_mtx);
        std::swap(q, _tasks);
    }

    while(!q.empty()){
        auto task = q.front();
        q.pop();

        if(task->_type == TaskType::RegisterConn){
            auto sess = std::make_shared<Session>(task->_fd, this);
            _sessions[task->_fd] = sess;
            set_nonblocking(task->_fd);
            add_fd(task->_fd, EPOLLIN | EPOLLET);
            continue;
        }

        if(task->_type == TaskType::SendData){
            auto iter = _sessions.find(task->_fd);
            if(iter == _sessions.end()) continue;

            auto data_buf = std::make_shared<DataBuf>(task->_msgtype, task->_data, task->_data.size());
            if(iter->second->_send_stage == SendStage::SENDING){
                iter->second->enqueue_data(data_buf);
                continue;
            }

            //没有数据发送，则直接发送
            auto send_res = iter->second->send_data(data_buf);
            if(send_res == -1){
                _sessions.erase(task->_fd);
                del_fd(task->_fd);
                close(task->_fd);
                continue;
            }

            //遇到EAGAIN，放入EPOLL监听
            if(send_res == 1){
                mod_fd(task->_fd, EPOLLIN | EPOLLOUT | EPOLLET);
                continue;
            }

            //发送正常则跳过后面内容
            continue;
        }

        if(task->_type == TaskType::ShutDown){
            return false;
        }
    }

    return true;
}



// 协议读取状态机
int IOThread::read_head_data(std::shared_ptr<Session> sess){
    if(sess == nullptr) return -1;

    //获取剩余需要读取的数据长度
    int remain = HEAD_LEN - sess->_head_buf->_offset;
    //读取头部
    ssize_t read_len = read(sess->_fd, sess->_head_buf->_buf + sess->_head_buf->_offset, remain);

    if(read_len < 0){ // 遇到eagain 返回1表示读空，非阻塞未读全
        if(errno == EAGAIN || errno == EWOULDBLOCK){
            return IO_EAGAIN;
        }

        if(errno == EINTR){ // 系统中断返回2, 表示继续
            return IO_CONTINUE;
        }

        // 真正错误, 返回-1
        perror("read header failed: ");
        return IO_ERROR;
    }

    // 对端关闭也按-1返回
    if (read_len == 0)
    {
        std::cout << "read peer closed , fd is " << sess->_fd << std::endl;
        return -1;
    }

    // 以下为read_len大于0的情况
    if(read_len < remain){
        // 未读出完头部
        sess->_recv_stage = HEAD_RECVING;
        sess->_head_buf->_offset += read_len;
        // 返回2表示继续
        return IO_CONTINUE;
    }

    //读出完整头部
    //处理头部信息
    uint8_t *hdr = (uint8_t *)(sess->_head_buf->_buf);
    uint16_t t, l;
    memcpy(&t, hdr, sizeof(t));
    memcpy(&l, hdr + 2, sizeof(l));
    uint16_t msg_type = ntohs(t);
    uint16_t body_len = ntohs(l);

    //校验对方发送长度字段是否合理
    if(body_len > BUFF_SIZE){
        std::cout << "msg body too big" << std::endl;
        return IO_ERROR;
    }

    // 特判body_len == 0
    if(body_len == 0){
        sess->Send("", msg_type);

        sess->_data_buf = NULL;
        sess->_recv_stage = NO_RECV;
        memset(sess->_head_buf->_buf, 0, HEAD_LEN);
        sess->_head_buf->_offset = 0;
        return IO_CONTINUE;
    }

    //数据接收正常
    sess->_recv_stage = BODY_RECVING;
    sess->_head_buf->_offset += read_len;
    sess->_data_buf = std::make_shared<DataBuf>(msg_type, body_len);
    return IO_CONTINUE;
}

int IOThread::read_body_data(std::shared_ptr<Session> sess){
    //剩余要读取的长度
    int remain = sess->_data_buf->_data_len - sess->_data_buf->_offset;

    ssize_t read_len = read(sess->_fd, sess->_data_buf->_buf + sess->_data_buf->_offset, remain);

    if(read_len < 0){
        if(errno == EAGAIN || errno == EWOULDBLOCK){
            return IO_EAGAIN;
        }

        if(errno == EINTR){
            //被信号中断，重试read
            return IO_CONTINUE;
        }

        //真正的错误
        perror("read body");

        return IO_ERROR;
    }

    // 对端关闭
    if(read_len == 0){
        return IO_ERROR;
    }

    sess->_data_buf->_offset += read_len;
    if(read_len<remain){
        return IO_CONTINUE;
    }


    //读完整了
    //这里做下测试，回发消息
    std::string str(sess->_data_buf->_buf, sess->_data_buf->_data_len);
    sess->Send(str, sess->_data_buf->_type);

    //回收消息
    sess->_data_buf = NULL;
    sess->_recv_stage = NO_RECV;
    memset(sess->_head_buf->_buf, 0, HEAD_LEN);
    sess->_head_buf->_offset = 0;

    //返回继续解析下个包头部
    return IO_CONTINUE;
}



// 发送逻辑
void IOThread::handle_epollout(std::shared_ptr<Session> sess){
    sess->_send_stage = SENDING;
    // RAII defer析构执行函数
    Defer defer([sess](){
        sess->_send_stage = NO_SEND;
    });

    while(!sess->_send_que.empty()){
        auto send_data = sess->_send_que.front();
        while(send_data->_offset < send_data->_data_len){
            auto write_len = write(sess->_fd, send_data->_buf + send_data->_offset, 
                send_data->_data_len - send_data->_offset);
            
            if(write_len > 0){
                send_data->_offset += write_len;
                continue;
            }

            if(write_len < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                     //非阻塞EAGAIN错误，直接返回
                    return;
                }

                if(errno == EINTR){
                    continue;
                }

                clear_fd(sess->_fd);
                return;
            }

            //对端关闭
            clear_fd(sess->_fd);
            return;
        }

        //发送完全，则出队继续下一个
        sess->_send_que.pop();
    }

    // 整个队列发送完成，则取消注册EPOLLOUT，改为EPOLLIN|EPOLLET
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = sess->_fd;
    epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, sess->_fd, &ev);
}


// 主事件循环：loop
void IOThread::loop(){
    while(!_stop){
        int nfds = epoll_wait(_epoll_fd, _event_addr, _event_count, -1);
        if(nfds < 0){
            if(errno == EINTR) continue; // 被信号打断，重试 epoll_wait
            perror("epoll_wait");
            break;
        }

        // 如果命中上限，动态扩容一倍
        if((size_t)nfds == _event_count && !_expanded_once){
            size_t new_count = _event_count * 2;
            struct epoll_event *new_addr = (struct epoll_event *)realloc(_event_addr,
                                                                         sizeof(struct epoll_event) * new_count);
            if(!new_count){
                // realloc 失败时继续用旧的数组，或根据需要退出
                perror("realloc event_addr");
            }else{
                _event_addr = new_addr;
                _event_count = new_count;
                std::cout << "expanded event_addr to " << _event_count << " size";
            }

            _expanded_once = true;
        }

        for (int i = 0; i < nfds;i++){
            int fd = _event_addr[i].data.fd;
            uint32_t evs = _event_addr[i].events;

            if(evs & (EPOLLERR | EPOLLHUP)){
                int err = 0, errlen = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, (socklen_t *)&errlen);
                fprintf(stderr, "fd=%d error: %s\n", fd, strerror(err));
                clear_fd(fd);
                continue;
            }

            if(fd == _event_fd){
                uint64_t cnt;
                read(fd, &cnt, sizeof(cnt));// 清空计数

                auto deal_res = deal_enque_task();
                if (!deal_res)
                {
                    std::cout << "io_thread receive exit eventfd" << std::endl;
                    return;
                }
                continue;
            }

            if(evs & (EPOLLIN | EPOLLOUT)){
                auto iter = _sessions.find(fd);
                if(iter == _sessions.end()) continue;

                auto sess = iter->second;
                if(evs & EPOLLIN){
                    while(1){
                        if(sess->_recv_stage == NO_RECV ||
                            sess->_recv_stage == HEAD_RECVING)
                        {
                            int head_res = read_head_data(sess);
                            if(head_res == IO_CONTINUE){
                                continue;
                            }
                            if(head_res == IO_ERROR){
                                clear_fd(fd);
                                break;
                            }
                            if(head_res == IO_EAGAIN){
                                //数据已经读完
                                break;
                            }

                            //成功则继续轮询
                            continue;
                        }
                        
                        if(sess->_recv_stage == BODY_RECVING){
                            int body_res = read_body_data(sess);
                            if(body_res == IO_CONTINUE){
                                continue;
                            }
                            if(body_res == IO_ERROR){
                                clear_fd(fd);
                                break;
                            }
                            if(body_res == IO_EAGAIN){
                                // 数据已经读完
                                break;
                            }

                            //成功则继续轮询
                            continue;
                        }
                    }
                }

                if(evs & EPOLLOUT){
                    if(sess->_send_stage == SENDING){
                        continue;
                    }

                    handle_epollout(sess);
                }

                continue;
            }
        }
    }
}