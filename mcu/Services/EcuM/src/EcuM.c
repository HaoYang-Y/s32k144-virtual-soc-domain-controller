/**
 * @file    EcuM.c
 * @brief   [AUTOSAR CP] ECU Manager 实现 — BSW 模块初始化调度
 *
 * @note    对标 AUTOSAR SWS_EcuM:
 *          EcuM 负责按顺序初始化所有 BSW 模块:
 *            MCAL → ECU Abstraction → Services → RTE → 进入 RUN 状态
 */

#include "EcuM.h"
#include "Can.h"
#include "Can_Cfg.h"
#include "CanIf.h"
/* TODO: 各层模块实现后依次加入
#include "Com.h"
#include "PduR.h"
#include "Rte.h"
*/

static EcuM_StateType EcuM_State = ECUM_STATE_STARTUP;

void EcuM_Init(void)
{
    /* ================================================================
     *  BSW 模块初始化顺序 (AUTOSAR CP 规范)
     *  自底向上: MCAL → ECU Abstraction → Services → RTE
     * ================================================================ */

    /* --- MCAL 层 --- */
    if (Can_Init(&Can_Config) != STATUS_SUCCESS) {
        return;
    }
    Can_SetControllerMode(CAN_CONTROLLER_0, CAN_CS_STARTED);

    /* --- ECU Abstraction 层 --- */
    CanIf_Init();
    /* TODO: SpiIf_Init(); */

    /* --- Services 层 --- */
    /* TODO: PduR_Init(); */
    /* TODO: Com_Init(); */

    /* --- RTE 层 --- */
    /* TODO: Rte_Init(); */

    EcuM_State = ECUM_STATE_RUN;
}

void EcuM_MainFunction(void)
{
    /* TODO: 运行状态机 — 处理睡眠/唤醒请求等 */
}

void EcuM_SelectShutdownTarget(void)
{
    /* TODO: 选择关机目标 (OFF / RESET / SLEEP) */
}

void EcuM_SetState(EcuM_StateType State)
{
    EcuM_State = State;
}
