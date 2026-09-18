按照你提供的大纲，我将为你详细梳理 Linux 环境下经典的 **System V IPC（进程间通信）** 体系。

System V IPC 是 UNIX System V 引入的三种高级进程间通信机制的统称：**共享内存、消息队列和信号量（灯）**。它们在底层设计上有着惊人的相似性，但又各自解决不同的并发与通信问题。

---

### 一、 全局认识与 IPC 命令

System V IPC 的核心设计理念是：**对象在内核中持久存在，通过全局唯一的整数 ID 来定位**。

#### 1. IPC 对象的生命周期
与管道不同，System V IPC 对象的生命周期随**内核**，而不是随进程。这意味着：一个进程创建了一个 IPC 对象后退出，该对象及其内部数据**依然存在于内核中**，直到被其他进程显式删除，或者系统重启。这带来了跨进程通信的便利，但也极易造成资源泄漏。

#### 2. 全局 IPC 命令（极其重要）
由于 IPC 对象在内核中不可见，Linux 提供了命令行工具来查看和删除它们：
*   **查看**：`ipcs` 
    *   `ipcs -m`：仅查看共享内存
    *   `ipcs -q`：仅查看消息队列
    *   `ipcs -s`：仅查看信号量
*   **删除**：`ipcrm`
    *   `ipcrm -m <shmid>`：删除指定 ID 的共享内存
    *   `ipcrm -q <msqid>`：删除消息队列

#### 3. 唯一标识：Key 与 ID
*   **Key (`key_t`)**：类似文件的绝对路径。希望通信的进程通过约定一个相同的 Key 值来找到彼此。通常使用 `ftok()` 函数通过文件路径生成。
*   **ID (`int`)**：类似文件描述符。当进程通过 Key 创建或获取 IPC 对象时，内核返回一个 ID，后续操作都使用这个 ID。ID 是进程私有的句柄。

---

### 二、 共享内存

这是**最快**的 IPC 方式。因为进程直接读写同一块物理内存，省去了数据在内核态和用户态之间的来回复制。

#### 1. 核心特点
*   **极速**：零拷贝通信。
*   **无同步**：它本身不提供任何互斥或同步机制。如果两个进程同时写，会产生数据覆盖。**必须配合信号量或互斥锁使用**。

#### 2. 使用步骤与 API

**步骤 1：创建/获取共享内存**
```c
#include <sys/ipc.h>
#include <sys/shm.h>
int shmget(key_t key, size_t size, int shmflg);
```
*   `size`：共享内存大小（建议是页大小 4096 的整数倍）。
*   `shmflg`：权限与创建标志，如 `IPC_CREAT | 0666`。如果希望不存在时报错，加上 `IPC_EXCL`。

**步骤 2：映射到进程虚拟空间**
```c
void *shmat(int shmid, const void *shmaddr, int shmflg);
```
*   `shmaddr`：通常传 `NULL`，让内核自己选择合适的虚拟地址映射。
*   `shmflg`：`SHM_RDONLY` 表示只读挂载，默认可读写。
*   返回映射的首地址。此后，进程操作这段内存就像操作普通 `malloc` 的内存一样。

**步骤 3：解除映射**
```c
int shmdt(const void *shmaddr);
```
*   进程不再使用时调用，断开虚拟地址与物理内存的连接。注意：这**不会删除**共享内存。

**步骤 4：控制与销毁**
```c
int shmctl(int shmid, int cmd, struct shmid_ds *buf);
```
*   `cmd` 常用 `IPC_RMID`，用于将共享内存标记为销毁。**注意**：只有当所有挂载该共享内存的进程都调用了 `shmdt` 后，内核才会真正将其物理释放。

#### 3. 简单示例片段
```c
// 进程 A
key_t key = ftok("/tmp/myshm", 1);
int shmid = shmget(key, 4096, IPC_CREAT | 0666);
char *addr = (char *)shmat(shmid, NULL, 0);
strcpy(addr, "Hello from A");
shmdt(addr);

// 进程 B
int shmid = shmget(key, 4096, 0666);
char *addr = (char *)shmat(shmid, NULL, 0);
printf("B reads: %s\n", addr);
shmdt(addr);
```

---

### 三、 消息队列

消息队列是保存在内核中的消息链表。它克服了管道只能传递无格式字节流的缺点，消息是有类型的。

#### 1. 核心特点
*   **有格式**：每条消息都有特定的类型（`long mtype`）和负载。
*   **生命周期长**：随内核。
*   **自带边界**：面向记录的通信，写一次就是一条消息，读一次也是一条完整消息，不会像管道那样黏包。
*   **缺点**：性能不如共享内存，因为每次收发都要经过内核拷贝，且内核对单条消息大小和队列总大小有上限限制（`/proc/sys/kernel/msgmax`, `msgmnb`）。

#### 2. 使用步骤与 API

**步骤 1：创建/打开消息队列**
```c
int msgget(key_t key, int msgflg);
```

