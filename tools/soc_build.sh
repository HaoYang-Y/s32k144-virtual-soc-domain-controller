#!/bin/bash
# =====================================================================
# @brief SOC 端构建和运行脚本
#        构建 Linux 端域控制器应用，支持 Debug/Release 模式
#
# @usage ./tools/soc_build.sh [debug|release|run|clean]
# =====================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"
BUILD_DIR="${PROJECT_DIR}/soc/build"

# 默认模式
BUILD_TYPE="${1:-debug}"

case "${BUILD_TYPE}" in
    debug)
        echo "[BUILD] Debug 模式编译 SOC 端..."
        cmake -S "${PROJECT_DIR}/soc" -B "${BUILD_DIR}" \
              -DCMAKE_BUILD_TYPE=Debug \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        cmake --build "${BUILD_DIR}" -j"$(nproc)"
        echo "[BUILD] 编译完成: ${BUILD_DIR}/domain_controller_soc"
        ;;

    release)
        echo "[BUILD] Release 模式编译 SOC 端..."
        cmake -S "${PROJECT_DIR}/soc" -B "${BUILD_DIR}" \
              -DCMAKE_BUILD_TYPE=Release \
              -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        cmake --build "${BUILD_DIR}" -j"$(nproc)"
        echo "[BUILD] 编译完成: ${BUILD_DIR}/domain_controller_soc"
        ;;

    run)
        if [ ! -f "${BUILD_DIR}/domain_controller_soc" ]; then
            echo "[ERROR] 请先编译: $0 debug|release"
            exit 1
        fi
        echo "[RUN] 启动域控制器 SOC 端..."
        exec "${BUILD_DIR}/domain_controller_soc" \
             -c "${PROJECT_DIR}/config/domain_config.yaml"
        ;;

    clean)
        echo "[CLEAN] 清理构建产物..."
        rm -rf "${BUILD_DIR}"
        echo "[CLEAN] 完成"
        ;;

    *)
        echo "用法: $0 [debug|release|run|clean]"
        echo "  debug   - Debug 模式编译 (默认)"
        echo "  release - Release 模式编译"
        echo "  run     - 运行已编译的 SOC 应用"
        echo "  clean   - 清理构建"
        exit 1
        ;;
esac
