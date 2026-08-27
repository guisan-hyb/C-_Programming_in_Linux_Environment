在 Linux C/C++ 中，标准 I/O 的核心就是围绕**流（`FILE *`）**进行操作。流提供了一层高级抽象，屏蔽了底层系统调用（`read/write`）的复杂性，并引入了缓冲机制。

前面我们已经总结了流的打开/关闭和基本的输入输出。这里我们将对**流的操作（定位、状态、缓冲控制）**进行详细总结，并向底层和高级应用作深度拓展。

---

### 一、 流的定位操作 (移动读写指针)

流内部维护了一个当前读写位置。当读写交替进行，或者需要随机访问文件时，必须手动控制这个位置。

#### 1. 基础定位：`fseek` / `ftell` / `rewind`
*   **`int fseek(FILE *stream, long offset, int whence);`**
    *   `whence` 取值：`SEEK_SET`（开头）、`SEEK_CUR`（当前）、`SEEK_END`（末尾）。
    *   `offset` 可正可负。注意：**不能将流定位到文件开头之前**（会报错），但**可以定位到文件末尾之后**（会产生空洞文件）。
*   **`long ftell(FILE *stream);`**
    *   返回流当前的读写偏移量。失败返回 `-1L`。
*   **`void rewind(FILE *stream);`**
    *   等价于 `fseek(stream, 0L, SEEK_SET)`，但它**还会清除流的错误标志**。

#### 2. 大文件定位（64位支持）：`fseeko` / `ftello`
在 32 位系统中，`long` 类型通常是 32 位，最大只能表示 2GB 的偏移量。为了处理大文件，POSIX 提供了使用 `off_t` 类型的替代函数：
*   **`int fseeko(FILE *stream, off_t offset, int whence);`**
*   **`off_t ftello(FILE *stream *);`**
*   *注：在 Linux 64 位系统中编译（或定义 `_FILE_OFFSET_BITS=64`），`off_t` 默认是 64 位的。*

#### 3. 保存与恢复状态：`fgetpos` / `fsetpos`
*   **`int fgetpos(FILE *stream, fpos_t *pos);`**
*   **`int fsetpos(FILE *stream, const fpos_t *pos);`**
*   这组函数是 ANSI C 标准定义的。`fpos_t` 是一个抽象类型，除了记录偏移量，在某些特殊系统下可能还记录了多字节字符的转换状态。推荐在需要跨平台且只要求“回到原来位置”的场景下使用。

---

### 二、 流的状态与错误处理

当流操作（如 `fgetc`, `fgets`）失败或到达文件尾时，它们都返回 `EOF`（-1）。单凭返回值无法区分是“真的没数据了”还是“出错了”。

#### 1. 状态检查函数
*   **`int feof(FILE *stream);`**
    *   如果流**到达文件末尾**，返回非零值。
    *   **重要机制**：`feof` 只有在**尝试读取越过文件尾部之后**，才会返回真。它不能“预测”文件是否结束。
*   **`int ferror(FILE *stream);`**
    *   如果流**发生读写错误**（如磁盘坏道、网络断开），返回非零值。

#### 2. 清除标志：`clearerr`
*   **`void clearerr(FILE *stream);`**
    *   **功能**：强制清除流的 EOF 标志和错误标志。
    *   **应用场景**：如果对一个流 `fread` 读到了 EOF，流就被“锁死”在 EOF 状态，无法再读。此时如果外部又有新数据写入（比如管道或网络 socket），必须调用 `clearerr` 清除 EOF 状态，才能继续读取。

#### 3. 判断流是否可读/可写：广义的 `fflush`
```c
// 判断输出流是否成功落盘
int fflush(FILE *stream);
```
如果 `stream` 是输出流，`fflush` 会将用户态缓冲区数据刷入内核。如果刷新失败（如磁盘满），`fflush` 会返回 `EOF` 并设置流的错误标志。这是一个经常被忽视的错误检查点。

---

### 三、 流的高级控制：缓冲区管理

默认情况下，标准 I/O 自动分配缓冲区并选择缓冲模式。但你可以手动接管。

#### 1. 修改缓冲模式：`setvbuf` (最强大)
```c
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
```
*   **必须在流打开之后、任何 I/O 操作之前调用！**
*   **`mode` 取值**：
    *   `_IOFBF`：全缓冲。
    *   `_IOLBF`：行缓冲。
    *   `_IONBF`：无缓冲。
*   **`buf`**：如果你传入自己的数组，流就会使用你的数组作为缓冲区。如果传 `NULL`，标准库会自动分配一个。
*   *应用场景*：在做高频大量日志输出时，为了避免标准库频繁 `malloc/free` 内部缓冲区，可以传入一个静态大数组（如 `char buf[8192]`）并设为全缓冲，提升性能。

#### 2. 简化版：`setbuf`
```c
void setbuf(FILE *stream, char *buf);
```
相当于 `setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);`。`buf` 必须至少有 `BUFSIZ` 个字节（通常是 8192）。

