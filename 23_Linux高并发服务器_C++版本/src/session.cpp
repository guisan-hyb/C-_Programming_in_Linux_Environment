#include "session.h"
#include "io_thread.h"
#include "defer.h"

HeadBuf::HeadBuf(size_t head_len) : _head_len(head_len), _offset(0) 
{
    _buf = (char *)malloc(sizeof(char) * head_len);
    if(!_buf){
        perror("malloc head buf failed");
        exit(1);
    }
}

HeadBuf::~HeadBuf(){
    if(_buf){
        free(_buf);
        _buf = nullptr;
    }
}


DataBuf::DataBuf(uint16_t type, size_t data_len) : _type(type), _data_len(data_len), _offset(0)
{
    _buf = (char *)malloc(sizeof(char) * data_len);
    if(!_buf){
        perror("malloc data buf failed");
        exit(1);
    }
}

DataBuf::DataBuf(uint16_t type, std::string data, size_t data_len) : _type(type), _data_len(data_len + HEAD_LEN), _offset(0)
{
    _buf = static_cast<char *>(std::malloc(sizeof(char) * _data_len));
    if(!_buf){
        perror("malloc data buf failed");
        exit(1);
    }

    uint16_t net_type = htons(type);
    uint16_t net_len = htons(data_len);
    memcpy(_buf, &net_type, 2);
    memcpy(_buf + 2, &net_len, 2);
    memcpy(_buf + 4, data.data(), data_len);
}

DataBuf::~DataBuf(){
    if(_buf){
        free(_buf);
        _buf = nullptr;
    }
}




Session::Session(int fd, IOThread *pthread) : _fd(fd), _p_ownerthread(pthread)
{
    _head_buf = std::make_shared<HeadBuf>(HEAD_LEN);
    _data_buf = nullptr;
    _recv_stage = NO_RECV;
    _send_stage = NO_SEND;
}

Session::~Session(){

}

void Session::Send(std::string data, int msg_type){
    _p_ownerthread->enqueue_send_data(_fd, data, msg_type);
}

void Session::enqueue_data(std::shared_ptr<DataBuf> data){
    _send_que.push(data);
}

int Session::send_data(std::shared_ptr<DataBuf> data){
    _send_stage = SENDING;
    //RAII defer析构执行函数
    Defer defer([this](){
        this->_send_stage = NO_SEND;
    });

    _send_que.push(data);
    while(!_send_que.empty()){
        auto msg = _send_que.front();
        while(msg->_offset < msg->_data_len){
            auto w = write(_fd, msg->_buf + msg->_offset, msg->_data_len - msg->_offset);
            if(w > 0){
                msg->_offset += w;
                continue;
            }
            
            if(w < 0){
                //写非阻塞，未写全说明socket缓冲区满，则返回1
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    return IO_EAGAIN;
                }

                //系统中断则尝试重写
                if(errno == EINTR){
                    continue;
                }

                // 其他error情况，返回-1
                perror("send failed: ");
                return IO_ERROR;
            }

             //对端关闭，也按-1返回
            if(w == 0){
                std::cout << "send peer closed, fd is " << _fd << std::endl;
                return IO_ERROR;
            }
        }

        // 发送完全
        if(msg->_offset == msg->_data_len){
            _send_que.pop();
            continue;
        }
    }

    //发送完全返回0
    return IO_SUCCESS;
}