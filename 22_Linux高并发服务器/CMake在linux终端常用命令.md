你的直觉非常准确！在现代 CMake 工作流中，**90% 的情况下你只需要记住 `cmake --preset` 和 `cmake --build` 这两条命令**。

过去那种 `mkdir build && cd build && cmake .. && make` 的繁琐步骤已经被现代 CMake 彻底简化了。下面为你梳理 Linux 终端下最常用的 CMake 命令清单及其实用场景：

---

### 一、 黄金搭档（日常开发 99% 用这两个）

#### 1. 配置阶段：`cmake --preset`
*   **作用**：读取 `CMakePresets.json`，生成构建系统文件（如 `Makefile` 或 `build.ninja`）。
*   **常用形式**：
    ```bash
    cmake --preset linux-debug
    ```
*   **特点**：只要执行一次，后续修改了 `CMakeLists.txt`，在执行 `--build` 时 CMake 会自动重新配置。

#### 2. 构建阶段：`cmake --build`
*   **作用**：执行编译和链接，生成可执行文件或库。
*   **常用形式**：
    ```bash
    # 基础用法（编译所有目标）
    cmake --build build

    # 只编译指定的目标（比如你只想测客户端）
    cmake --build build --target Client

    # 强制多核并行编译（大幅提升编译速度，8代表8核）
    cmake --build build -j 8
    ```

---

### 二、 清理与重置（遇到玄学问题时使用）

#### 3. 清理编译产生的中间文件
*   **作用**：删除 `.o` 等目标文件，但保留 `CMakeCache.txt` 和 Makefile。
*   **命令**：
    ```bash
    cmake --build build --target clean
    ```

#### 4. 彻底重置（删除缓存重新配置）
*   **作用**：当你修改了 `CMakePresets.json` 中的路径、编译器选项，或者遇到莫名其妙的编译错误时使用。
*   **命令**：直接删除整个 build 文件夹，然后重新配置。
    ```bash
    rm -rf build/
    cmake --preset linux-debug
    cmake --build build
    ```

---

### 三、 安装与部署（发布时使用）

#### 5. 安装目标文件
*   **作用**：将编译好的可执行文件、头文件、库文件安装到系统目录（如 `/usr/local/bin`）。
*   **命令**：
    ```bash
    # 需要在 CMakeLists.txt 中预先写好 install() 指令
    sudo cmake --install build
    ```

---

### 四、 终端可视化工具（排查问题专用）

#### 6. 终端图形化配置工具 `ccmake`
*   **作用**：在终端中提供一个简易的 UI 界面，让你查看和修改 CMake 的缓存变量（如开关某个选项）。
*   **命令**：
    ```bash
    # 需要单独安装：sudo apt install cmake-curses-gui
    ccmake build
    ```
    *(进入后按 `c` 配置，`g` 生成并退出，`q` 不保存退出)*

---

### 总结流程图

你在终端的日常循环基本就是下面这个套路：

```text
1. 改 CMakeLists.txt / CMakePresets.json
       ↓
2. cmake --preset linux-debug   (如果提示没变化可跳过)
       ↓
3. cmake --build build -j 8     (疯狂编译)
       ↓
4. ./build/YourApp               (运行测试)
       ↓
   出错了？ → 回到第 1 步
   玄学错误？ → rm -rf build 回到第 2 步
```

所以，你把握住 `--preset` 和 `--build`，就已经掌握了现代 CMake 命令行的精髓！

