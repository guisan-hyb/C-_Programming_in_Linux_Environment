# Linux 五种 IO 模型详解 —— 从阻塞到 io_uring 的演进之路

> 网络编程绕不开一个问题：一个进程要同时处理成千上万个连接，怎么把 CPU 用到极致？
>
> Linux 给出了五种 IO 模型作为答案：**阻塞 IO、非阻塞 IO、IO 多路复用、信号驱动 IO、异步 IO**。这是 POSIX 标准下的经典划分，从最早的 BSD Socket 一直到 2019 年的 io_uring，整整演进了四十年。
>
> 本文逐一拆解这五种模型的原理、内核数据结构、C/C++ 代码示例、优缺点，并在最后给出横向对比与工程选型建议。

---

## 一、回顾：一次 IO 的两个阶段

理解五种模型，必须先抓住 POSIX 对一次 IO 的拆分：

```
┌────────────────────────────────────────────────────────────┐
│                  一次 read() 的完整生命周期                  │
├────────────────────────────────────────────────────────────┤
│                                                            │
│   阶段一：数据准备（waiting for readiness）                  │
│   ────────────────────────────────                         │
│   网卡收到数据 → DMA 拷贝到内核 socket 缓冲区 → 标记"可读"   │
│   数据还在内核空间，用户进程拿不到                            │
│                                                            │
│   阶段二：数据读写（data transfer）                         │
│   ────────────────────────────────                         │
│   内核把数据从 socket 缓冲区拷贝到用户提供的 buffer 中       │
│   这一步由 CPU 拷贝，发生在用户进程上下文（同步 IO 时）      │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

**五种模型的差异，本质就是这两个阶段由谁干、怎么等**：

- 阶段一（等就绪）：阻塞 / 非阻塞 / epoll 代理 / 信号通知 / 内核自己等
- 阶段二（搬数据）：用户线程搬 = 同步 IO；内核搬 = 异步 IO

按 POSIX 严格定义，**前四种都是同步 IO**（阶段二都由用户线程搬数据），只有第五种（异步 IO）才是真异步。

---

## 二、模型一：阻塞 IO（Blocking IO，BIO）

### 1. 工作原理

调用 `read()` / `recv()` 时，如果 socket 缓冲区没有数据，**当前线程被 OS 挂起**（进入 `TASK_INTERRUPTIBLE` 状态），直到数据到达才被唤醒。

```
用户线程                内核
   │                     │
   │  read(fd, buf)       │
   ├────────────────────>│
   │                     │ 数据没就绪
   │   线程睡眠           │
   │   (挂起 TASK_INT)    │
   │                     │ 数据到达，DMA 拷贝到内核缓冲
   │                     │ 内核拷贝到用户 buf
   │   唤醒               │
   │<────────────────────┤  read 返回
   │  继续执行             │
```

### 2. C 代码示例

```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

int main() {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenfd, 5);

    while (1) {
        int connfd = accept(listenfd, NULL, NULL);  // 阻塞等待新连接
        char buf[1024];
        // 下面这行会一直卡住，直到客户端发数据或断开
        ssize_t n = read(connfd, buf, sizeof(buf));
        if (n > 0) {
            buf[n] = '\0';
            printf("recv: %s\n", buf);
        }
        close(connfd);
    }
}
```

### 3. 优缺点与适用场景

**优点**：
- 编程模型最简单，代码顺序执行，符合人类直觉。
- 调试容易，单步跟踪无并发问题。
- 在连接数少、请求快的场景下，性能开销最小。

**缺点**：
- **1 连接 = 1 线程**。1 万个慢连接 = 1 万个线程。
- 每个线程默认 8MB 栈，1 万连接吃掉 80GB 虚拟内存。
- 上下文切换成本高，CPU 时间片在切换中浪费。
- 完全无法应对 C10K 问题。

**适用场景**：
- 客户端程序、连接数极少的服务（如本地 IPC）。
- 早期 Apache prefork MPM（多进程，每个进程一个连接）。
- 学习网络编程的入门模型。

---

## 三、模型二：非阻塞 IO（Non-blocking IO，NIO）

### 1. 工作原理

把 fd 设为 `O_NONBLOCK`，调用 `read()` 时如果没数据，**OS 立刻返回 `EAGAIN`/`EWOULDBLOCK`**，线程不会被挂起。

```
用户线程                内核
   │                     │
   │  read(fd) [非阻塞]   │
   ├────────────────────>│
   │                     │ 数据没就绪
   │  -1, errno=EAGAIN   │
   │<────────────────────┤  立即返回
   │  干别的事 / 继续问    │
   │  read(fd) 又一次     │
   ├────────────────────>│
   │  -1, errno=EAGAIN   │
   │<────────────────────┤
   │  read(fd) 第 N 次    │
   ├────────────────────>│
   │                     │ 这次数据就绪了
   │                     │ 内核拷贝到用户 buf
   │  n 字节              │
   │<────────────────────┤
