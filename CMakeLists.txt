cmake_minimum_required(VERSION 3.20)
project(domain_controller VERSION 1.0.0 LANGUAGES C CXX)

# 顶层 CMakeLists.txt — 可选的聚合构建入口
# 当前 SOC 端和 MCU 端各自独立构建:
#   SOC: cd soc && mkdir build && cd build && cmake ..
#   MCU: cd mcu && make
#
# 如需统一构建, 取消下方注释:
# add_subdirectory(soc)
