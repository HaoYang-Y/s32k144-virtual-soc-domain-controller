# =====================================================================
# SOC 端 (Ubuntu) — 域控制器应用 CMake 构建
# 对标 AUTOSAR Adaptive Platform 架构 + SPI 驱动
# =====================================================================
cmake_minimum_required(VERSION 3.20)
project(domain_controller_soc VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========== 源文件 ==========
# 注意：骨架文件 (已#注释) 待实现后取消注释
set(SOURCES
    # App 层 (已实现)
    apps/DomainController/src/main.cpp
    apps/DomainController/src/ConfigManager.cpp
    apps/DomainController/src/SignalFusion.cpp

    # Platform 层骨架 (待实现)
    #platform/ara/log/src/Logger.cpp
    #platform/ara/exec/src/ExecClient.cpp
    #platform/Spi/src/SpiDriver.cpp

    # Communication 层骨架 (待实现)
    #communication/ara/com/src/ServiceDiscovery.cpp
    #communication/ara/com/src/SomeIpBinding.cpp
    #communication/SpiGateway/src/SpiGateway.cpp

    # Diag 层骨架 (待实现)
    #diag/ara/diag/src/UdsServer.cpp
)

# ========== 可执行目标 ==========
add_executable(domain_controller_soc ${SOURCES})

# ========== 头文件路径 (AUTOSAR AP 模块) ==========
target_include_directories(domain_controller_soc PRIVATE
    # Platform 层
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/ara/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/ara/log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/ara/exec/include
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/Spi/include

    # Communication 层
    ${CMAKE_CURRENT_SOURCE_DIR}/communication/ara/com/include
    ${CMAKE_CURRENT_SOURCE_DIR}/communication/SpiGateway/include
    ${CMAKE_CURRENT_SOURCE_DIR}/communication/SpiGateway/config

    # Diag 层
    ${CMAKE_CURRENT_SOURCE_DIR}/diag/ara/diag/include

    # App 层
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/DomainController/include
)

# ========== 编译选项 ==========
target_compile_options(domain_controller_soc PRIVATE
    -Wall -Wextra -Wpedantic
)

# ========== 安装 ==========
install(TARGETS domain_controller_soc RUNTIME DESTINATION bin)

message(STATUS "=== SOC 端构建摘要 ===")
message(STATUS "  目标:     domain_controller_soc")
message(STATUS "  C++:      C++17")
message(STATUS "  源文件数: ${SOURCES_COUNT}")
message(STATUS "  未实现骨架: 6 个模块 (ara/log, ara/exec, Spi, com, SpiGateway, diag)")