```

### 2. C 代码示例

```c
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    int fd = /* ... socket 或其他 fd ... */;
    set_nonblock(fd);

    char buf[1024];
    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n >= 0) {
            // 读到数据
            buf[n] = '\0';
            printf("recv: %s\n", buf);
            break;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 数据没就绪，线程不睡，做点别的
            do_some_work_or_sleep_a_bit();
            continue;
        } else {
            perror("read");
            break;
        }
    }
}
```

### 3. 优缺点与适用场景

**优点**：
- 线程不被挂起，CPU 资源不浪费在睡眠上。
- 单线程可以"照顾"多个 fd（轮询）。

**缺点**：
- **纯非阻塞 + 轮询是反模式**：CPU 在 `while(true)` 里疯狂空转，吃满一个核。
- 即使 sleep 一下再问，响应延迟也难以控制。
- 每次系统调用都要陷入内核态，开销大。

**适用场景**：
- **几乎不单独使用**，必须配合 IO 多路复用才有意义。
- 偶尔用在「等就绪的同时还要做点别的事，且不希望线程睡眠」的场景。
- 是 IO 多路复用 + Reactor 模式的底层基础。

---

## 四、模型三：IO 多路复用（IO Multiplexing）

这是 Linux 网络编程的**绝对主力**，几乎所有高性能网络框架（Nginx、Redis、Netty、muduo）的底层都是它。

IO 多路复用经历了三代演进：`select` → `poll` → `epoll`。

### 1. select（1983，BSD）

#### 1.1 API

```c
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);
```

#### 1.2 内核数据结构

`fd_set` 是一个**位图（bitmap）**，每个 bit 对应一个 fd：

```
fd_set (默认 1024 bit，由 FD_SETSIZE 决定)
┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐
│fd0│fd1│fd2│fd3│fd4│..│..│..│..│..│fd1023│  0 = 不关心, 1 = 关心可读
└──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘
```

每次调用 `select`：
1. 用户把 `fd_set` 从用户态**拷贝**到内核态。
2. 内核遍历这 1024 个 bit，**逐个检查**每个 fd 是否就绪。
3. 内核把就绪的 fd 保留 bit=1，未就绪的清 0，再**拷回**用户态。
4. 用户态**再次遍历**这 1024 个 bit，找出哪些 fd 是 1，逐个处理。

#### 1.3 代码示例

```c
#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    int fds[3] = {fd1, fd2, fd3};
    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int maxfd = 0;
        for (int i = 0; i < 3; i++) {
            FD_SET(fds[i], &readfds);
            if (fds[i] > maxfd) maxfd = fds[i];
        }
        struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
        int n = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (n < 0) { perror("select"); break; }
        if (n == 0) { printf("timeout\n"); continue; }
        // 必须遍历全部 fd 来找就绪的
        for (int i = 0; i < 3; i++) {
            if (FD_ISSET(fds[i], &readfds)) {
                char buf[1024];
                read(fds[i], buf, sizeof(buf));  // 数据搬运仍是用户做
            }
        }
    }
}
```

#### 1.4 缺点

- **fd 数量限制**：`FD_SETSIZE = 1024`，编译期写死。
- **每次都要拷贝整个 fd_set**：1 万 fd 时单次拷贝几 KB。
- **O(n) 遍历**：内核和用户态都要遍历所有 fd，即使只有 1 个就绪。
- **fd_set 不可重用**：每次调用前都要重新 FD_SET。

### 2. poll（1986，System V）

#### 2.1 API

```c
struct pollfd {
    int fd;           // 关心的 fd
    short events;     // 关心的事件（POLLIN/POLLOUT 等）
    short revents;    // 内核填的就绪事件
};
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

#### 2.2 与 select 的差异

poll 用**动态数组**（`struct pollfd[]`）代替位图，三个核心改进：
- **没有 fd 数量上限**（受限于进程 ulimit）。
- **事件分离**：`events` 和 `revents` 分开，避免 select 的"fd_set 被改坏"问题。
- **更清晰的事件类型**：支持 POLLIN、POLLOUT、POLLERR、POLLHUP 等。

