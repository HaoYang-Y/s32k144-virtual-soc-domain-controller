/**
 * @file    Port.h
 * @brief   AUTOSAR CP MCAL Port 驱动 — 引脚复用配置
 *
 * @note    底层调用 NXP S32 SDK PINS_DRV_Init() 配置所有 IO 引脚
 *          配置数据来自 pin_mux.c (NXP Config Tools 生成)
 *          头文件不暴露任何 SDK 类型
 */

#ifndef MCAL_PORT_H
#define MCAL_PORT_H

#include <stdint.h>

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

/**
 * @brief 初始化所有 IO 引脚 — 配置复用功能、方向、上下拉
 *
 * @note  调用 NXP SDK PINS_DRV_Init() 批量完成 PORT PCR 配置
 *        内部引用 pin_mux.c 中的 g_pin_mux_InitConfigArr0 数组
 */
void Port_Init(void);

#endif /* MCAL_PORT_H */