在 Linux 网络编程中，当对非阻塞 Socket（结合 `epoll`/`select`）调用 `read`/`recv` 或 `write`/`send` 时，如果函数返回 `-1`，你必须通过 `errno` 来判断到底发生了什么。

`EAGAIN`、`EWOULDBLOCK` 和 `EINTR` 是其中最常见、也最核心的三个错误码。理解它们的区别并正确处理，是保证服务器稳定运行的基础。

---

### 一、 核心概念解析

#### 1. `EAGAIN` (Error Again, 错误码 11)
*   **含义**：“资源暂时不可用”。
*   **发生场景**：在**非阻塞** I/O 中，你尝试读取数据，但接收缓冲区是空的；或者你尝试写入数据，但发送缓冲区已经满了。
*   **本质**：这不是一个真正的“错误”，而是一种**正常的阻塞状态**。系统在说：“我现在没数据给你（或者装不下了），你等会儿再来试一次吧。”

#### 2. `EWOULDBLOCK` (Error Would Block)
*   **含义**：“操作将会阻塞”。
*   **发生场景**：与 `EAGAIN` 完全相同。
*   **本质**：在 Linux 系统中，`EWOULDBLOCK` 和 `EAGAIN` 的值是**完全相等**的（都等于 11）。它们只是同一个意思的两种不同历史叫法。早期 BSD Unix 用 `EWOULDBLOCK`，System V Unix 用 `EAGAIN`，POSIX 标准为了兼容两者，将它们定义为同一个值。

#### 3. `EINTR` (Error Interrupted, 错误码 4)
*   **含义**：“系统调用被信号中断”。
*   **发生场景**：当进程正在执行慢系统调用（如阻塞型的 `read`、`epoll_wait`，或非阻塞型的 `read` 但执行期间发生信号）时，内核收到一个信号（如 `SIGCHLD` 子进程状态改变，或定时器信号），内核会打断当前的系统调用，先去执行信号处理函数。执行完毕后，系统调用返回 `-1`，并设置 `errno = EINTR`。
*   **本质**：这也不是真正的错误，只是操作系统对进程说：“不好意思打断一下，处理个急事，你刚才做的事情需要重做一遍。”

---

### 二、 为什么要严格区分它们？（处理策略完全不同）

在网络底层框架代码中，遇到这三个错误码的处理逻辑有着天壤之别：

*   **遇到 `EAGAIN` / `EWOULDBLOCK`**：
    *   **读操作**：表示缓冲区被你读干了。必须立刻 `break` 退出读循环，结束当前处理，**等待下一次 `epoll_wait` 通知**。
    *   **写操作**：表示缓冲区写满了。必须立刻 `break` 退出写循环，**将未发送的数据暂存到应用层发送队列**，并向 `epoll` 注册 `EPOLLOUT` 事件，等待下次可写时继续发送。
*   **遇到 `EINTR`**：
    *   **处理策略**：**绝对不能退出循环或放弃操作！** 因为这只是被信号打断了一下，数据可能还在缓冲区里没读完。正确的做法是直接 `continue`，重新发起一次 `read`/`write` 系统调用。

---

### 三、 标准代码实战（ET 模式下的正确写法）

在 `epoll` 的边缘触发（ET）模式下，必须循环读取直到返回 `EAGAIN`。以下是处理读事件时绝对标准的代码模板：

```c
while (1) {
    ssize_t count = recv(client_fd, buffer, sizeof(buffer), 0);
    
    if (count > 0) {
        // 1. 成功读到数据，处理数据...
        // (比如追加到 Session 的 DataBuf 中)
        continue; // 继续尝试读，榨干内核缓冲区
    } 
    else if (count == 0) {
        // 2. 对端正常关闭了连接
        printf("Client disconnected\n");
        close(client_fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
        break; // 跳出循环
    } 
    else { // count == -1，出错了
        if (errno == EINTR) {
            // 3. 被信号中断，绝对不能放弃！继续重试！
            continue; 
        } 
        else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 4. 缓冲区已读干，这才是正常退出循环的条件！
            break; 
        } 
        else {
            // 5. 真正的不可恢复错误（如 ECONNRESET 连接被重置）
            perror("recv error");
            close(client_fd);
            epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
            break;
        }
    }
}
```

---

### 四、 总结对比表

| 错误码 | 数字值 | 含义 | 发生原因 | 正确处理动作 | 是否致命 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`EAGAIN`** | 11 | 资源暂不可用 | 非阻塞读时缓冲区空，或写时缓冲区满 | `break`，等待下次 `epoll` 通知 | 否 |
| **`EWOULDBLOCK`**| 11 | 操作将阻塞 | 同上（Linux 中等同于 EAGAIN） | 同上 | 否 |
| **`EINTR`** | 4 | 被信号中断 | 执行系统调用时收到信号 | `continue`，重新发起调用 | 否 |

**黄金法则**：
在非阻塞网络编程中，`EAGAIN` 是循环退出的标志，`EINTR` 是循环继续的标志。将它们写反了，或者漏写了其中一个，你的服务器要么会陷入死循环烧毁 CPU，要么会因为漏读数据导致连接莫名其妙断开。