但**核心问题没解决**：内核和用户态仍需 **O(n) 遍历**全部 fd 查找就绪。

#### 2.3 代码示例

```c
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    struct pollfd fds[3] = {
        { .fd = fd1, .events = POLLIN },
        { .fd = fd2, .events = POLLIN },
        { .fd = fd3, .events = POLLIN },
    };
    while (1) {
        int n = poll(fds, 3, 5000);  // 5 秒超时
        if (n < 0) { perror("poll"); break; }
        if (n == 0) { printf("timeout\n"); continue; }
        // 仍要遍历所有 fd 检查 revents
        for (int i = 0; i < 3; i++) {
            if (fds[i].revents & POLLIN) {
                char buf[1024];
                read(fds[i].fd, buf, sizeof(buf));
            }
        }
    }
}
```

### 3. epoll（2002，Linux 2.5.44）—— Linux 的杀手锏

epoll 是 Linux 专有的多路复用机制，通过**两个核心数据结构**彻底解决了 select/poll 的 O(n) 问题。

#### 3.1 API

```c
int epoll_create(int size);              // 创建 epoll 实例
int epoll_ctl(int epfd, int op, int fd,
              struct epoll_event *event); // 增删改关心的 fd
int epoll_wait(int epfd, struct epoll_event *events,
               int maxevents, int timeout);  // 等待就绪事件
```

#### 3.2 内核数据结构（关键！）

epoll 内部维护两个数据结构：

```
eventpoll 结构（内核态）
┌──────────────────────────────────────────────────────────┐
│                                                          │
│   1. 红黑树 rbr (RB-Tree)                                │
│   ────────────────────────                              │
│   存所有被 epoll_ctl 注册的 fd                           │
│   每个节点是 epitem，包含 fd + 关心的事件                │
│   插入/删除/查找都是 O(log n)                            │
│                                                          │
│        ┌─────┐                                            │
│        │ ep1 │ fd=3, EPOLLIN                              │
│        └─────┘                                            │
│       /        \                                          │
│   ┌─────┐    ┌─────┐                                     │
│   │ ep2 │    │ ep3 │ fd=5, EPOLLIN|EPOLLOUT              │
│   └─────┘    └─────┘                                     │
│   fd=4        fd=6                                       │
│                                                          │
│   2. 就绪链表 rdllist (Ready List, 双向链表)              │
│   ────────────────────────                              │
│   当某个 fd 数据到达，内核通过回调把它对应的 epitem        │
│   挂到 rdllist 上 —— 这是 epoll 的精髓！                 │
│                                                          │
│   rdllist: ep3 <-> ep1 <-> NULL                          │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

**epoll 高效的核心机制**：

1. **注册时**：`epoll_ctl` 把 fd 插入红黑树，O(log n)，**只做一次**，后续不需要每次重新传 fd。
2. **数据到达时**：网卡触发硬件中断 → 内核驱动 → 调用 fd 注册时的**回调函数** `ep_poll_callback` → 把该 fd 对应的 epitem 加到 rdllist。
3. **用户调用 `epoll_wait` 时**：内核直接把 rdllist 上的 epitem 拷贝到用户态的 `events` 数组，**复杂度 O(就绪 fd 数)**，而不是 O(总 fd 数)。

#### 3.3 与 select/poll 的对比

| 维度 | select | poll | epoll |
|------|--------|------|-------|
| fd 集合存储 | 位图 | 动态数组 | 红黑树 + 就绪链表 |
| fd 上限 | 1024 | 进程 ulimit | 进程 ulimit |
| 注册 fd | 每次传 | 每次传 | 注册一次 |
| 内核遍历 | O(n) 全部 | O(n) 全部 | O(就绪数) |
| 用户态遍历 | O(n) 全部 | O(n) 全部 | O(就绪数) |
| fd_set 复用 | 不可复用 | events/revents 分离 | 不可复用 |
| 跨平台 | POSIX 通用 | POSIX 通用 | Linux 专有 |

#### 3.4 LT 模式 vs ET 模式

epoll 有两种触发模式：

**LT (Level Triggered，水平触发，默认)**：
- 只要 fd 缓冲区还有数据可读，每次 `epoll_wait` 都会返回该 fd。
- 编程简单，但同一数据可能被多次通知。
- 适合读不及时的程序（比如缓冲区大于 `read` 一次读的量）。

**ET (Edge Triggered，边缘触发)**：
- fd 从"无可读"变为"有可读"时，`epoll_wait` **只通知一次**。
- 程序必须**一次性读完**所有数据（循环 `read` 直到 `EAGAIN`），否则下次不会通知。
- 编程复杂，但效率更高，减少 epoll_wait 被唤醒的次数。
- **fd 必须设为非阻塞**，否则 read 会阻塞死循环。
- Nginx 用的就是 ET 模式。

#### 3.5 完整代码示例（ET 模式）

```cpp
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void set_nonblock(int fd) {
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

// ET 模式必须循环读到 EAGAIN
void handle_read(int epfd, int fd) {
    char buf[1024];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            buf[n] = '\0';
            printf("fd=%d recv: %s\n", fd, buf);
        } else if (n == 0) {
            // 对端关闭
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 数据读完了，ET 模式正确退出
                break;
            }
            perror("read");
            close(fd);
            break;
        }
    }
}

