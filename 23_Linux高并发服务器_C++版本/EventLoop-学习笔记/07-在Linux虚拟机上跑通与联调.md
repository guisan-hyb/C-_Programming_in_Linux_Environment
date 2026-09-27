# 07 · 在 Linux 虚拟机上跑通与联调（验收手册）

> 说明：这份笔记是在 Windows 上"读代码"写出来的（本机没有 Linux 环境，无法编译验证）。下面的命令都是标准步骤，**如果哪一步报错，把报错发我**，我按真实输出帮你定位 —— 这本身就是"复现"的一部分。

## 1. 环境准备（虚拟机里执行一次）

```bash
sudo apt update                      # Debian/Ubuntu 系
sudo apt install -y build-essential cmake python3
g++ --version && cmake --version && python3 --version

ulimit -n            # 看能打开多少 fd，建议 ≥ 1024；太小压测会 EMFILE
```

把 `EventLoop` 整个目录拷进虚拟机（共享文件夹 / scp / 直接拖拽都可以），然后：

```bash
cd EventLoop
ls          # 应该看到 CMakeLists.txt config.ini *.cpp *.h
```

## 2. 构建

### 方式 A：CMake（推荐）

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j"$(nproc)"
cp ../config.ini .        # ★ 必须！程序按“当前工作目录”找 config.ini
ls                        # 应该有 event_server 和 config.ini
```

### 方式 B：一行 g++（改代码后快速重编，最省事）

```bash
cd EventLoop
g++ -std=c++17 -O2 -g -Wall -Wextra -pthread *.cpp -o event_server
cp config.ini . # 本来就在，忽略
```

### 调试推荐：带 ASAN/UBSAN

```bash
g++ -std=c++17 -O1 -g -fsanitize=address,undefined -pthread *.cpp -o event_server_asan
```

内存/越界问题会立刻暴露（顺便能验证第 6 章讲的 `shared_ptr` 保命技巧确实有效）。

## 3. 启动

```bash
./event_server
```

期望输出（fd 号码可能不同）：

```
server.port = 12345
server _listen_fd is 3
server _event_fd is 4
server _epoll_fd is 5
construt event_loop num is 4          ← 注意原代码拼写就是 construt
```

另开一个终端确认监听：

```bash
ss -ltnp | grep 12345
ps -T -p "$(pgrep -f event_server)"   # 应看到 1 主线程 + 4 个 IO 线程
```

## 4. 十个验收用例（全部通过 = 复现成功）

把 [echo_client.py](echo_client.py) 拷到虚拟机，和 `event_server` 放一起或直接用绝对路径。

| # | 目的 | 命令 | 期望结果 | 失败时先查 |
| --- | --- | --- | --- | --- |
| 1 | 服务能起来 | `./event_server` | 打印 4 个 fd + `thread_num` | `config.ini` 是否在当前目录；端口是否被占（`ss -ltnp`） |
| 2 | 基本回显 | `python3 echo_client.py echo` | 每个包原样返回，`type` 一致 | 防火墙；客户端头字节序是否也用了大端 |
| 3 | 半包（包体分两次） | `python3 echo_client.py split` | 服务端等到第二段才回包 | `BODY_RECVING` 状态是否正确保留 |
| 4 | 半截头（头分两次） | `python3 echo_client.py split-head` | 同样正常回显 | `_head_buf->_offset` 续写偏移是否正确 |
| 5 | 粘包（一次发多包） | `python3 echo_client.py sticky` | 一次收到全部回包，数量一致 | 读完一包后状态是否复位（4 行复位最容易漏） |
| 6 | 多连接并发 | `python3 echo_client.py echo -n 8` | 8 条连接各自正常回显；`Accepted fd=` 打印 8 次 | fd 取模分发是否正常 |
| 7 | 超长包被拒 | `python3 echo_client.py oversize` | 服务端打印 `msg body too big` 并断开该连接 | `BUFF_SIZE` 校验是否存在 |
| 8 | 空包体缺陷复现 | `python3 echo_client.py zero` | **连接被断开**（这是 D1 缺陷，不是你的 bug） | 见 [06 章 D1](06-生命周期、错误处理与已知缺陷.md) |
| 9 | 触发 EPOLLOUT 续传 | `python3 echo_client.py flood` | 服务端不卡死、不崩、CPU 正常，客户端最终能读完回包 | `handle_epollout` 是否补齐；`mod_fd` 是否加了 EPOLLOUT |
| 10 | 优雅退出 | 在服务端终端按 `Ctrl-C` | 打印顺序：`Received signal 2...` → 各 `io_thread receive exit eventfd` → `receive exit eventfd` → `Server exit` → `IOThread join exit` ×4 → `EventLoop exit` → `IOThread 【i】exit` ×4 | 是否忘了 `StopIOThread`（会卡住不退出） |

多连接并发那一条，另开终端观察线程与 fd：

```bash
top -H -p "$(pgrep -f event_server)"          # 看 4 个 IO 线程都在
ls -l /proc/$(pgrep -f event_server)/fd       # 看连接 fd 数量
```

## 5. 排障工具箱

| 工具 | 用途 | 例子 |
| --- | --- | --- |
| `ss -ltnp` | 看监听与连接状态 | `ss -tnp state established '( sport = :12345 )'` |
| `strace -f` | 看真实系统调用（最有用的一个） | `strace -f -e trace=network,epoll_wait,read,write ./event_server` |
| `top -H` / `ps -T` | 看线程级 CPU | `top -H -p <pid>` |
| `gdb` | 卡住时看每个线程栈 | `gdb -p <pid>` 然后 `thread apply all bt` |
| `perf top` | 看热点 | `perf top -p <pid>` |
| ASAN/UBSAN | 内存与未定义行为 | 见第 2 节方式 |
| `valgrind` | 泄漏与竞态（慢但直观） | `valgrind --leak-check=full ./event_server`（可验证 D2 的泄漏） |

对照阅读：用 `strace -f -e trace=epoll_wait,accept4,read,write ./event_server` 跑一遍用例 2~5，你能**亲眼看到**"ET 下循环读到 EAGAIN"这件事：每次 `read` 之后紧跟一个返回 `-1 EAGAIN` 的 `read`，然后才回到 `epoll_wait`。这比看代码直观得多。

## 6. 虚拟机网络注意事项

- **在虚拟机里自己测最省事**：`127.0.0.1:12345` 直接可用。
- 想让 **Windows 主机**上跑客户端连虚拟机里的服务：
  - NAT 模式：需要在虚拟机软件里做端口转发（把 guest 的 12345 映射到 host 的 12345）；
  - 桥接模式：给虚拟机固定 IP，主机直接用该 IP；
  - Host-Only：主机与 guest 之间可通，但 guest 无外网。
- 检查 guest 防火墙：`sudo ufw status`，必要时 `sudo ufw allow 12345/tcp`。
- 服务端 `bind` 的是 `INADDR_ANY`，所以不限制网卡，问题一般出在 NAT/防火墙。

## 7. 常见报错对照表

| 现象 | 原因 | 处理 |
| --- | --- | --- |
| `Failed to load config.ini` | 启动目录里没有 `config.ini` | 把 `config.ini` 拷到当前工作目录（`build/` 里跑就拷进 `build/`） |
| `bind: Address already in use` | 12345 被占 / 上次进程没退 | `ss -ltnp \| grep 12345`，`kill` 掉旧进程 |
| 编译报 `sys/epoll.h: No such file` | 在 Windows 上编译 | 换到 Linux 虚拟机里编译 |
| `Accepted fd=..` 之后无反应 | 客户端头格式不对（长度字段没转大端） | 用 `echo_client.py` 对照 `struct.pack("!HH", type, len)` |
| `Ctrl-C` 后卡住不退出 | 少了 `StopIOThread` | 见 [06 章第 1 节](06-生命周期、错误处理与已知缺陷.md) |
| 客户端收到乱码长度 | 字节序错误 | 发送端 `htons`、接收端 `ntohs` 要配对 |

## 8. 验收结论模板（自己填，写完拍照/贴给我也行）

```
[ ] 环境：Ubuntu 版本____  g++ 版本____  ulimit -n ____
[ ] 用例 1~10 全部通过，其中第 8 项复现了 D1 缺陷
[ ] strace 观察到“read 循环到 EAGAIN”
[ ] 我理解了：ET 为什么要读到 EAGAIN、EPOLLOUT 为什么要用完就摘、eventfd 为什么不能省
[ ] 遗留疑问：____________
```
