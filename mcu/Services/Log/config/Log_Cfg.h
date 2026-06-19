/**
 * @file    Log_Cfg.h
 * @brief   分级日志预编译配置
 */

#ifndef LOG_CFG_H
#define LOG_CFG_H

/* ===================================================================
 *  日志级别 (数值越大越详细)
 * =================================================================== */
#define LOG_LEVEL_ERROR   1U
#define LOG_LEVEL_WARN    2U
#define LOG_LEVEL_INFO    3U
#define LOG_LEVEL_DEBUG   4U

/* ===================================================================
 *  编译期日志级别开关
 *  设为 LOG_LEVEL_DEBUG (4) 时所有 LOG_D() 参与编译
 *  设为 LOG_LEVEL_INFO  (3) 时 LOG_D() 被编译为 no-op
 *  设为 LOG_LEVEL_WARN  (2) 时 LOG_D()/LOG_I() 都不参与编译
 *  设为 LOG_LEVEL_ERROR (1) 时仅 LOG_E() 参与编译
 * =================================================================== */
#ifndef LOG_LEVEL
#define LOG_LEVEL  LOG_LEVEL_DEBUG
#endif

/* ===================================================================
 *  环形缓冲区大小 (字节)
 * =================================================================== */
#define LOG_RINGBUF_SIZE  2048U

#endif /* LOG_CFG_H */