int main() {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblock(listenfd);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenfd, 128);

    int epfd = epoll_create1(0);

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // ET 模式
    ev.data.fd = listenfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);

    struct epoll_event events[1024];
    printf("epoll server on :8080\n");
    while (true) {
        int n = epoll_wait(epfd, events, 1024, -1);
        if (n < 0) { perror("epoll_wait"); break; }
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if (fd == listenfd) {
                // ET 模式下 listenfd 也要循环 accept 到 EAGAIN
                while (true) {
                    struct sockaddr_in caddr;
                    socklen_t clen = sizeof(caddr);
                    int cfd = accept(listenfd, (struct sockaddr*)&caddr, &clen);
                    if (cfd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }
                    set_nonblock(cfd);
                    struct epoll_event cev;
                    cev.events = EPOLLIN | EPOLLET;
                    cev.data.fd = cfd;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &cev);
                }
            } else {
                if (events[i].events & EPOLLIN) {
                    handle_read(epfd, fd);
                }
            }
        }
    }
    close(listenfd);
    close(epfd);
    return 0;
}
```

#### 3.6 epoll 的优缺点

**优点**：
- **O(1) 通知就绪 fd**，性能与总 fd 数无关，只与活跃 fd 数有关。
- 1 个线程可处理 10 万+ 并发连接（C10K 轻松解决）。
- 红黑树增删改 fd 都是 O(log n)，效率高。
- 支持 ET 和 LT 两种模式。

**缺点**：
- **本质仍是同步 IO**：数据从内核到用户缓冲区仍由用户线程搬。
- Linux 专有，跨平台不便（BSD/macOS 用 kqueue，Windows 用 IOCP）。
- ET 模式编程复杂，容易漏读数据。

---

## 五、模型四：信号驱动 IO（Signal-driven IO）

### 1. 工作原理

把 fd 设为 `O_ASYNC`（或 `F_SETSIG`），当 fd 就绪时，内核向进程发送 `SIGIO` 信号。进程在信号处理函数中调用 `read()` 取数据。

```
用户线程                内核
   │                     │
   │  fcntl(O_ASYNC)     │
   ├────────────────────>│  注册信号通知
   │  干别的事            │
   │                     │ 数据到达
   │<═══════════════════┤  内核发 SIGIO 信号
   │  信号处理函数:        │
   │   read(fd) 拷数据    │
   ├────────────────────>│
   │   n 字节             │
   │<────────────────────┤
```

### 2. C 代码示例

```c
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

static int g_fd;

void sigio_handler(int signo) {
    char buf[1024];
    ssize_t n = read(g_fd, buf, sizeof(buf));
    if (n > 0) {
        buf[n] = '\0';
        printf("recv via signal: %s\n", buf);
    }
}

int main() {
    g_fd = /* ... socket fd ... */;

    // 1. 设置 fd 属主（接收信号的进程）
    fcntl(g_fd, F_SETOWN, getpid());
    // 2. 启用异步通知
    int flags = fcntl(g_fd, F_GETFL);
    fcntl(g_fd, F_SETFL, flags | O_ASYNC);
    // 可选：用 F_SETSIG 指定实时信号，可携带 fd 信息
    // fcntl(g_fd, F_SETSIG, SIGRTMIN + 1);

    // 3. 注册信号处理
    struct sigaction sa;
    sa.sa_handler = sigio_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGIO, &sa, NULL);

    while (1) {
        pause();  // 等待信号
    }
}
```

### 3. 优缺点与适用场景

**优点**：
- 不需要像 select/poll 那样主动轮询。
- 信号到达前线程可以做别的事。

**缺点**：
- **信号处理是异步的，但 `read()` 仍然是同步的**，本质还是同步 IO。
- 信号处理函数中能做的事有限（不能调用非可重入函数）。
- **信号排队问题**：多个 fd 同时就绪时，信号可能合并，丢失就绪信息。
- 用 `F_SETSIG` + 实时信号可以排队，但编程复杂。
- 信号开销大（上下文切换、信号栈切换）。

**适用场景**：
- **网络 IO 几乎没人用**。
- 主要用于 **UDP socket**（UDP 包边界清晰，一个信号对应一个包）。
- 偶尔用于某些特殊场景：硬件中断模拟、串口通信。

---

## 六、模型五：异步 IO（Asynchronous IO）

终于到 POSIX 定义的**真异步 IO**。用户告诉内核："我要读这些数据，读到这个 buffer，读完了通知我"，然后立刻返回。**内核负责等就绪 + 拷贝数据**，用户收到通知时数据已就位。

Linux 上有三个异步 IO 实现，逐一拆解。

### 1. POSIX AIO（glibc 实现，伪异步）

glibc 在用户态用**线程池 + 同步 IO** 模拟异步：

```c
#include <aio.h>
#include <string.h>
#include <stdio.h>

