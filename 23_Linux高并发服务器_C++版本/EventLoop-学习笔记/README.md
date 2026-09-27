# EventLoop 学习笔记 · 索引

> 学什么：一个 Linux 下用 `epoll(ET)` + `eventfd` + 多线程实现的 **Reactor 网络服务器**。
> 怎么学：**第 1~6 章讲透原理 → 第 7 章把原版跑通做验收 → 第 8 章按阶段自己从零复刻**。
> 跑在哪：你自己的 Linux 虚拟机（这份代码用了 `epoll/eventfd/arpa/inet.h`，**Windows 上编不过**，MinGW 也不行）。

代码位置：[../EventLoop](../EventLoop)（原样复制到虚拟机里编译即可，本身不依赖任何第三方库）。

---

## 0. 一分钟看懂它是什么

一句话：**主线程只负责 accept，收到的新连接按 fd 取模分给 N 个 IO 线程；每个 IO 线程跑一个自己的 epoll 循环，处理读写、拆包、回显。**

```
                        ┌──────────────────────────────────────────┐
                        │            主线程 (Server::run)           │
                        │  epoll: { listen_fd , event_fd }          │
  客户端 connect ──────► │  accept() 循环到 EAGAIN (ET)              │
                        │  新连接 fd 攒进 _con_fds                  │
                        │  NotifyNewCons(): index = fd % N          │
                        └───────────────┬──────────────────────────┘
                                        │ 入队 RegisterConn 任务 + 写 eventfd 唤醒
              ┌─────────────────────────┼─────────────────────────┐
              ▼                         ▼                         ▼
   ┌────────────────────┐   ┌────────────────────┐   ┌────────────────────┐
   │ IOThread #0        │   │ IOThread #1        │   │ IOThread #N-1      │
   │ epoll: {eventfd,   │   │                    │   │                    │
   │        各客户端 fd} │   │   …… 同上 ……       │   │   …… 同上 ……       │
   │ 收：拆包状态机      │   │                    │   │                    │
   │ 发：队列+EPOLLOUT   │   │                    │   │                    │
   └────────────────────┘   └────────────────────┘   └────────────────────┘
```

一份数据从"到网卡"到"回给客户端"的完整路径：

```
内核收包 → IO线程 epoll_wait 返回 EPOLLIN
        → read_head_data()  读 4 字节头（可能只读到一半 → HEAD_RECVING）
        → read_body_data()  读包体（可能只读到一半 → 继续等）
        → 读全一个包 → Session::Send() → 投递 SendData 任务给【自己这个线程】+ 写 eventfd
        → 下一轮 epoll_wait → eventfd 事件 → deal_enque_tasks()
        → Session::send_data() 直接 write()；写不动(EAGAIN) → 注册 EPOLLOUT 等可写
        → EPOLLOUT 事件 → handle_epollout() 续写 → 发完 → 摘掉 EPOLLOUT
```

注意最后回显那步：**echo 不走"跨线程"，而是"自己给自己投递任务"**，所以它多了一次 eventfd 往返。这是可以优化的点，第 6 章会讲。

---

## 1. 为什么这份代码值得学

它是一份**极简但结构完整**的 one-loop-per-thread Reactor，踩到了高性能服务器里最核心的几件事：

| 主题 | 在这份代码里对应 | 价值 |
| --- | --- | --- |
| 非阻塞 IO + 边沿触发(ET) | `IOThread::loop` | 面试高频，写错就是丢包/连接假死 |
| TCP 粘包/半包拆解 | `read_head_data` / `read_body_data` | 所有自定义协议都要写一遍 |
| 部分写与发送队列 | `Session::send_data` / `handle_epollout` | 不处理 EAGAIN 就是线上丢数据 |
| 跨线程唤醒 | `eventfd` + `IOTask` 队列 | Reactor 多线程化的标准做法 |
| 免锁设计 | 一个连接只属于一个线程 | 比"到处加锁"更值得学 |
| 生命周期管理 | `shared_ptr<Session>` + `Defer` | 回调中删连接的安全做法 |

它相当于 **muduo 的迷你版**：`EventLoop ≈ muduo::EventLoop`、`IOThread ≈ EventLoopThread + EventLoop`、`Server ≈ Acceptor`、`Session ≈ TcpConnection`。学懂这份，再去看 muduo 源码会顺很多。

---

## 2. 推荐路线（每个阶段都要能跑起来再往下）

