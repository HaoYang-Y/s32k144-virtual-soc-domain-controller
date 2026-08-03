/**
 * @file    Det.c
 * @brief   [AUTOSAR CP] Default Error Tracer 实现 — 学习版
 *
 * @note    学习版实现: 错误信息通过 LOG_E 输出到 UART。
 *          生产级系统中 DET 通常写入环形缓冲区、触发断点或调用钩子函数。
 */

#include "Det.h"
#include "Log.h"

void Det_Init(void)
{
    LOG_I("Det", "Init done (development mode)");
}

/**
 * @brief 报告开发期错误
 *
 * 记录到 UART 日志，格式: [COM] DET Error (Mod=22, Api=1, Err=1): invalid SignalId
 */
void Det_ReportError(uint16_t ModuleId, uint8_t InstanceId,
                     uint8_t ApiId, uint8_t ErrorId)
{
    (void)InstanceId;

    /* 错误描述映射表 */
    static const char *ModNames[] = {
        [DET_MODULE_ID_COM]   = "Com",
        [DET_MODULE_ID_PDUR]  = "PduR",
        [DET_MODULE_ID_CANIF] = "CanIf",
        [DET_MODULE_ID_CANTP] = "CanTp",
        [DET_MODULE_ID_CAN]   = "Can",
    };

    const char *mod_name = "Unknown";
    if (ModuleId < (uint16_t)(sizeof(ModNames) / sizeof(ModNames[0]))
        && ModNames[ModuleId] != NULL) {
        mod_name = ModNames[ModuleId];
    }

    LOG_E(mod_name, "DET Err (API=%u, Err=%u)", (unsigned int)ApiId, (unsigned int)ErrorId);
}