int main() {
    int fd = open("test.txt", O_RDONLY);
    char buf[1024];
    struct aiocb cb = {0};
    cb.aio_fildes = fd;
    cb.aio_buf = buf;
    cb.aio_nbytes = sizeof(buf);
    cb.aio_offset = 0;

    aio_read(&cb);   // 立刻返回，glibc 内部用线程池同步读
    while (aio_error(&cb) == EINPROGRESS) {
        // 干别的事
    }
    ssize_t n = aio_return(&cb);
    printf("read %zd bytes\n", n);
    close(fd);
}
```

**问题**：底层是线程池同步 IO，**根本不是真异步**，性能差，基本没人用于网络 IO。

### 2. Linux AIO（libaio / `io_submit`）

内核态实现，但**对网络 fd 不支持**，只能用于文件 IO，且必须 `O_DIRECT`：

```c
#include <libaio.h>
#include <fcntl.h>
#include <stdio.h>

int main() {
    io_context_t ctx = 0;
    io_setup(64, &ctx);

    int fd = open("test.txt", O_RDONLY | O_DIRECT);
    char* buf; posix_memalign((void**)&buf, 512, 1024);

    struct iocb iocb;
    struct iocb* iocbs[1] = { &iocb };
    io_prep_pread(&iocb, fd, buf, 1024, 0);

    io_submit(ctx, 1, iocbs);

    struct io_event events[1];
    io_getevents(ctx, 1, 1, events, NULL);
    printf("read %llu bytes\n", events[0].res);
    close(fd);
    io_destroy(ctx);
}
```

**问题**：
- 仅支持文件 IO，**网络 socket 不支持**。
- 必须用 `O_DIRECT`，对齐要求严格。
- 部分操作仍可能阻塞。

### 3. io_uring（2019，Linux 5.1+）—— Linux 真正的异步 IO

io_uring 是 Jens Axboe 设计的**统一异步 IO 接口**，支持文件和网络，性能碾压前两者。

#### 3.1 核心设计：共享内存环形队列

io_uring 用两个环形队列在用户态和内核态之间通信，**完全无系统调用提交 IO**（SQPOLL 模式下）：

```
┌─────────────────────────────────────────────────────────────┐
│                    io_uring 内存布局                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  用户态                         内核态                       │
│  ┌────────────────┐            ┌────────────────┐          │
│  │                │            │                │          │
│  │  SQ (提交队列)  │ <共享内存> │  消费 SQE      │          │
│  │  SQE entries   │            │  执行 IO        │          │
│  │  ┌───┬───┬───┐ │            │                │          │
│  │  │Q0 │Q1 │Q2 │ │            └────────────────┘          │
│  │  └───┴───┴───┘ │                   │                    │
│  │       │        │                   │                    │
│  └───────┼────────┘                   ▼                    │
│          │                  ┌────────────────┐              │
│          └─────────────────>│  CQ (完成队列) │              │
│                             │  CQE entries  │              │
│                             │  ┌───┬───┬───┐│              │
│                             │  │C0 │C1 │C2 ││              │
│                             │  └───┴───┴───┘│              │
│                             └────────────────┘              │
│                                                             │
│  关键：SQ/CQ 是 mmap 共享内存，无系统调用即可通信             │
│  SQPOLL 模式下，内核常驻内核线程消费 SQ，用户态零 syscall    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

#### 3.2 核心优势

