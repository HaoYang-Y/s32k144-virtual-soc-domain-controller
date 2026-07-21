/**
 * CAN 测试 — 使用 AUTOSAR 标准接口 (ComStack_Types + CanIf + CanTp)
 *
 * BSW 初始化由 EcuM 统一调度（自底向上）:
 *   EcuM_Init():
 *     MCAL:            Can_Init(&Can_Config) → Can_SetControllerMode(STARTED)
 *     ECU Abstraction: CanIf_Init()
 *     Services:        PduR_Init() → CanTp_Init()
 *     RTE:             (Rte_Init 待实现)
 *
 * N-PDU 数据流:
 *   TX: main → CanIf_Transmit(PduId, &pduInfo) → Can_Write(Hth, &canPdu)
 *   RX: Can_Read() → CanIf_RxIndication(PduId, &pduInfo) → CanTp → PduR → Com
 *
 * PTD0=橙(TX) PTD1=红(错误) PTD15=绿(心跳) PTD16=蓝(RX)
 */
#include "Port.h"
#include "clock_config.h"
#include "Can.h"
#include "CanIf.h"
#include "CanIf_PduId.h"
#include "ComStack_Types.h"        /* PduInfoType */
#include "EcuM.h"
#include "pins_driver.h"
#include <stdint.h>

#define TX_MB  0U
#define RX_MB  1U  /* idx = TX数量 + 0 = 1 */

static void delay_ms(uint32_t ms){
    volatile uint32_t i; for(;ms>0;ms--) for(i=0;i<12000U;i++)__asm__("nop");
}

int main(void)
{
    uint32_t cnt=0;
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    Port_Init();
    PINS_DRV_SetPins(PTD, (1u<<0)|(1u<<1)|(1u<<15)|(1u<<16));

    /* BSW 模块初始化 (自底向上: MCAL → ECU Abstraction → Services → RTE) */
    EcuM_Init();

    for(;;)
    {
        /* ================================================================
         *  TX: main → CanIf_Transmit(PduId, &pduInfo) → Can_Write(Hth, &canPdu) → 硬件
         *  AUTOSAR 标准: CanIf_Transmit 不传 Controller，由 CanIf 内部查表获取
         * ================================================================ */
        {
            uint8_t txData[8];
            txData[0]=cnt&0xFF; txData[1]=(cnt>>8)&0xFF;
            txData[2]=0xAA; txData[3]=0x55; txData[4]=0xAA; txData[5]=0x55;
            txData[6]=(cnt>>16)&0xFF; txData[7]=(cnt>>24)&0xFF;

            /* 使用 AUTOSAR 标准 PduInfoType (即 N-PDU) */
            PduInfoType txPdu = {
                .SduId      = CANIF_PDU_ID_TX_0x123,
                .SduLength  = 8U,
                .SduDataPtr = txData,
            };

            PINS_DRV_ClearPins(PTD, 1u<<0);
            Std_ReturnType ret = CanIf_Transmit(CANIF_PDU_ID_TX_0x123, &txPdu);
            PINS_DRV_SetPins(PTD, 1u<<0);
            PINS_DRV_TogglePins(PTD, 1u<<15);
            if(ret != E_OK) PINS_DRV_ClearPins(PTD, 1u<<1);
            else            PINS_DRV_SetPins(PTD, 1u<<1);
        }

        /* ================================================================
         *  RX: Can_Read() → CanIf_RxIndication(PduId, &pduInfo) → CanIf 内部处理
         *  AUTOSAR 标准: CanIf_RxIndication 使用 PduId + PduInfoType*
         * ================================================================ */
        {
            Can_PduType rxCanPdu;
            if(Can_Read(0, RX_MB, &rxCanPdu)==STATUS_SUCCESS)
            {
                CanIf_PduIdType pduId = CanIf_FindPduIdByCanId(rxCanPdu.id);
                if (pduId < CANIF_PDU_COUNT) {
                    /* 使用 AUTOSAR 标准 PduInfoType */
                    PduInfoType rxPdu = {
                        .SduId      = pduId,
                        .SduLength  = rxCanPdu.length,
                        .SduDataPtr = rxCanPdu.data,
                    };
                    CanIf_RxIndication(pduId, &rxPdu);
                }
                PINS_DRV_TogglePins(PTD, 1u<<16);
            }
        }

        cnt++; delay_ms(500);
    }
    return 0;
}