| 阶段 | 读什么 | 做什么 | 产出 |
| --- | --- | --- | --- |
| 第 1 天 | [01-项目总览与线程模型.md](01-项目总览与线程模型.md)、[02-协议与数据结构.md](02-协议与数据结构.md)、[07](07-在Linux虚拟机上跑通与联调.md) | 在虚拟机里编译原版跑起来，用 `echo_client.py` 做一次回显 | 看到 `Accepted fd=..` 和回包 |
| 第 2 天 | [03-接收链路：拆包状态机.md](03-接收链路：拆包状态机.md)、[05-单线程事件循环与多线程唤醒.md](05-单线程事件循环与多线程唤醒.md) | 用"半包/粘包"两种客户端验证拆包；`top -H` 看线程 | 能画出收包状态机 |
| 第 3 天 | [04-发送链路：发送队列与部分写.md](04-发送链路：发送队列与部分写.md)、[06-生命周期、错误处理与已知缺陷.md](06-生命周期、错误处理与已知缺陷.md) | 用"只发不读"的客户端把发送缓冲打满，观察 EPOLLOUT 路径 | 能解释 EPOLLOUT 为什么要"用完就摘" |
| 第 4 天起 | [08-从零复刻：分阶段任务与自检.md](08-从零复刻：分阶段任务与自检.md) | 按阶段 0→7 自己写，每阶段跑验收用例 | 一份你自己的实现 |

---

## 3. 目录

| 文件 | 内容 |
| --- | --- |
| [01-项目总览与线程模型.md](01-项目总览与线程模型.md) | 文件职责、启动时序、线程模型、四条核心不变量 |
| [02-协议与数据结构.md](02-协议与数据结构.md) | 4 字节协议格式、`IOTask`/`HeadBuf`/`DataBuf`/`Session`、内存所有权表 |
| [03-接收链路：拆包状态机.md](03-接收链路：拆包状态机.md) | 收头/收体状态机逐行讲解、ET 下为什么必须读到 EAGAIN、粘包半包时序 |
| [04-发送链路：发送队列与部分写.md](04-发送链路：发送队列与部分写.md) | `send_data`、`handle_epollout`、EPOLLOUT 的订阅与摘除、`Defer` 保命技巧 |
| [05-单线程事件循环与多线程唤醒.md](05-单线程事件循环与多线程唤醒.md) | epoll 主循环、accept 循环、`IOTask` 队列、eventfd 原理、批量唤醒 |
| [06-生命周期、错误处理与已知缺陷.md](06-生命周期、错误处理与已知缺陷.md) | 启停时序、资源释放、15 条已知缺陷（含复现方法）、编译告警 |
| [07-在Linux虚拟机上跑通与联调.md](07-在Linux虚拟机上跑通与联调.md) | 环境准备、构建、10 项验收清单、排障工具箱、虚拟机网络注意事项 |
| [08-从零复刻：分阶段任务与自检.md](08-从零复刻：分阶段任务与自检.md) | 阶段 0~7 的任务/验收/自检问题/我会 review 的点 + 进阶改造题 |
| [echo_client.py](echo_client.py) | 联调客户端：回显、半包、粘包、空包体、大包、压测 |
| [09-附：两张图的文字内容.md](09-附：两张图的文字内容.md) | 从两张 PNG 里抠出的图上文字，逐条对应到代码行 |

---

## 4. 前置知识自测（有一条答不上，就先补那一条再读，否则会卡住）

1. `read()` 返回 0 / 返回 -1 且 `errno==EAGAIN` / 返回 -1 且 `errno==EINTR`，分别是什么意思？
2. LT 和 ET 的区别是什么？为什么 ET 必须"循环读写直到 EAGAIN"？
3. `epoll_wait` 返回的事件里，`EPOLLERR`/`EPOLLHUP` 需要你自己注册吗？
4. TCP 是字节流，"粘包"和"半包"分别指什么？为什么必须在应用层拆包？
5. `htons`/`ntohs` 是干什么的？为什么协议里的长度字段要转字节序？
6. 阻塞 IO 上 `accept` 会阻塞；那 ET 模式下的 `listen_fd` 为什么要循环 `accept`？
7. `std::shared_ptr` 的引用计数什么时候会归零？函数里 `auto sess = iter->second;` 这一行为什么重要？
8. `eventfd` 是什么？为什么不能只用一个 `std::queue` + `mutex` 就完成跨线程投递？
9. `std::atomic<bool>` 和加锁的 `bool` 有什么区别？这里为什么能用原子变量？
10. 为什么"一个连接只让一个线程处理"就能免掉 `Session` 内部所有的锁？

答案都会在第 1~6 章里出现，带着问题读效率最高。

---

## 5. 阅读顺序（对着代码读）

```
main.cpp          入口：读配置 → 建 Server → 注册信号 → run()
  └─ configmgr.cpp / configmgr.h    ini 解析 + 单例
  └─ server.cpp / server.h          主线程：listen + epoll + accept + 分发
        └─ event_loop.cpp / event_loop.h   IOThread 池的创建、分发、停止
              └─ io_thread.cpp / io_thread.h   ★ 核心：事件循环 + 拆包 + 发送 + 任务队列
                    └─ session.cpp / session.h   ★ 核心：连接状态、缓冲、协议头拼装
                          └─ global.h  协议常量与 IO 返回码
                          └─ defer.h   RAII 小工具
```

核心就读 `io_thread.cpp`（约 550 行）和 `session.cpp`（约 120 行）这两个文件，其余都是脚手架。
