/**
 * @file    EcuM.c
 * @brief   [SKELETON] ECU Manager 实现
 */

#include "EcuM.h"

static EcuM_StateType EcuM_State = ECUM_STATE_STARTUP;

void EcuM_Init(void)
{
    /* TODO: 初始化 EcuM 状态机 */
    EcuM_State = ECUM_STATE_RUN;
}

void EcuM_MainFunction(void)
{
    /* TODO: 运行状态机 */
}

void EcuM_SelectShutdownTarget(void)
{
    /* TODO */
}

void EcuM_SetState(EcuM_StateType State)
{
    EcuM_State = State;
}
