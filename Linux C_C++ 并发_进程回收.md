在 Linux C/C++ 编程中，进程的创建（`fork`）和执行（`exec`）固然重要，但**进程的回收**同样是系统健壮性的关键一环。如果只管生不管埋，系统资源很快就会耗尽。

下面为您详细解析进程回收的核心机制、API 细节，以及在高并发场景下的进阶应用与避坑指南。

---

### 一、 为什么需要回收？（僵尸进程的由来）

当一个子进程退出（无论是正常调用 `exit` 还是被信号杀死）时，内核会释放该进程占用的几乎所有资源：内存空间、文件描述符等。但是，**内核会保留该进程的进程表项中的一小部分信息**，包括：
1.  **进程 ID (PID)**
2.  **终止状态**（退出码或致死信号）
3.  **资源使用统计**（CPU 时间、内存使用量等）

此时，该进程变成了**僵尸进程（Zombie，状态标识为 `Z`）**。保留这些信息的目的，是为了让父进程在将来某个时刻能够查询子进程的“死因”。

**如果父进程不回收：**
僵尸进程的 PID 和内核栈等结构会一直被占用。由于系统的 PID 数量是有限制的（默认通常是 32768，虽可调整但仍有限），大量僵尸进程最终会导致系统无法创建新进程，报错 `Resource temporarily unavailable` 或 `fork: retry`。

---

### 二、 核心回收 API：`wait` 与 `waitpid`

这两个函数定义在 `<sys/wait.h>` 中，它们的作用是等待子进程状态改变（终止、停止、继续），并回收其残留资源。

#### 1. `wait` 函数
```c
pid_t wait(int *status);
```
*   **行为**：阻塞当前父进程，直到**任意一个**子进程终止。如果子进程已经是个僵尸，立即返回并清理。
*   **缺点**：无法指定等待哪个子进程，也无法非阻塞等待。

#### 2. `waitpid` 函数（实战主力）
```c
pid_t waitpid(pid_t pid, int *status, int options);
```
`waitpid` 提供了更精细的控制：

**A. `pid` 参数（指定目标进程）：**
*   `pid > 0`：等待 PID 等于 `pid` 的特定子进程。
*   `pid == -1`：等待任意子进程（等价于 `wait`）。
*   `pid == 0`：等待与父进程同一个进程组的任意子进程。
*   `pid < -1`：等待进程组 ID 等于 `pid` 绝对值的任意子进程。

**B. `options` 参数（控制行为）：**
*   `0`：默认阻塞等待。
*   **`WNOHANG`**：**非阻塞模式**。如果没有子进程退出，立即返回 `0`；如果有子进程退出，返回子进程 PID。这在网络服务器的事件循环中极其重要。
*   `WUNTRACED`：报告停止运行的子进程状态。
*   `WCONTINUED`：报告被信号 `SIGCONT` 恢复运行的子进程状态。

**C. 返回值：**
*   `> 0`：成功回收的子进程 PID。
*   `0`：如果使用了 `WNOHANG`，表示暂时没有子进程退出。
*   `-1`：出错（例如没有子进程，或者被信号中断，此时 `errno == EINTR`）。

#### 3. 状态解析（Macros）
`status` 是一个位掩码，不能直接读取。必须使用 `<sys/wait.h>` 提供的宏来解析：

*   `WIFEXITED(status)`：如果子进程正常退出（调用 `exit` 或 `return`），返回真。
    *   `WEXITSTATUS(status)`：获取正常退出时的退出码（低 8 位）。
*   `WIFSIGNALED(status)`：如果子进程是被信号异常终止的，返回真。
    *   `WTERMSIG(status)`：获取导致终止的信号编号。
    *   `WCOREDUMP(status)`：是否产生了核心转储文件。
*   `WIFSTOPPED(status)`：如果子进程被信号暂停（如 `SIGSTOP`），返回真。
    *   `WSTOPSIG(status)`：获取导致暂停的信号编号。

---

### 三、 经典实战：非阻塞轮询回收

在服务端开发中，父进程通常很忙（如在处理网络 I/O），不能死等子进程退出。通常的做法是结合非阻塞的 `waitpid` 进行轮询：

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

