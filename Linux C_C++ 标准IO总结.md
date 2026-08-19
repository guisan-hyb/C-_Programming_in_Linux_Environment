以下是对 Linux 环境下 C/C++ 标准I/O的详细总结，内容涵盖核心概念、流的打开与关闭、以及输入输出操作。

---

### 一、 标准I/O 概述

**1. 什么是标准I/O？**
标准I/O是C标准库（`<stdio.h>` / `<cstdio>`）提供的一套针对文件I/O（系统调用如 `open/read/write`）的**封装接口**。它在用户态维护了**缓冲区**，旨在减少系统调用的次数，从而提高I/O效率。

**2. 核心概念：流**
在标准I/O中，所有操作都围绕**流**展开。流由 `FILE` 结构体表示（`FILE *`），该结构体包含了：
*   文件描述符（底层系统I/O的句柄）
*   缓冲区的指针及大小
*   当前读写位置
*   错误和文件结束标志

**3. 三大预定义流**
程序启动时，默认打开三个流：
*   `stdin`：标准输入（文件描述符为 0）
*   `stdout`：标准输出（文件描述符为 1）
*   `stderr`：标准错误（文件描述符为 2）

**4. 缓冲机制（重点）**
*   **全缓冲**：填满缓冲区后才进行实际I/O操作。通常用于磁盘文件。
*   **行缓冲**：遇到换行符 `\n` 时刷新缓冲区。通常用于终端设备（如 `stdout`）。
*   **无缓冲**：数据立即写入目标。通常用于标准错误（如 `stderr`），确保错误信息及时输出。
*   *注意*：标准输入和输出在重定向到文件时，缓冲模式会由行缓冲自动变为全缓冲。

---

### 二、 标准IO流的打开与关闭

#### 1. 打开流：`fopen`
```c
FILE *fopen(const char *pathname, const char *mode);
```
*   **pathname**：文件路径。
*   **mode**：打开模式，决定了读写权限和文件截断方式。

| 模式 | 说明 | 文件已'存在时的动作 | 文件不'存在时的动作 |
| :--- | :--- | :--- | :--- |
| **r** | 只读 | 从头读 | **出错** |
| **w** | 只写 | **截断为0** (清空) | 创建 |
| **a** | 追加写 | 在末尾写 | 创建 |
| **r+** | 读写 | 从头读写 | **出错** |
| **w+** | 读写 | **截断为0** (清空) | 创建 |
| **a+** | 读+追加写 | 读从开头，写在末尾 | 创建 |

*   **二进制模式**：在 Linux 中，文本文件和二进制文件没有区别，`b` 标志（如 `rb`, `wb`）会被忽略，但为了跨平台兼容性，操作二进制文件时建议加上 `b`。
*   **返回值**：成功返回 `FILE *` 指针，失败返回 `NULL` 并设置 `errno`。

#### 2. 关闭流：`fclose`
```c
int fclose(FILE *stream);
```
*   **功能**：刷新缓冲区（将未写的数据写入文件，丢弃未读的缓冲数据），释放内核资源，释放 `FILE` 结构体占用的内存。
*   **返回值**：成功返回 `0`，失败返回 `EOF`（通常, 磁盘满或硬件错误时关闭会失败）。
*   **重要原则**：**打开的流必须关闭**，否则会导致内存泄漏和文件描述符耗尽。

#### 3. 其他打开函数 (补充)
*   `freopen(const char *pathname, const char *mode, FILE *stream)`：重定向指定的流（常用于将 `stdout` 重定向到日志文件）。

---

### 三、 标准IO流的输入与输出

标准I/O按数据处理粒度分为三类：字符级、行级、块级；以及格式化输入输出。

#### 1. 字符级 I/O (一次一个字符)

**输入 (读取)：**
```c
int fgetc(FILE *stream);
int getc(FILE *stream);   // 宏实现，速度快
int getchar(void);        // 等同于 getc(stdin)
```
*   **返回值**：成功返回读取的字符（转为 `unsigned char` 再提升为 `int`），到达文件尾或出错返回 `EOF`。

**输出 (写入)：**
```c
int fputc(int c, FILE *stream);
int putc(int c, FILE *stream);   // 宏实现
int putchar(int c);              // 等同于 putc(c, stdout)
```
*   **返回值**：成功返回输出的字符 `c`，出错返回 `EOF`。

#### 2. 行级 I/O (一次一行字符串)

