cmake_minimum_required(VERSION 3.20)
project(EPollHighPerfServer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 开启严格的编译警告，高并发项目强烈建议开启，提前发现潜在 Bug
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g")

# 查找pthread库
find_package(Threads REQUIRED)

# ==========================================
# 1. 构建 Server 可执行文件
# ==========================================
# 明确列出 Server 所需的源文件
set(SERVER_SOURCES
    server/src/main.c
    server/src/server.c
    server/src/hash_map.c
)

add_executable(Server ${SERVER_SOURCES})

# 添加 Server 的头文件搜索路径
target_link_directories(Server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/server/include
)

# 链接 pthread
target_link_libraries(Server PRIVATE
    Threads::Threads
)



# ==========================================
# 2. 构建 Client 可执行文件
# ==========================================
# Client 1
add_executable(Client client/client.c)
target_link_libraries(Client PRIVATE Threads::Threads)

# Client 2
add_executable(Client2 client/client2.c)
target_link_libraries(Client2 PRIVATE Threads::Threads)

