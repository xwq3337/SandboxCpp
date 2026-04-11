cmake_minimum_required(VERSION 3.15)
project(CodeRunner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
add_compile_options(-Wall -Wextra -O2)

# 查找依赖
find_package(Threads REQUIRED)

# httplib
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)
FetchContent_MakeAvailable(httplib)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 源文件
set(SOURCES
    src/main.cpp
    src/data_structures.cpp
    src/seccomp_filter.cpp
    src/namespace_isolation.cpp
    src/cgroup_manager.cpp
    src/code_executor.cpp
    src/web_server.cpp
    src/stats_collector.cpp
    src/compile_cache.cpp
)

# 可执行文件
add_executable(code_runner ${SOURCES})

# 链接库
target_link_libraries(code_runner
    PRIVATE
    httplib::httplib
    Threads::Threads
    seccomp
)

# Google Test
enable_testing()
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# 防止在 Windows MinGW 上覆盖父项目的编译器/链接器设置
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 测试
add_subdirectory(test)

# 安装
install(TARGETS code_runner DESTINATION bin)
