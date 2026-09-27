#include "event_loop.h"
#include <unordered_set>

EventLoop::EventLoop(int thread_num)
    : _next_idx(0), _thread_num(thread_num)
{
    std::cout << "construt event_loop num is " << thread_num << std::endl;
    _work_threads.reserve(thread_num);
    for (int i = 0; i < thread_num;i++){
        auto t = std::make_unique<IOThread>(i);
        t->start();
        _work_threads.emplace_back(std::move(t));
    }
}

EventLoop::~EventLoop(){
    for (int i = 0; i < _thread_num;i++){
        _work_threads[i]->join();
    }

    std::cout <<  "EventLoop exit" << std::endl;
}

void EventLoop::StopIOThread(){
    for (int i = 0; i < _thread_num; ++i)
    {
        _work_threads[i]->stop();
    }
}

void EventLoop::NotifyNewConns(std::vector<int> &conns){
    std::unordered_set<int> notify_threads;
    for (int i = 0; i < conns.size();i++){
        auto fd = conns[i];
        auto index = fd % _work_threads.size();
        // 批量唤醒
        _work_threads[index]->catch_new_conn(fd); 
        notify_threads.insert(index);
    }

    for(auto idx:notify_threads){
        _work_threads[idx]->wakeup();
    }
}

