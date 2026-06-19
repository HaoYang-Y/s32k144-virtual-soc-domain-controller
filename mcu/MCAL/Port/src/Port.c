/**
 * @file    Port.c
 * @brief   AUTOSAR CP MCAL Port 驱动实现
 *
 * @note    委托给 NXP SDK PINS_DRV_Init() 完成引脚初始化
 *          配置数组 g_pin_mux_InitConfigArr0 定义在 pin_mux.c 中
 *          (由 NXP Pins Tool 生成，可独立更新)
 */

#include "Port.h"
#include "pin_mux.h"      /* g_pin_mux_InitConfigArr0, NUM_OF_CONFIGURED_PINS0 */

/* SDK 依赖仅在本 .c 内可见 */
#include "pins_driver.h"  /* PINS_DRV_Init */
#include <stddef.h>

void Port_Init(void)
{
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0, g_pin_mux_InitConfigArr0);
}