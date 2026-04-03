# ##############################################################################
# 项目: ServerPractice (高性能并发服务器框架)
# 功能: 核心网络抽象、事件驱动 Reactor 模型与数据库集成
# ##############################################################################

cmake_minimum_required(VERSION 3.10)

# 让 CMake 自动生成 compile_commands.json，利于 VSCode/Antigravity 等 IDE 解析
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# 1. 编译选项配置
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# -O3: 强力优化, -Wall: 开启所有警告, -g: 保留调试信息, -pthread: 启用多线程支持
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -g -pthread")

# 去除 __FILE__ 宏中的绝对路径前缀，保护隐私并缩减二进制体积
add_compile_options(-fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=)

project(ServerPractice)

# ------------------------------------------------------------------------------
# 2. 依赖库管理 (MySQL, Pthread)
# ------------------------------------------------------------------------------

# 使用 PkgConfig 自动查找 MySQL 客户端开发库
find_package(PkgConfig REQUIRED)
pkg_check_modules(MYSQL REQUIRED mysqlclient)
pkg_check_modules(HIREDIS REQUIRED hiredis)

# 包含路径配置
# SYSTEM 关键字可以抑制第三方库头文件产生的警告
include_directories(SYSTEM ${MYSQL_INCLUDE_DIRS} ${HIREDIS_INCLUDE_DIRS})
include_directories(include)

# 定义项目通用的第三方库列表 (避免在每个 target 下重复书写)
set(COMMON_LIBS 
    server_core 
    pthread 
    ${MYSQL_LIBRARIES}
    ${HIREDIS_LIBRARIES}
)

# ------------------------------------------------------------------------------
# 3. 核心库 (Static Library)
# ------------------------------------------------------------------------------

# 搜集 src 目录下的所有源文件 (排除 main.cpp)
file(GLOB SRC_FILES "src/*.cpp")
list(REMOVE_ITEM SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# 构建服务器核心静态库，避免重复编译
add_library(server_core STATIC ${SRC_FILES})

# ------------------------------------------------------------------------------
# 4. 可执行程序 (Executables)
# ------------------------------------------------------------------------------

# 主程序
add_executable(main src/main.cpp)

# 示例: 基础 Echo 服务
add_executable(EchoSever EchoSever.cpp)

# 示例: 基于线程池的 Echo 服务
add_executable(EchoSeverByThreadPool EchoSeverByThreadPool.cpp)

# 其他测试程序 (如果存在)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SocketTest.cpp")
    add_executable(SocketTest SocketTest.cpp)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/custom_deleter_test.cpp")
    add_executable(custom_deleter_test custom_deleter_test.cpp)
endif()

# ------------------------------------------------------------------------------
# 5. 链接管理
# ------------------------------------------------------------------------------

# 将通用库链接到所有 Target
target_link_libraries(main PRIVATE ${COMMON_LIBS})
target_link_libraries(EchoSever PRIVATE ${COMMON_LIBS})
target_link_libraries(EchoSeverByThreadPool PRIVATE ${COMMON_LIBS})

if(TARGET SocketTest)
    target_link_libraries(SocketTest PRIVATE ${COMMON_LIBS})
endif()

if(TARGET custom_deleter_test)
    target_link_libraries(custom_deleter_test PRIVATE ${COMMON_LIBS})
endif()

# ##############################################################################
# 配置完毕. 使用 cmake .. && make 构建
# ##############################################################################