/**
 * @file    EcuM.h
 * @brief   [SKELETON] ECU Manager — AUTOSAR CP Services 层
 *
 * @note    ECU 状态管理: 启动/关闭/休眠/唤醒序列
 *          EcuM_Init → 初始化 BSW 所有模块 → 启动 RTE → 进入运行态
 */

#ifndef ECUM_H
#define ECUM_H

#include "Std_Types.h"

typedef enum {
    ECUM_STATE_STARTUP      = 0,
    ECUM_STATE_RUN          = 1,
    ECUM_STATE_SHUTDOWN     = 2,
    ECUM_STATE_SLEEP        = 3,
    ECUM_STATE_WAKEUP       = 4
} EcuM_StateType;

void EcuM_Init(void);
bool EcuM_MainFunction(void);  /* 返回 true 表示本轮处理了 RX 数据 */
void EcuM_SelectShutdownTarget(void);
void EcuM_SetState(EcuM_StateType State);

#endif /* ECUM_H */