**步骤 2：发送消息**
```c
struct my_msg {
    long mtype;     // 必须是正数，表示消息类型
    char mtext[100]; // 负载数据
};

int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);
```
*   `msgp`：指向自定义的消息结构体。
*   `msgsz`：负载 `mtext` 的大小，**不包含** `mtype` 的 4 个字节。
*   `msgflg`：通常为 0（队列满时阻塞）；`IPC_NOWAIT`（队列满时立即返回错误）。

**步骤 3：接收消息**
```c
ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, int msgflg);
```
*   `msgtyp`：消息过滤策略。
    *   `= 0`：读取队列中的第一条消息（先进先出）。
    *   `> 0`：读取类型为 `msgtyp` 的第一条消息。
    *   `< 0`：读取类型小于等于 `msgtyp` 绝对值的、类型值最小的第一条消息。
*   这个特性使得消息队列可以实现优先级队列的效果。

**步骤 4：控制与销毁**
```c
int msgctl(int msqid, int cmd, struct msqid_ds *buf);
```
*   同样使用 `IPC_RMID` 删除队列。

---

### 四、 信号灯

这里需要特别注意：**System V 信号量与前面讲的 POSIX 信号量（`sem_wait/sem_post`）有本质区别！** System V 信号量操作的是“信号量集（数组）”，而不是单个值。

#### 1. 核心特点与含义
*   **概念**：System V 信号量是一个或多个信号量的集合。每次申请或释放资源时，可以原子性地对整个集合中的多个信号量进行加减操作。
*   **复杂性**：初始化极其繁琐，这是它被诟病最多的地方。因为 `semget` 创建集合时无法原子性地初始化所有值，必须通过 `semctl` 逐个 SETVAL，这在多进程并发初始化时容易产生竞态条件。

#### 2. 使用步骤与 API

**步骤 1：创建/获取信号量集**
```c
int semget(key_t key, int nsems, int semflg);
```
*   `nsems`：集合中信号量的个数。如果是单个信号量，传 1。

**步骤 2：初始化信号量（坑点）**
创建者必须使用 `semctl` 初始化。
```c
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int semctl(int semid, int semnum, int cmd, ...);
// 初始化第 0 个信号量的值为 1：
union semun arg;
arg.val = 1;
semctl(semid, 0, SETVAL, arg);
```

**步骤 3：P/V 操作（核心）**
通过 `semop` 函数，可以一次操作多个信号量。
```c
struct sembuf {
    unsigned short sem_num; // 信号量在集合中的索引
    short sem_op;          // 操作数：-1 为 P 操作，+1 为 V 操作
    short sem_flg;         // 标志：通常为 0（阻塞），或 IPC_NOWAIT
};

int semop(int semid, struct sembuf *sops, size_t nsops);
```
*   如果操作数 `sem_op` 为负数，且当前信号量值不够减，进程会阻塞睡眠，直到其他进程执行了 V 操作。

**步骤 4：销毁**
```c
semctl(semid, 0, IPC_RMID);
```

---

### 五、 补充与拓展（进阶必知）

#### 1. System V IPC 的致命缺陷：引用计数与内核持久性
*   **文件描述符的优雅**：管道和 Socket 使用文件描述符，进程退出时内核会自动关闭它们，资源自动回收。
*   **System V IPC 的坑**：进程崩溃或异常退出时，IPC 对象依然残留在内核中。如果开发人员忘记调用 `IPC_RMID`，会导致内存泄漏，最终耗尽系统资源。必须养成严格使用 RAII（C++）或 `atexit` 机制清理 IPC 资源的习惯。

#### 2. System V vs. POSIX IPC
在现代 Linux 编程中，通常**推荐使用 POSIX IPC**（如 `mq_open` 消息队列, `sem_open` 信号量, `shm_open` 共享内存）来替代 System V IPC：
*   **接口更优雅**：POSIX IPC 使用文件描述符（类似文件操作），而 System V 使用全局整数 ID，容易冲突或误用。
*   **更安全**：POSIX 信号量是单一值，且初始化简单。
*   **可扩展性**：POSIX IPC 更容易与 `epoll` / `select` 等 I/O 多路复用机制结合。

#### 3. 性能对比
*   **最快**：共享内存 (零拷贝)。
*   **次之**：消息队列 (一次系统调用传递一条结构化消息，内核拷贝)。
*   **最慢**：信号量 (不传数据，仅做同步阻塞)。
*   如果需要传输大量数据，标准套路是：**用消息队列或信号量传“控制信号”，用共享内存传“实际大数据”**。

### 总结
System V IPC 是 UNIX 系统的瑰宝，理解它对于阅读老一代 C 源码（如早期的数据库、中间件）至关重要。但在新开发现代 C++ 网络服务时，除非需要与旧系统交互，否则应优先考虑 Unix Domain Socket、共享内存 + `pthread_mutex` (设置 `PTHREAD_PROCESS_SHARED`) 或 POSIX IPC 机制。

