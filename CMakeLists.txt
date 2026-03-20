# @brief 设置项目所需的最低 CMake 版本要求。
# @param VERSION 关键字，指明后续的参数是版本号。
# @param <min> (如 3.10) 指定所需的最低 CMake 版本号。如果当前编译环境的 CMake 版本低于此值，将报错并停止配置。
cmake_minimum_required(VERSION 3.10)

# @brief 定义项目名称及基本属性。
# @param <PROJECT-NAME> (如 ServerPractice) 项目的名称。调用此命令后，CMake 会隐式生成 PROJECT_NAME 等相关内置变量供后续使用。
project(ServerPractice)

# @brief 将指定的 CMake 变量设置为给定的值。
# @param <variable> (如 CMAKE_CXX_STANDARD) 要设置的变量名称。
# @param <value>... (如 17 或 True) 要赋给该变量的值（可以是多个值，以空格分隔）。
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED True)

# @brief 将指定的目录添加到编译器的头文件搜索路径中。
# @param [AFTER|BEFORE] (可选) 控制目录是追加到现有搜索路径列表的末尾还是插入到开头。
# @param [SYSTEM] (可选) 将这些目录标记为系统包含目录，通常编译器会抑制在系统目录头文件中发现的警告。
# @param <dir>... (如 include) 要添加的目录路径列表。
include_directories(include)

# @brief 搜索匹配指定模式的文件路径，并将结果保存到变量中。
# @param GLOB 关键字，表示基于通配符进行文件匹配（注：匹配得到的是绝对路径列表）。
# @param <variable> (如 SRC_FILES) 用于保存匹配结果的变量名。
# @param <globbing-expressions>... (如 "src/*.cpp") 要匹配的路径通配符。
file(GLOB SRC_FILES "src/*.cpp")

# @brief 对 CMake 列表变量执行特定操作（如追加、查找、移除等）。
# @param REMOVE_ITEM 关键字，表示从列表中移除指定的值。
# @param <list> (如 SRC_FILES) 要进行操作的列表变量名。
# @param <items>... (如 "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp") 要移除的具体项。因 GLOB 返回绝对路径，此处需拼接 CMAKE_CURRENT_SOURCE_DIR 以精确匹配并剔除 main.cpp。
list(REMOVE_ITEM SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# @brief 将公共源文件编译成一个静态库，避免同一个文件被重复编译多次
# @param <name> (如 server_core) 库的名称
# @param [STATIC|SHARED|MODULE] 指定创建静态库、动态库还是模块库
add_library(server_core STATIC ${SRC_FILES})

# @brief 使用指定的源文件来构建目标可执行文件。
# @param <name> (如 main) 构建出的可执行文件的名称，此名称在整个 CMake 项目中必须全局唯一。
# @param [WIN32] (可选) 用于 Windows 平台，指示构建一个基于 GUI 的 Win32 应用程序（无控制台窗口）。
# @param [MACOSX_BUNDLE] (可选) 用于 macOS 平台，指示构建一个 App Bundle（GUI 应用程序）。
# @param [EXCLUDE_FROM_ALL] (可选) 如果指定，该目标将不会包含在默认的构建目标（如 make all）中，必须显式指定构建。
# @param <source>... (如 src/main.cpp) 用于编译生成该可执行文件所需的源文件列表。
add_executable(main src/main.cpp)

add_executable(EchoSever EchoSever.cpp)

add_executable(EchoSeverByThreadPool EchoSeverByThreadPool.cpp)

# @brief 将给定的库或标志链接到指定目标上。
# @param <target> (如 main) 必须是由 add_executable 或 add_library 创建的目标名称。
# @param <PRIVATE|PUBLIC|INTERFACE> 声明依赖的传播范围。PRIVATE 表示仅该目标自身链接该库。
# @param <item>... (如 pthread) 要链接的系统库名称（Linux 环境下使用 std::thread 等需链接 pthread）。
target_link_libraries(main PRIVATE server_core pthread)
target_link_libraries(EchoSever PRIVATE server_core pthread)
target_link_libraries(EchoSeverByThreadPool PRIVATE server_core pthread)