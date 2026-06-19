/**
 * @file    Log.h
 * @brief   分级日志模块 — 环形缓冲区 + UART 输出 + DWT 时间戳
 *
 * @note    时间戳来源: ARM DWT 硬件周期计数器 (Cortex-M4)，1ms = 48000 cycles @ 48MHz
 * @note    输出: 通过注册的 output function (通常指向 Uart_SendString)
 *
 * 用法:
 *   Log_Init();                                  // 初始化环形缓冲 + 启用 DWT 周期计数
 *   Log_SetOutput(Uart_SendString);              // 绑定 UART 输出
 *   LOG_E("CanIf", "TX_BUSY PduId=0x%04X", id);  // 错误
 *   LOG_W("App",   "retry %d", count);            // 警告
 *   LOG_I("main",  "Boot OK, %d modules", n);     // 信息
 *   LOG_D("Can",   "MB0 status=0x%02X", s);       // 调试
 */

#ifndef LOG_H
#define LOG_H

#include "Log_Cfg.h"
#include "Std_Types.h"
#include "status.h"
#include <stdarg.h>
#include <stdbool.h>

/* ===================================================================
 *  类型定义
 * =================================================================== */

/** @brief 日志输出回调函数原型 (如 Uart_SendString) */
typedef status_t (*Log_OutputFn)(const char *str);

/* ===================================================================
 *  API 函数
 * =================================================================== */

/**
 * @brief 初始化日志模块
 *        - 清空环形缓冲区
 *        - 启用 ARM DWT 周期计数器作为时间戳源
 */
void Log_Init(void);

/**
 * @brief 注册日志输出回调
 * @param fn 输出函数指针 (如 &Uart_SendString)，传 NULL 则关闭输出
 */
void Log_SetOutput(Log_OutputFn fn);

/**
 * @brief 返回自启动以来的毫秒数
 */
uint32_t Log_GetTimeMs(void);

/**
 * @brief 核心日志写入函数 (宏展开后调用)
 * @param level  日志级别: 'E', 'W', 'I', 'D'
 * @param tag    模块标签 (如 "CanIf")
 * @param fmt    格式化字符串
 * @param ...    可变参数
 */
void Log_Write(char level, const char *tag, const char *fmt, ...);

/**
 * @brief 获取环形缓冲区内容（用于崩溃后 dump）
 * @param buf    输出缓冲区
 * @param size   输出缓冲区大小
 * @return 实际写入字节数
 */
uint32_t Log_DumpRingBuf(char *buf, uint32_t size);

/* ===================================================================
 *  分级日志宏
 * =================================================================== */

#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_E(tag, fmt, ...)  Log_Write('E', (tag), (fmt), ##__VA_ARGS__)
#else
#define LOG_E(tag, fmt, ...)  ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_W(tag, fmt, ...)  Log_Write('W', (tag), (fmt), ##__VA_ARGS__)
#else
#define LOG_W(tag, fmt, ...)  ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_I(tag, fmt, ...)  Log_Write('I', (tag), (fmt), ##__VA_ARGS__)
#else
#define LOG_I(tag, fmt, ...)  ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_D(tag, fmt, ...)  Log_Write('D', (tag), (fmt), ##__VA_ARGS__)
#else
#define LOG_D(tag, fmt, ...)  ((void)0)
#endif

#endif /* LOG_H */