---

### 四、 核心深度拓展 (高阶面试/实战考点)

#### 拓展一：底层系统 I/O (`fd`) 与标准流 (`FILE *`) 的互相转换
这是连接 Linux 文件 I/O 和 C 标准 I/O 的桥梁。

1.  **从 `FILE *` 获取底层 `fd`：`fileno`**
    ```c
    int fileno(FILE *stream);
    ```
    *   *用途*：你用 `fopen` 打开了文件，但突然需要用底层的 `fsync` 强制落盘，或者需要用 `flock` 进行文件加锁。这时就需要先用 `fileno(fp)` 拿到文件描述符。
2.  **从 `fd` 包装出 `FILE *`：`fdopen`**
    ```c
    FILE *fdopen(int fd, const char *mode);
    ```
    *   *用途*：底层系统调用创建的东西没有对应的 C 库函数。比如：
        *   管道：`pipe()` 返回两个 `fd`，可以用 `fdopen` 将其包装成 `FILE *`，然后用 `fprintf` 写管道，用 `fgets` 读管道，极其方便。
        *   内存文件：`open("/dev/shm/test", ...)` 或 `memfd_create()` 返回 `fd`，用 `fdopen` 包装后可以用 `fscanf` 解析。
    *   *注意*：`fdopen` 不会截断文件，`mode` 必须与底层 `fd` 的打开权限兼容。`fclose` 关闭这个流时，底层 `fd` 也会被关闭。

#### 拓展二：流的定向 —— `fwide` (宽字符与多字节)
在 Linux 中，流可以是“字节定向”（处理普通的 `char`）或“宽定向”（处理 `wchar_t`，如中文字符）。
*   一个流在被第一次操作时，会根据操作的类型自动确定定向。
*   如果先用 `fgetc` 读了一个字节，流就变成了字节定向，之后就不能再用 `fgetwc` 读宽字符了（会报错）。
*   `int fwide(FILE *stream, int mode);` 可以强制设置流的定向，通常用于国际化程序的底层处理。

#### 拓展三：内存流 (`fmemopen` / `open_memstream`) —— 把内存当文件读写
Linux/GNU 提供了一组强大的扩展，允许你把一段内存当作流来操作，**不需要创建临时文件**。

1.  **`FILE *fmemopen(void *buf, size_t size, const char *mode);`**
    *   把 `buf` 这块内存关联到一个流。你可以用 `fprintf` 往内存里写格式化字符串，或者用 `fscanf` 从内存里解析数据。
    *   *应用场景*：替代 `sprintf/snprintf`，当你需要构造一个极其复杂的结构化文本，且代码里全都是 `fprintf` 逻辑时，直接把内存当文件写，避免了缓冲区溢出的心智负担。
2.  **`FILE *open_memstream(char **ptr, size_t *sizeloc);`**
    *   动态分配内存流。你只管往流里写，它会自动用 `malloc` 扩容。关闭流后，通过 `ptr` 拿到这块内存的指针。常用于底层日志引擎拼接字符串。

#### 拓展四：管道流 (`popen`) —— 执行 Shell 命令并交互
```c
FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);
```
*   *原理*：底层调用 `fork()` 和 `exec()` 执行 `command` 指定的 Shell 命令，并创建一个管道连接到它的标准输入或输出。
*   *应用场景*：C 程序需要获取 `ls -l` 的输出结果。可以直接 `FILE *fp = popen("ls -l", "r");` 然后用 `fgets` 逐行读取结果。这在写系统管理脚本时极为方便。
*   *注意*：会引发 Shell 注入风险，如果 `command` 包含外部输入，必须对输入进行严格转义。

#### 拓展五：流的刷新时机与程序崩溃
这是实际开发中最容易踩坑的地方：
*   如果程序调用了 `exit()` 正常退出：标准 C 库会自动执行 `fclose` 关闭所有打开的流，**缓冲区的数据会安全落盘**。
*   如果程序被信号杀死（如 `SIGKILL` 或发生段错误 `SIGSEGV`）：**进程瞬间死亡，缓冲区数据直接丢失**！C 库根本没机会执行 `fclose`。
*   *防范措施*：对于关键的日志输出，要么使用 `fflush(stdout)` 手动刷新，要么直接操作无缓冲的 `stderr`，或者用底层的文件 I/O (`write`) 结合 `O_SYNC`。

---

### 总结

流（`FILE *`）是 C 语言中最优雅的设计之一。它将杂乱的底层文件描述符包装成了具有状态、缓冲和统一接口的数据通道。

*   **常规操作**：用 `fseek` 控制位置，用 `feof/ferror` 检查状态，用 `setvbuf` 调优性能。
*   **高级桥接**：用 `fileno` 下探内核，用 `fdopen` 包装底层资源。
*   **黑科技**：用 `fmemopen` 操作内存，用 `popen` 调动系统命令。

掌握流操作的细节和缓冲机制，是写出高性能、高稳定性 Linux C/C++ 服务程序的必备基础。

