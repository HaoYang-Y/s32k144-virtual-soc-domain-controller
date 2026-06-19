/**
 * @file    stubs.c
 * @brief   编译链接桩函数集
 *
 * 提供 SDK 所需但未链接的外部符号的轻量实现。
 * 这些函数在当前非 DMA 场景下不会被实际调用，仅用于满足链接。
 */

#include <stdint.h>
#include <stddef.h>

/* =====================================================================
 * 标准库桩函数
 * ===================================================================== */

/**
 * @brief   内存填充 stub
 *
 * 简单的 memset 实现，用于满足链接需求。
 * 编译器使用结构体初始化时可能生成 __aeabi_memset 调用。
 *
 * @param[out] s     目标内存地址
 * @param[in]  c     填充值
 * @param[in]  n     填充字节数
 * @return           目标内存地址
 */
void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    size_t i;
    for (i = 0U; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

/*
 * ARM EABI alias: __aeabi_memset 与标准 memset 签名完全相同，
 * 直接调用 memset 即可。
 */
void __attribute__((alias("memset"))) __aeabi_memset(void *s, int c, size_t n);

/**
 * @brief   内存复制 stub (标准 C)
 *
 * 简单的 memcpy 实现，不支持源/目标重叠（重叠场景应由 memmove 处理）。
 * 编译器为大型结构体赋值生成 memcpy 调用。
 *
 * @param[out] dest   目标地址
 * @param[in]  src    源地址
 * @param[in]  n      复制字节数
 * @return            目标地址
 */
void *memcpy(void *dest, const void *src, size_t n)
{
    const unsigned char *s = (const unsigned char *)src;
    unsigned char       *d = (unsigned char *)dest;
    size_t i;
    for (i = 0U; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

/** ARM EABI 版本的 memcpy */
void __attribute__((alias("memcpy"))) __aeabi_memcpy(void *dest, const void *src,
                                                      size_t n);

/**
 * @brief   内存移动 stub (标准 C)
 *
 * 支持源/目标重叠的 memmove 实现，方向自适应。
 *
 * @param[out] dest   目标地址
 * @param[in]  src    源地址
 * @param[in]  n      复制字节数
 * @return            目标地址
 */
void *memmove(void *dest, const void *src, size_t n)
{
    const unsigned char *s = (const unsigned char *)src;
    unsigned char       *d = (unsigned char *)dest;
    size_t i;
    if (d < s) {
        /* 正向复制 */
        for (i = 0U; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        /* 反向复制，避免重叠覆盖 */
        for (i = n; i > 0U; i--) {
            d[i - 1U] = s[i - 1U];
        }
    }
    /* d == s 时无需任何操作 */
    return dest;
}

/** ARM EABI 版本的 memmove */
void __attribute__((alias("memmove"))) __aeabi_memmove(void *dest, const void *src,
                                                        size_t n);

/**
 * @brief   内存清零 stub (ARM EABI)
 *
 * __aeabi_memclr 是 ARM EABI 标准的清零函数，等价于 memset(ptr, 0, n)。
 * 编译器为大型结构体零初始化生成此调用。
 *
 * @param[out] dest   目标地址
 * @param[in]  n      清零字节数
 */
void __aeabi_memclr(void *dest, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    size_t i;
    for (i = 0U; i < n; i++) {
        d[i] = 0U;
    }
}

/**
 * @brief   4 字节对齐清零 stub (ARM EABI)
 *
 * __aeabi_memclr4 是 __aeabi_memclr 的 4 字节对齐优化版本，
 * 编译器在确认目标地址 4 字节对齐时可能生成此调用。
 *
 * @param[out] dest   目标地址 (必须 4 字节对齐)
 * @param[in]  n      清零字节数
 */
void __aeabi_memclr4(void *dest, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    size_t i;
    for (i = 0U; i < n; i++) {
        d[i] = 0U;
    }
}

/**
 * @brief   8 字节对齐清零 stub (ARM EABI)
 *
 * __aeabi_memclr8 是 __aeabi_memclr 的 8 字节对齐优化版本。
 *
 * @param[out] dest   目标地址 (必须 8 字节对齐)
 * @param[in]  n      清零字节数
 */
void __aeabi_memclr8(void *dest, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    size_t i;
    for (i = 0U; i < n; i++) {
        d[i] = 0U;
    }
}

/* =====================================================================
 * EDMA stub — 非 DMA 模式下提供空实现，满足链接
 * ===================================================================== */

/** @brief EDMA 状态枚举 */
typedef enum {
    EDMA_CHN_NORMAL = 0
} edma_chn_status_t;

/**
 * @brief   EDMA 停止通道 stub
 */
int EDMA_DRV_StopChannel(uint8_t channel)
{
    (void)channel;
    return 0;
}

/**
 * @brief   EDMA 配置多块传输 stub
 */
int EDMA_DRV_ConfigMultiBlockTransfer(uint8_t channel, void *config)
{
    (void)channel;
    (void)config;
    return 0;
}

/**
 * @brief   EDMA 安装回调 stub
 */
int EDMA_DRV_InstallCallback(uint8_t channel, void *callback, void *param)
{
    (void)channel;
    (void)callback;
    (void)param;
    return 0;
}

/**
 * @brief   EDMA 启动通道 stub
 */
int EDMA_DRV_StartChannel(uint8_t channel)
{
    (void)channel;
    return 0;
}

/**
 * @brief   EDMA 设置源地址 stub
 */
int EDMA_DRV_SetSrcAddr(uint8_t channel, uint32_t addr)
{
    (void)channel;
    (void)addr;
    return 0;
}

/**
 * @brief   EDMA 设置主循环计数 stub
 */
int EDMA_DRV_SetMajorLoopIterationCount(uint8_t channel, uint32_t count)
{
    (void)channel;
    (void)count;
    return 0;
}

/* =====================================================================
 * libc 补充 stub (newlib-nano 精简版可能缺少的函数)
 * ===================================================================== */

/**
 * @brief   strlen stub
 */
size_t strlen(const char *s)
{
    size_t n = 0U;
    while (*s++) n++;
    return n;
}