| 特性 | 说明 |
|------|------|
| **真异步** | 内核负责等就绪 + 拷数据，用户拿到 CQE 时数据已就位 |
| **零系统调用** | SQPOLL 模式下提交 IO 完全无 syscall |
| **批量提交** | 一次提交多个 SQE，内核批量处理 |
| **统一接口** | 文件、网络、定时器、futex 都走同一接口 |
| **可扩展** | fixed files、registered buffers 等优化特性 |

#### 3.3 liburing 完整代码示例

下面是一个完整的 echo server 示例（使用 liburing 简化 API）：

```cpp
// 编译: g++ -std=c++17 -O2 -o server server.cpp -luring
#include <liburing.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

constexpr int BACKLOG = 128;
constexpr int BUF_SIZE = 1024;
constexpr int MAX_EVENTS = 256;

struct conn_info {
    int fd;
    char buf[BUF_SIZE];
};

int main() {
    // 1. 创建监听 socket
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenfd, BACKLOG);

    // 2. 初始化 io_uring
    struct io_uring ring;
    io_uring_queue_init(MAX_EVENTS, &ring, 0);

    // 3. 注册 accept 操作（多 shot 模式，一次注册可接受多个连接）
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_multishot_accept(sqe, listenfd, NULL, NULL, 0);
    io_uring_sqe_set_data64(sqe, 0xFFFFFFFF);  // 标记 listenfd 的请求

    printf("io_uring echo server on :8080\n");

    // 4. 事件循环
    while (true) {
        io_uring_submit_and_wait(&ring, 1);

        struct io_uring_cqe* cqe;
        unsigned head;
        unsigned count = 0;

        io_uring_for_each_cqe(&ring, head, cqe) {
            count++;
            uint64_t key = io_uring_cqe_get_data64(cqe);
            int res = cqe->res;

            if (key == 0xFFFFFFFF) {
                // accept 完成事件：res 是新 fd
                if (res >= 0) {
                    int cfd = res;
                    auto* info = new conn_info{ .fd = cfd };
                    // 为新连接提交 read
                    struct io_uring_sqe* rs = io_uring_get_sqe(&ring);
                    io_uring_prep_recv(rs, cfd, info->buf, BUF_SIZE, 0);
                    io_uring_sqe_set_data(rs, info);
                }
            } else {
                // read 完成事件
                auto* info = (conn_info*)key;
                if (res <= 0) {
                    // 对端关闭或出错
                    close(info->fd);
                    delete info;
                } else {
                    // 立即提交 write 回写（echo）
                    struct io_uring_sqe* ws = io_uring_get_sqe(&ring);
                    io_uring_prep_send(ws, info->fd, info->buf, res, 0);
                    io_uring_sqe_set_data(ws, info);
                    // 写完后继续提交 read
                    // （实际工程中要追踪状态机，这里简化）
                }
            }
        }
        io_uring_cq_advance(&ring, count);
    }

    io_uring_queue_exit(&ring);
    close(listenfd);
    return 0;
}
```

#### 3.4 io_uring 的高级特性

| 特性 | 说明 |
|------|------|
| **SQPOLL** | 内核常驻一个内核线程消费 SQ，用户态完全无 syscall |
| **multishot accept** | 一次 accept 提交，可接收多个连接 |
| **fixed files** | 注册固定 fd 表，避免内核 fd 查找开销 |
| **registered buffers** | 注册固定 buffer，内核锁定页表，避免每次 DMA 映射 |
| **polling 模式** | BUSY_POLL 模式下，内核主动 poll 网卡，避免硬件中断延迟 |

#### 3.5 io_uring 的现状与适用场景

**现状**：
- Linux 5.1 引入，5.6 之后逐步成熟，5.18+ 网络支持完善。
- 已被诸多高性能项目采用：Redis、Ceph、QEMU、ScyllaDB 等。
- 但生态相比 epoll 仍年轻，调试工具少。

**适用场景**：
- 高 IOPS 存储服务（数据库、对象存储）。
- C10M 级网络服务。
- 对延迟极敏感的高频交易系统。
- 混合 IO 场景（文件 + 网络 + 定时器统一管理）。

**不适合**：
- 通用 Web 服务（epoll 已足够）。
- 临时原型/小工具（API 复杂度高）。
- 跨平台项目（io_uring 仅 Linux 5.1+）。

---

## 七、五种 IO 模型横向对比

