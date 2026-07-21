/**
 * @file    Std_Types.h
 * @brief   AUTOSAR 标准类型定义（CP）
 *
 * @note    对应 AUTOSAR_SWS_StdTypes 规范
 *          提供 uint8/uint16/uint32/sint8/sint16/sint32 等标准类型别名
 */

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "Compiler.h"

/* ===================================================================
 *  AUTOSAR 标准类型
 * =================================================================== */

typedef uint8_t         uint8;
typedef uint16_t        uint16;
typedef uint32_t        uint32;
typedef uint64_t        uint64;
typedef int8_t          sint8;
typedef int16_t         sint16;
typedef int32_t         sint32;
typedef int64_t         sint64;
typedef volatile uint8_t    vuint8;
typedef volatile uint16_t   vuint16;
typedef volatile uint32_t   vuint32;

/* STD_HIGH / STD_LOW */
#ifndef STD_HIGH
#define STD_HIGH   1u
#endif
#ifndef STD_LOW
#define STD_LOW    0u
#endif

#ifndef STD_ON
#define STD_ON     1u
#endif
#ifndef STD_OFF
#define STD_OFF    0u
#endif

/* E_OK / E_NOT_OK */
#ifndef E_OK
#define E_OK       0u
#endif
#ifndef E_NOT_OK
#define E_NOT_OK   1u
#endif

/** AUTOSAR SWS_StdTypes_00007 — 标准返回值类型 */
typedef uint8 Std_ReturnType;

#endif /* STD_TYPES_H */
