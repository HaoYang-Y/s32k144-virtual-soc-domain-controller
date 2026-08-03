/**
 * @file    Det.h
 * @brief   [AUTOSAR CP] Default Error Tracer — Development Error Reporting
 *
 * @note    对标 AUTOSAR SWS_Det:
 *          Det_ReportError(ModuleId, InstanceId, ApiId, ErrorId)
 *
 *          AUTOSAR 学习要点:
 *          - DET 只在开发阶段启用 (Production 代码中去掉)
 *          - 用于捕获 BSW 模块的非法参数、状态冲突等编程错误
 *          - 不是给最终用户看的，而是给集成工程师调试用的
 *          - 本学习实现: 直接通过 LOG_E 输出错误信息
 */

#ifndef DET_H
#define DET_H

#include "Std_Types.h"

/* ===================================================================
 *  AUTOSAR 标准模块 ID (部分)
 * =================================================================== */
#define DET_MODULE_ID_CAN      (16U)  /* Can */
#define DET_MODULE_ID_CANIF    (18U)  /* CanIf */
#define DET_MODULE_ID_PDUR     (21U)  /* PduR */
#define DET_MODULE_ID_COM      (22U)  /* Com */
#define DET_MODULE_ID_CANTP    (28U)  /* CanTp */

/* ===================================================================
 *  COM 模块错误码 (AUTOSAR SWS_COM 标准)
 * =================================================================== */
#define COM_E_PARAM             (0x01U)  /* 参数非法 (NULL 指针, ID 越界) */
#define COM_E_UNINIT            (0x02U)  /* 模块未初始化 */
#define COM_E_SIGNAL_NOT_FOUND  (0x04U)  /* 信号 ID 在配置表中未找到 */

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

/**
 * @brief 报告开发期错误 (SWS_Det_00020)
 *
 * AUTOSAR 标准: BSW 模块在检测到非法状态时调用此函数。
 * 在 Release 版本中通常被宏替换为空操作。
 *
 * @param ModuleId   模块 ID (如 DET_MODULE_ID_COM)
 * @param InstanceId 实例 ID (通常为 0，单实例模块)
 * @param ApiId      发生错误的 API ID
 * @param ErrorId    错误码 (如 COM_E_PARAM)
 */
void Det_ReportError(uint16_t ModuleId, uint8_t InstanceId,
                     uint8_t ApiId, uint8_t ErrorId);

/** @brief 初始化 DET 模块 */
void Det_Init(void);

#endif /* DET_H */