void reap_children_non_blocking() {
    pid_t pid;
    int status;
    
    // 循环回收，WNOHANG 保证没有僵尸进程时立即返回 0，不会卡住父进程
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("成功回收子进程 PID: %d\n", pid);
        if (WIFEXITED(status)) {
            printf("  退出码: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("  被信号 %d 杀死\n", WTERMSIG(status));
        }
    }
}
```
*调用时机：可以在主循环的每次迭代时调用，或者定时器触发时调用。*

---

### 四、 进阶拓展与补充

#### 1. 异步优雅回收：`SIGCHLD` 信号机制

轮询虽然能用，但可能浪费 CPU 周期。更优雅的方式是利用信号通知。

当子进程终止或停止时，内核会向父进程发送 `SIGCHLD` 信号。父进程可以注册一个信号处理函数，在其中调用 `waitpid` 进行回收。

**避坑指南（极其重要）：**
*   **信号不排队**：`SIGCHLD` 是标准信号，不支持排队。如果在处理函数执行期间，有多个子进程同时死亡，父进程只会收到一个 `SIGCHLD`。
*   **解决方案**：在信号处理函数中，**必须使用 `while` 循环配合 `WNOHANG`** 进行回收，直到 `waitpid` 返回 0 或 -1，确保把所有变成僵尸的子进程“一网打尽”。
*   **异步信号安全**：在信号处理函数中，只能调用异步信号安全的函数。`printf` 不是！实战中应使用 `write` 替代，或者仅设置一个标志位，在主循环中检测标志位再调用复杂的回收打印逻辑。

```c
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    // 必须用 while 循环清理所有僵尸
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // 这里为了演示用 write，实际工程中通常只设 flag
        char buf[64];
        int len = snprintf(buf, sizeof(buf), "Reaped child %d\n", pid);
        write(STDOUT_FILENO, buf, len);
    }
}

// 注册信号
// struct sigaction sa;
// sa.sa_handler = sigchld_handler;
// sigemptyset(&sa.sa_mask);
// sa.sa_flags = SA_RESTART | SA_NOCLDSTOP; // SA_RESTART 防止系统调用被打断
// sigaction(SIGCHLD, &sa, NULL);
```
*   `SA_NOCLDSTOP`：只在子进程终止时发信号，子进程停止（如 `Ctrl+Z`）时不发，避免打扰父进程。

#### 2. 孤儿进程的归宿
如果父进程先死了，子进程还在运行，子进程就成了**孤儿进程**。
Linux 的机制是：孤儿进程会被 `init` 进程（PID 为 1，现代系统通常是 `systemd`）或指定的“subreaper”收养。
由于 `init` 的设计就是不断调用 `wait` 回收子进程，所以**孤儿进程退出后会被自动回收，不会变成僵尸，对系统无害。**

#### 3. 双重 Fork：彻底脱离父进程控制
在某些场景（如编写 Daemon 守护进程，或父进程不想管子进程死活），我们希望避免产生僵尸进程，也不想去 `wait`。
**双重 Fork 技术**：
1. 进程 A `fork` 出进程 B。
2. 进程 A 立刻 `waitpid` 回收进程 B（或者立刻退出）。
3. 进程 B 再次 `fork` 出进程 C。
4. 进程 B 立刻 `exit(0)` 退出。
5. 此时，进程 C 成了孤儿，被 `init` 收养。
这样，进程 A 完全不需要关心进程 C 的生命周期，进程 C 的回收由 `init` 自动完成。

#### 4. 高级 API：`waitid`
POSIX 定义了一个更强大的接口：
```c
int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options);
```
相比 `waitpid`，`waitid` 提供了更丰富的信息（通过 `siginfo_t` 返回子进程的 UID、退出码、致死信号等），并且支持 `WNOWAIT` 选项（只查看状态，但不回收，留给以后再回收）。

#### 5. Linux 特有 API：`clone` 与 `exit_signal`
在 Linux 底层，进程是通过 `clone` 创建的。`clone` 接受一个 `exit_signal` 参数。如果设置为 `SIGCHLD`，则子进程退出时向父进程发 `SIGCHLD`（默认行为）。如果设置为 `0` 或其他信号，父进程将收不到 `SIGCHLD`，这常用于内核线程或特殊线程的创建。

#### 6. C++ 视角：RAII 与子进程管理
C++ 程序员习惯用 RAII 管理资源，但 `fork` 出来的子进程是系统级资源，不属于对象内存模型。
*   **痛点**：如果父进程异常崩溃（如段错误），子进程会变成孤儿，可能继续运行导致逻辑错乱。
*   **对策**：
    *   使用 `prctl(PR_SET_PDEATHSIG, SIGTERM)` 在子进程中设置：如果父进程死亡，内核自动给子进程发 `SIGTERM`。（注意：此调用必须在 `fork` 后的子进程中立刻执行）。
    *   使用现代 C++ 库（如 Boost.Process）或封装专用的 `Process` 类，在析构函数中自动调用 `waitpid` 或发送终止信号。

#### 7. 排查僵尸进程的工具
*   `ps -elf | grep Z` 或 `ps aux | awk '{print $8}' | grep Z`：查找状态为 `Z` 的进程。
*   `top`：查看系统当前有多少僵尸进程。
*   `cat /proc/<pid>/status`：查看僵尸进程的详细信息，特别是 `State` 字段和 `PPid`（找到罪魁祸首的父进程）。

### 总结

进程回收是 Linux 编程中防止资源泄漏的最后防线。
*   对于简单同步逻辑，用 `wait` / `waitpid` 阻塞等待。
*   对于高并发网络服务，**必须**采用 `SIGCHLD` 信号 + `waitpid(WNOHANG)` 循环，或者在事件循环（如 `epoll`）中结合 `eventfd` / `signalfd` 进行异步回收。
掌握 `waitpid` 的细节，尤其是 `WNOHANG` 和状态解析宏，是后台开发者的必备技能。