**输入 (读取)：**
```c
char *fgets(char *s, int size, FILE *stream);
```
*   **功能**：最多读取 `size - 1` 个字符，遇到 `\n` 或 `EOF` 提前停止，并在末尾自动追加 `\0`。
*   **返回值**：成功返回 `s`，到达文件尾或出错返回 `NULL`。
*   **警告**：**绝对不要使用 `gets()`**，它不检查缓冲区长度，极易导致栈溢出漏洞。

**输出 (写入)：**
```c
int fputs(const char *s, FILE *stream);
int puts(const char *s); // 输出到 stdout
```
*   **区别**：`fputs` 将字符串 `s` 写入流，**不自动追加换行符**；`puts` 将字符串写入标准输出，**自动在末尾追加换行符**。

#### 3. 块级/二进制 I/O (直接内存读写)

常用于读写结构体、数组等二进制数据。
```c
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
```
*   **参数**：
    *   `ptr`：数据内存缓冲区指针。
    *   `size`：单个数据项的大小（如 `sizeof(int)` 或 `sizeof(struct Student)`）。
    *   `nmemb`：数据项的个数。
*   **返回值**：**成功读取/写入的数据项个数**（不是字节数！）。若返回值小于 `nmemb`，说明到了文件尾或出错。

#### 4. 格式化 I/O (最常用)

**输入 (读取并解析)：**
```c
int scanf(const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);
int sscanf(const char *str, const char *format, ...); // 从字符串读取
```
*   **返回值**：成功匹配并赋值的输入项个数。到达文件尾返回 `EOF`。

**输出 (格式化并写入)：**
```c
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);       // 写入字符串 (有溢出风险)
int snprintf(char *str, size_t size, const char *format, ...); // 安全版本，限制长度
```
*   **返回值**：成功写入的字符数（不包括末尾的 `\0`）。

---

### 四、 错误处理与流定位 (核心辅助操作)

#### 1. 区分 EOF 和 错误
当 `fgetc` 等函数返回 `EOF` 或 `fread` 读取不足时，可能是到了文件尾，也可能是出错了。必须使用以下函数判断：
```c
int feof(FILE *stream);   // 若到了文件尾返回非0
int ferror(FILE *stream); // 若发生错误返回非0
void clearerr(FILE *stream); // 清除错误和EOF标志
```
*   **注意**：必须**先读一次**导致状态改变后，再调用 `feof`，不能用它来“预测”文件是否结束。

#### 2. 刷新缓冲区
```c
int fflush(FILE *stream);
```
*   若 `stream` 是输出流，将用户态缓冲区数据强制刷入内核。
*   若传入 `NULL`，刷新所有打开的输出流。

#### 3. 定位流 (移动读写指针)
```c
long ftell(FILE *stream); // 获取当前偏移量
int fseek(FILE *stream, long offset, int whence); // 设置偏移量 (whence: SEEK_SET/SEEK_CUR/SEEK_END)
void rewind(FILE *stream); // 重置到文件开头，并清除错误标志
```
*   *大文件支持*：在32/64位系统中，操作大于2GB的文件需使用 `fseeko`/`ftello` (off_t) 或 `fpos_t` 相关接口。

---

### 五、 C++ 中的注意事项

虽然 C++ 有自己的流库 (`<iostream>`, `<fstream>`)，但很多 C++ 项目依然会使用 C 标准I/O（出于性能或历史原因）：
1.  **同步机制**：默认情况下，C++ 的 `std::cin/cout` 与 C 的 `stdin/stdout` 是同步的（`std::ios::sync_with_stdio(true)`），这意味着你可以混用 `printf` 和 `std::cout`，且顺序不会乱，但**性能会大幅下降**。
2.  **解绑提速**：在算法竞赛等追求极致性能的场景，常调用 `std::ios::sync_with_stdio(false); std::cin.tie(nullptr);` 来解除绑定，此时**绝对不能**再混用 C 和 C++ 的 I/O 接口，否则会导致缓冲区混乱，输出无序。

### 总结建议
1.  **优先使用标准I/O**：日常读写文本、配置文件等，使用 `fopen/fclose/fread/fwrite` 比系统调用更高效、更方便。
2.  **警惕缓冲区陷阱**：输出到终端时默认行缓冲（看到 `\n` 就刷新），输出到文件时全缓冲。如果程序 Crash，全缓冲的数据可能还在内存没落盘，导致数据丢失。关键日志记得 `fflush` 或用 `stderr`（无缓冲）。
3.  **二进制读写用 `fread/fwrite`**，文本读写用 `fgets/fputs`，尽量避免逐字符操作大文件（系统调用开销虽小，但函数调用开销仍存在）。
4.  **安全第一**：拒绝 `gets`，慎用 `sprintf`，拥抱 `fgets` 和 `snprintf`。

