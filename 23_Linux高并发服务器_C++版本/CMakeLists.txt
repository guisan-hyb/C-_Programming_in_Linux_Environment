cmake_minimum_required(VERSION 3.10)

# 1. 定义项目名和使用的语言（C++）
project(event_server LANGUAGES CXX)

# 2. 指定 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 3. 查找 pthread（Threads）库
find_package(Threads REQUIRED)

# 4. 添加可执行文件，源文件改为 C++ 后缀
add_executable(event_server
    src/main.cpp
    src/ConfigMgr.cpp
    src/server.cpp
    src/io_thread.cpp 
    src/event_loop.cpp
    src/session.cpp
)

target_include_directories(event_server
    PRIVATE ${CMAKE_SOURCE_DIR}/include
)

# 5. 链接 pthread（Threads）库
target_link_libraries(event_server
    PRIVATE Threads::Threads
)


# ------------------------------------------------------------
#  自动拷贝根目录下的 .ini 到 build 目录
# ------------------------------------------------------------
# 查找项目根目录下所有 .ini 文件
file(GLOB CONFIG_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*.ini")

if(CONFIG_FILES)
    # 方式一：配置阶段拷贝（只执行一次，.ini 修改后需重新 cmake）
    # file(COPY ${CONFIG_FILES} DESTINATION ${CMAKE_BINARY_DIR})

    # 方式二：构建阶段拷贝（推荐，每次 make 都会检查并同步）
    add_custom_target(copy_configs ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CONFIG_FILES}
            ${CMAKE_BINARY_DIR}
        COMMENT "Copying .ini files to ${CMAKE_BINARY_DIR}"
        VERBATIM
    )

    # 如果希望 .ini 跟可执行文件放在同一目录（多配置生成器更准）
    # add_custom_command(TARGET io_server POST_BUILD
    #     COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #         ${CONFIG_FILES}
    #         $<TARGET_FILE_DIR:io_server>
    #     COMMENT "Copying .ini files to target directory"
    # )
else()
    message(STATUS "No .ini files found in ${CMAKE_SOURCE_DIR}")
endif()

