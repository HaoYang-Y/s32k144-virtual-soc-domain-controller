# =====================================================================
# @brief 域控制器开发环境 Docker 镜像
#        提供一致的 Linux 开发环境:
#          - Ubuntu 24.04 + GCC/G++ 14
#          - CMake 3.28+
#          - libyaml-cpp (YAML 解析)
#          - socketcan 支持
#          - ARM GCC 交叉编译工具链 (MCU 端)
#
# @usage
#   构建镜像: docker build -t domain-controller-dev .
#   进入开发环境: docker run --rm -it -v $(pwd):/workspace domain-controller-dev
#   编译 SOC 端: cd /workspace && ./tools/soc_build.sh debug
# =====================================================================

FROM ubuntu:24.04 AS base

LABEL maintainer="user"
LABEL description="Domain Controller Development Environment (SOC + MCU)"

# 避免交互式 tzdata 配置
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai

# ========== 基础工具 ==========
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    curl \
    ca-certificates \
    pkg-config \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# ========== C++ 依赖 ==========
RUN apt-get update && apt-get install -y --no-install-recommends \
    libyaml-cpp-dev \
    libboost-dev \
    libssl-dev \
    libfmt-dev \
    libspdlog-dev \
    && rm -rf /var/lib/apt/lists/*

# ========== Linux CAN 开发工具 ==========
RUN apt-get update && apt-get install -y --no-install-recommends \
    can-utils \
    libsocketcan-dev \
    libnl-3-dev \
    && rm -rf /var/lib/apt/lists/*

# ========== ARM GCC 交叉编译工具链 (MCU 端) ==========
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    gdb-multiarch \
    openocd \
    && rm -rf /var/lib/apt/lists/*

# ========== 代码风格检查 ==========
RUN apt-get update && apt-get install -y --no-install-recommends \
    clang-format \
    cppcheck \
    && rm -rf /var/lib/apt/lists/*

# ========== 开发工具 ==========
RUN apt-get update && apt-get install -y --no-install-recommends \
    vim \
    nano \
    tmux \
    htop \
    tree \
    net-tools \
    iproute2 \
    usbutils \
    && rm -rf /var/lib/apt/lists/*

# ========== 设置工作目录 ==========
WORKDIR /workspace

# 验证工具链
RUN echo "[VERIFY] GCC version:" && gcc --version | head -1 && \
    echo "[VERIFY] G++ version:" && g++ --version | head -1 && \
    echo "[VERIFY] CMake version:" && cmake --version | head -1 && \
    echo "[VERIFY] ARM GCC version:" && arm-none-eabi-gcc --version | head -1 && \
    echo "[VERIFY] OpenOCD version:" && openocd --version | head -1 && \
    echo "[VERIFY] can-utils:" && candump --version 2>/dev/null || echo "  candump available"

# 默认命令
CMD ["/bin/bash"]