| 模型 | 阶段一（等就绪） | 阶段二（搬数据） | 线程状态 | 是否真异步 | 典型 API |
|------|-----------------|------------------|----------|-----------|----------|
| 阻塞 IO | 阻塞，挂起线程 | 用户搬 | 睡眠 | ❌ | `read()` |
| 非阻塞 IO | 非阻塞，立即返回 EAGAIN | 用户搬 | 运行（轮询） | ❌ | `read()` + `O_NONBLOCK` |
| IO 多路复用 | 阻塞在 select/poll/epoll | 用户搬 | 睡眠（批量等） | ❌ | `epoll_wait` + `read()` |
| 信号驱动 IO | 信号通知 | 用户搬 | 运行 | ❌ | `O_ASYNC` + SIGIO |
| 异步 IO | 内核等 | **内核搬** | 运行 | ✅ | `io_uring` / IOCP |

---

## 八、性能详细对比表（select / poll / epoll / io_uring）

| 维度 | select | poll | epoll | io_uring |
|------|--------|------|-------|----------|
| **fd 上限** | 1024 (FD_SETSIZE) | 进程 ulimit | 进程 ulimit | 进程 ulimit |
| **fd 集合存储** | 位图 (fd_set) | 数组 (pollfd[]) | 红黑树 + 就绪链表 | 共享内存环形队列 |
| **注册 fd** | 每次调用都要传 | 每次调用都要传 | 注册一次，红黑树维护 | 注册一次（可 fixed files 加速） |
| **就绪检查复杂度** | O(n) | O(n) | O(就绪数) | O(就绪数) |
| **用户态遍历** | O(n) | O(n) | O(就绪数) | O(就绪数) |
| **fd_set 复用** | 否（被改坏） | events/revents 分离 | 是 | 是 |
| **拷贝开销** | 每次传 fd_set | 每次传 pollfd[] | 注册时拷一次 | 共享内存零拷贝 |
| **数据搬运者** | 用户线程 | 用户线程 | 用户线程 | **内核** |
| **是否真异步** | ❌ 同步 | ❌ 同步 | ❌ 同步 | ✅ 异步 |
| **触发模式** | LT | LT | LT / ET | 完成事件 |
| **跨平台** | POSIX 通用 | POSIX 通用 | Linux 专有 | Linux 5.1+ 专有 |
| **C10K 支持** | ❌ | ⚠️ 性能差 | ✅ | ✅ |
| **C10M 支持** | ❌ | ❌ | ⚠️ 接近极限 | ✅ |
| **典型场景** | 兼容性代码 | 偶尔用于非 Linux | Web 服务/网关/数据库 | 高性能存储/网络 |
| **代表项目** | 早期 Apache | - | Nginx, Redis, Netty | ScyllaDB, Ceph, 新版 Redis |

### 1 万并发连接场景下的对比

假设场景：1 万个长连接，每秒约 1000 个连接有事件，每次读 100 字节。

| 指标 | select | poll | epoll | io_uring |
|------|--------|------|-------|----------|
| 注册 fd 开销 | 1 万 bit × 每次调用 | 1 万 × 8 字节 × 每次调用 | 1 万 × O(log n) × 注册一次 | 1 万 × SQE（共享内存） |
| 就绪检查 CPU | 1 万次比较 | 1 万次比较 | 1000 次（只查就绪） | 1000 次 CQE |
| 内核/用户态切换 | 每次都全量拷 | 每次都全量拷 | 注册一次 + 就绪拷 | 共享内存零拷贝 |
| 实测 QPS | 无法承载 | ~1 万 QPS 极限 | ~50 万 QPS | ~100 万+ QPS |

---

## 九、工程选型建议

### 1. 按场景选模型

| 场景 | 推荐模型 | 理由 |
|------|---------|------|
| 客户端程序、连接数 < 10 | 阻塞 IO + 多线程 | 编程最简单，性能足够 |
| 连接数 100~1000 | 阻塞 IO + 线程池 | 简单可控 |
| Web 服务、API 网关（1k~10万连接） | epoll（LT 或 ET） | Linux 标准方案，生态成熟 |
| 数据库 / 消息队列 / 高并发中间件 | epoll + 多 Reactor 线程 | Nginx/Redis/Netty 路线 |
| 高性能存储 / 对象存储 | io_uring | 高 IOPS 场景有数量级优势 |
| 极高并发网络服务（C10M） | io_uring + SQPOLL | 突破 epoll 性能天花板 |
| 跨平台网络库 | epoll + kqueue + IOCP 抽象层 | Boost.Asio / libuv 思路 |
| UDP 服务（包边界清晰） | 信号驱动 IO（可选） | 偶尔用于特殊场景 |

### 2. 实际工程的注意点

