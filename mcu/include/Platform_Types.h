/**
 * @file    Platform_Types.h
 * @brief   AUTOSAR 平台相关类型定义
 *
 * @note    对应 AUTOSAR_SWS_PlatformTypes 规范
 *          定义编译器相关的基本类型宽度和字节序
 */

#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* CPU 类型 */
#define CPU_TYPE            CPU_TYPE_32
#define CPU_BIT_ORDER       MSB_FIRST
#define CPU_BYTE_ORDER      HIGH_BYTE_FIRST

#ifndef FALSE
#define FALSE       (boolean)false
#endif
#ifndef TRUE
#define TRUE        (boolean)true
#endif

typedef _Bool      boolean;

#endif /* PLATFORM_TYPES_H */
