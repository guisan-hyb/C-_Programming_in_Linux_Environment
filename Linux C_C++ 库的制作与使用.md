在 Linux C/C++ 开发中，库（Library）是代码复用的核心机制。简单来说，库就是一组编译好的目标文件的集合，供其他程序链接使用。

下面详细总结 Linux 下静态库和动态库的制作与使用，并进行相关知识点的拓展。

---

### 一、 核心概念对比

在开始制作之前，先理解静态库和动态库（共享库）的本质区别：

| 特性 | 静态库 | 动态库 / 共享库 |
| :--- | :--- | :--- |
| **Linux 后缀** | `.a` (Archive) | `.so` (Shared Object) |
| **Windows 后缀**| `.lib` | `.dll` |
| **链接时机** | **编译期**链接，库的代码被拷贝到可执行文件中 | **运行期**链接，可执行文件只记录库的引用地址 |
| **可执行文件大小** | 较大（包含库代码） | 较小（只包含引用） |
| **内存占用** | 多个程序使用同一库，内存中有多份副本 | 内存中只有一份副本，多个程序共享 |
| **更新与部署** | 库更新需重新编译整个程序 | 直接替换 `.so` 文件即可生效（需保持接口兼容） |
| **独立性** | 独立运行，不依赖外部库文件 | 运行时必须能找到对应的 `.so` 文件 |

---

### 二、 静态库的制作与使用

**场景设定：** 我们有头文件 `hello.h`、源文件 `hello.c`，以及调用它的 `test.c`。

#### 1. 制作静态库 (`.a`)

**步骤 1：将源文件编译成目标文件**
使用 `-c` 选项只编译不链接，生成 `.o` 文件。
```bash
gcc -c hello.c -o hello.o
# 或者 C++: g++ -c hello.cpp -o hello.o
```

**步骤 2：使用 `ar` 工具打包**
将 `.o` 文件打包成静态库。命名规范必须以 `lib` 开头，`.a` 结尾。
```bash
ar rcs libhello.a hello.o
```
*   `r`：插入/替换目标文件到库中。
*   `c`：创建库。
*   `s`：创建索引（加快链接速度，相当于 `ranlib`）。

#### 2. 使用静态库

假设 `test.c` 中调用了 `hello()` 函数：
```bash
# -L. : 指定库搜索路径为当前目录
# -lhello : 链接名为 hello 的库 (自动寻找 libhello.a)
gcc test.c -L. -lhello -o test
```
执行：`./test`（因为静态库代码已打包进可执行文件，直接运行即可）。

---

### 三、 动态库的制作与使用

动态库是 Linux 系统中最重要的库形式，系统自带的库（如 `libc.so`）基本都是动态库。

#### 1. 制作动态库 (`.so`)

**步骤 1：编译成位置无关代码**
必须加上 `-fPIC`（Position Independent Code）选项。这使得生成的代码可以在内存的任意位置加载运行，这是动态库共享的前提。
```bash
gcc -fPIC -c hello.c -o hello.o
```

**步骤 2：打包成动态库**
使用 `-shared` 选项生成动态库。命名规范以 `lib` 开头，`.so` 结尾。
```bash
gcc -shared hello.o -o libhello.so
```
*(也可以一步到位：`gcc -fPIC -shared hello.c -o libhello.so`)*

#### 2. 使用动态库

编译时的命令与静态库完全一样：
```bash
gcc test.c -L. -lhello -o test
```

#### 3. 运行动态库（重点与痛点）

编译成功后，如果直接执行 `./test`，通常会报错：
`./test: error while loading shared libraries: libhello.so: cannot open shared object file: No such file or directory`

**原因：** 程序运行时，系统默认去 `/lib`、`/usr/lib` 等标准目录寻找 `.so` 文件，找不到就会报错。

**解决方法（3种）：**

1.  **临时配置环境变量（当前终端有效）：**
    ```bash
    export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
    ./test
    ```
2.  **将库拷贝到系统标准目录（不推荐自己写的库这么做）：**
    ```bash
    sudo cp libhello.so /usr/lib/
    sudo ldconfig  # 刷新动态链接库的缓存
    ./test
    ```
3.  **使用 `Wl,-rpath` 在编译时指定运行时搜索路径（最推荐）：**
    在编译可执行文件时，将当前目录（或你指定的绝对路径）写进可执行文件中，告诉它运行时去哪里找库。
    ```bash
    gcc test.c -L. -lhello -o test -Wl,-rpath=.
    ./test
    ```

---

### 四、 进阶拓展知识

#### 1. GCC 的 `-l` 参数寻址规则
当你使用 `-lhello` 时，链接器（`ld`）是如何寻找库的呢？
*   它会在 `-L` 指定的路径和系统默认路径下寻找。
*   寻找的文件名是 `libhello.so` 和 `libhello.a`。
*   **如果同名同时存在 `.so` 和 `.a`，默认优先链接动态库。**
*   如果你想强制使用静态库，可以使用 `-static` 参数（整个程序全部静态链接），或者更精细的 `-Wl,-Bstatic -lhello -Wl,-Bdynamic`。

#### 2. 头文件搜索路径 (`-I`)
如果 `hello.h` 不在当前目录，而在 `./include` 目录下，编译 `test.c` 时会找不到头文件。使用 `-I` 参数指定头文件路径：
```bash
gcc test.c -I./include -L./lib -lhello -o test
```
*记忆口诀：大写 I (Include) 找头文件，大写 L (Location) 找库文件。*

#### 3. 常用的库分析工具
Linux 提供了强大的工具来检查库和可执行文件的状态：

*   **`nm`**: 列出目标文件、静态库或动态库中的符号（函数、全局变量等）。
    *   `nm libhello.a` (T 表示代码段，通常代表函数定义)
    *   `nm test` (U 表示未定义的符号，说明需要从外部库链接)
*   **`ldd`**: 查看可执行文件依赖的动态库。
    *   `ldd test` (会显示 `libhello.so => ./libhello.so` 等信息)
*   **`ar`**: 除了打包静态库，还可以查看静态库内容。
    *   `ar -t libhello.a` (列出包中包含的 `.o` 文件)
    *   `ar -x libhello.a` (解压出 `.o` 文件)
*   **`readelf`**: 查看 ELF 格式文件的详细信息（如动态库依赖、版本等）。
    *   `readelf -d libhello.so` (查看动态库的动态段信息)

#### 4. 动态库的显式调用
除了上面提到的在编译时隐式链接动态库，C/C++ 还可以在代码运行时**动态加载**库（类似 Windows 的 `LoadLibrary`）。这需要使用 `<dlfcn.h>` 库。

**示例代码：**
```c
#include <stdio.h>
#include <dlfcn.h>

int main() {
    // 1. 动态加载 .so 文件
    void* handle = dlopen("./libhello.so", RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Error: %s\n", dlerror());
        return 1;
    }

    // 2. 获取库中的函数指针
    // 注意：C++ 需要考虑 name mangling，通常配合 extern "C" 使用
    typedef void (*HelloFunc)();
    HelloFunc hello = (HelloFunc)dlsym(handle, "hello");
    
    if (!hello) {
        fprintf(stderr, "Error: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }

    // 3. 调用函数
    hello();

    // 4. 卸载库
    dlclose(handle);
    return 0;
}
```
**编译：** 必须链接 `dl` 库（`libdl.so`）。
```bash
gcc dynamic_call.c -o dynamic_call -ldl
```
*显式调用的优点：可以实现插件化架构，按需加载，不需要在编译时知道库的存在。*