**epoll 工程 tips**：
- 默认用 LT 模式，编程简单，出错率低。
- 只有在性能瓶颈确实在 `epoll_wait` 唤醒次数时，才考虑 ET。
- ET 模式下，fd **必须**设为非阻塞，且必须循环读到 `EAGAIN`。
- 用 `epoll_create1(EPOLL_CLOEXEC)` 避免子进程继承。
- 监听 fd 的 `accept` 在 ET 模式下要循环 accept 到 EAGAIN。
- 用 `EPOLLET` + `EPOLLONESHOT` 处理多线程场景，避免惊群。
- Linux 4.5+ 支持 `EPOLLEXCLUSIVE` 解决惊群问题。

**io_uring 工程 tips**：
- 用 liburing 而非裸 syscall，API 友好得多。
- SQPOLL 模式会常驻一个内核线程，仅在确实高频时启用。
- fixed files / registered buffers 是性能优化的关键，但管理复杂。
- 内核版本至少 5.10+，5.18+ 网络支持才完整。
- 调试工具少，慎重用于核心链路。

### 3. 何时该升级

```
是否需要从 epoll 升级到 io_uring？

QPS < 50 万                  → epoll 足够，不要折腾
50 万 < QPS < 200 万         → epoll 多 Reactor 线程还能扛
QPS > 200 万 或 极低延迟要求  → 考虑 io_uring
存储 IOPS > 100 万            → io_uring 是必须
```

---

## 十、常见误解辨析

**Q1：epoll 比 select/poll 快是因为它没有 fd 数量限制？**
不对。epoll 快的核心是**O(1) 就绪通知**（红黑树 + 就绪链表 + 回调），fd 数量只是表象。

**Q2：epoll_wait 是阻塞的，所以 epoll 是阻塞 IO？**
不对。`epoll_wait` 确实阻塞调用线程，但 epoll 是 IO 多路复用模型，不是阻塞 IO 模型。两者属于不同的 IO 模型分类。

**Q3：epoll 是异步 IO 吗？**
不是。epoll 是同步 IO 多路复用。阶段二（数据搬运）仍由用户线程自己做。POSIX 严格定义下，只有阶段二由内核完成的才是异步 IO。

**Q4：Nginx 用的是 epoll 还是 io_uring？**
目前 Nginx 主线仍是 epoll（macOS 用 kqueue）。社区有 io_uring patch 但未成为主流。

**Q5：Node.js / Go 用的是 epoll 吗？**
是的。libuv (Node.js) 和 Go runtime 的 netpoller 在 Linux 上都是 epoll 的封装，本质都是同步 IO + 事件循环。

**Q6：io_uring 会取代 epoll 吗？**
短期内不会。epoll 生态成熟、调试工具丰富，对绝大多数应用足够。io_uring 在高性能场景有数量级优势，会逐步蚕食高端市场，但 epoll 在通用 Web 服务领域还会长期存在。

**Q7：信号驱动 IO 为什么没人用？**
三个原因：(1) 信号处理函数中能做的事有限；(2) 信号合并导致就绪信息丢失；(3) 多 fd 共用一个 SIGIO，要自己分辨是哪个 fd 就绪，复杂度比 epoll 还高。

---

## 结语

Linux 五种 IO 模型的演进，本质是一场**"如何把 CPU 资源更高效地分配给 IO 等待"**的工程优化史：

- **阻塞 IO** 让出 CPU，但 1 连接 1 线程，资源浪费。
- **非阻塞 IO** 不让出 CPU，但 CPU 空转轮询，更浪费。
- **IO 多路复用** 让 1 个线程照看 N 个 fd，是目前 Linux 网络编程的绝对主力。从 select 到 poll 到 epoll，把"找就绪 fd"的复杂度从 O(n) 降到 O(1)。
- **信号驱动 IO** 想用信号避免轮询，但信号机制本身有诸多限制。
- **异步 IO** 让内核负责"等就绪 + 搬数据"两件事，用户线程完全解放。io_uring 是 Linux 真正的高性能异步 IO 实现。

**判断一个 IO 模型属于哪一类，永远回到两个问题**：
1. **数据就绪（阶段一）由谁等？** 用户线程等（阻塞/轮询）vs 内核等（信号/异步）。
2. **数据搬运（阶段二）由谁做？** 用户搬 = 同步 IO；内核搬 = 异步 IO。

按这个标准，**Linux 上前四种 IO 模型本质上都是同步 IO**，只有 io_uring 才是真正的异步 IO。这就是为什么陈硕说"阻塞和非阻塞都是同步 IO，只有使用了特殊的 API 才是异步 IO"——在 Linux 上，这个"特殊 API"在 io_uring 出现之前，长期是缺位的。
