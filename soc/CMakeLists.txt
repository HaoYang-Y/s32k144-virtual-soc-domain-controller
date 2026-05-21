# =====================================================================
# SOC 端 (Ubuntu) — 域控制器应用 CMake 构建 (TODO 骨架)
# @brief 编译 Linux 端域控制器可执行程序
#
# TODO：看书学习并实现各模块后，添加对应源文件和依赖：
#       1. config_manager — 需要 yaml-cpp 库
#       2. can_manager    — 需要 Linux SocketCAN (AF_CAN)
#       3. signal_manager — 纯 C++ 无需外部依赖
#       4. http_server    — 需要 cpp-httplib 库
#       5. someip_server  — 需要 SOME/IP 协议栈
#       6. uds_server     — 需要 UDS 协议封装
#
# 当前：仅编译骨架 main.cpp 占位
# =====================================================================
cmake_minimum_required(VERSION 3.20)
project(domain_controller_soc VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========== 可执行目标 (骨架) ==========
add_executable(domain_controller_soc
    src/main.cpp
)

# ========== 基础编译选项 ==========
target_compile_options(domain_controller_soc PRIVATE
    -Wall -Wextra -Wpedantic
)

# ========== 安装 ==========
install(TARGETS domain_controller_soc RUNTIME DESTINATION bin)

message(STATUS "=== SOC 端构建摘要 (骨架模式) ===")
message(STATUS "  目标:     domain_controller_soc")
message(STATUS "  C++:      C++17")
message(STATUS "  状态:     骨架 — 实现后需添加依赖和源文件")
