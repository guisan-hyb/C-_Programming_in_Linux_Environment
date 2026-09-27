#pragma once

#include "io_thread.h"
#include <atomic>
#include <vector>

class EventLoop{
public:
    EventLoop(int thread_num);
    ~EventLoop();
    void NotifyNewConns(std::vector<int> &conns);
    void StopIOThread();

private:
    std::vector<std::unique_ptr<IOThread>> _work_threads;
    std::atomic<size_t> _next_idx;
    int _thread_num;
};