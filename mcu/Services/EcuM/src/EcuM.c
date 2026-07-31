/**
 * @file    EcuM.c
 * @brief   [AUTOSAR CP] ECU Manager 实现 — BSW 模块初始化调度
 *
 * @note    对标 AUTOSAR SWS_EcuM:
 *          EcuM 负责按顺序初始化所有 BSW 模块:
 *            MCAL → ECU Abstraction → Services → RTE → 进入 RUN 状态
 */

#include "EcuM.h"
#include "Port.h"
#include "clock_config.h"
#include "Can.h"
#include "Can_Cfg.h"
#include "CanIf.h"
#include "CanTp.h"
#include "PduR.h"
/* TODO: 各层模块实现后依次加入
#include "Com.h"
#include "Rte.h"
*/

static EcuM_StateType EcuM_State = ECUM_STATE_STARTUP;

void EcuM_Init(void)
{
    /* ================================================================
     *  BSW 模块初始化顺序 (AUTOSAR CP 规范)
     *  自底向上: MCAL → ECU Abstraction → Services → RTE
     * ================================================================ */

    /* --- 硬件前置: 时钟 + 引脚复用 (必须在所有 MCAL 模块之前) --- */
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    Port_Init();

    /* --- MCAL 层 --- */
    if (Can_Init(CAN_CONTROLLER_0, &Can_Config_CAN0) != E_OK) {
        return;
    }
    (void)Can_SetControllerMode(CAN_CONTROLLER_0, CAN_CS_STARTED);
    Can_EnableInterrupts();  /* RX 中断模式 */

    /* --- ECU Abstraction 层 --- */
    CanIf_Init();
    /* TODO: SpiIf_Init(); */

    /* --- Services 层 --- */
    PduR_Init();
    CanTp_Init();
    /* TODO: Com_Init(); */

    /* --- RTE 层 --- */
    /* TODO: Rte_Init(); */

    EcuM_State = ECUM_STATE_RUN;
}

void EcuM_MainFunction(void)
{
    /* 驱动 CAN Transport Layer 流控状态机 (FF → FC → CF) */
    CanTp_MainFunction();

    /* RX: ISR 标记 → 消费 CAN 帧 → CanIf → PduR → CanTp 重组 */
    (void)Can_MainFunctionRx();

    /* TX: ISR 标记 → 消费 TX 完成 → 回调 CanIf → PduR → CanTp 确认 */
    Can_MainFunctionWrite();
}

void EcuM_SelectShutdownTarget(void)
{
    /* TODO: 选择关机目标 (OFF / RESET / SLEEP) */
}

void EcuM_SetState(EcuM_StateType State)
{
    EcuM_State = State;
}