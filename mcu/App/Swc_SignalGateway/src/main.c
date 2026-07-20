/**
 * CAN 测试 — 使用 AUTOSAR CanIf 接口（ECU Abstraction 层）
 *
 * BSW 初始化顺序（由 EcuM 统一调度）:
 *   MCAL: Can_Init() → Can_SetControllerMode(STARTED)
 *   ECU Abstraction: EcuM_Init() → CanIf_Init()
 *   Services:  (PduR_Init, Com_Init 待实现后加入 EcuM)
 *   RTE:       (Rte_Init 待实现后加入 EcuM)
 *
 * 数据流:
 *   TX: main → CanIf_Transmit() → Can_Write() → 硬件
 *   RX: main → Can_Read() → CanIf_RxIndication()
 *
 * PTD0=橙(TX) PTD1=红(错误) PTD15=绿(心跳) PTD16=蓝(RX)
 */
#include "Port.h"
#include "clock_config.h"
#include "Can.h"
#include "CanIf.h"
#include "CanIf_PduId.h"
#include "EcuM.h"
#include "pins_driver.h"
#include <stdint.h>

#define TX_MB  0U
#define RX_MB  1U  /* idx = TX数量 + 0 = 1 */

static const Can_HardwareObject tx_mb[] = {{.id=0x123UL}};
static const Can_HardwareObject rx_mb[] = {{.id=0x100UL}};

static const Can_ConfigType can0_cfg = {
    .controller=0,.max_num_mb=16,.flexcan_mode=CAN_MODE_NORMAL,
    .prop_seg=7,.phase_seg1=4,.phase_seg2=1,.pre_divider=0,.r_jumpwidth=1,
    .num_tx_mailboxes=1,.num_rx_mailboxes=1,
    .tx_mailboxes=tx_mb,.rx_mailboxes=rx_mb
};

static void delay_ms(uint32_t ms){
    volatile uint32_t i; for(;ms>0;ms--) for(i=0;i<12000U;i++)__asm__("nop");
}

int main(void)
{
    uint32_t cnt=0;
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    Port_Init();
    PINS_DRV_SetPins(PTD, (1u<<0)|(1u<<1)|(1u<<15)|(1u<<16));

    /* --- 硬件层初始化 (MCAL Can) --- */
    if(Can_Init(&can0_cfg)!=STATUS_SUCCESS){
        PINS_DRV_ClearPins(PTD, 1u<<1); /* 红=失败 */
        for(;;){}
    }
    Can_SetControllerMode(0, CAN_CS_STARTED);

    /* --- BSW 模块初始化 (ECU Abstraction → Services → RTE) --- */
    EcuM_Init();  /* 内部按 AUTOSAR 顺序调用 CanIf_Init() 等 */

    for(;;)
    {
        /* ================================================================
         * TX 路径: main → CanIf_Transmit() → Can_Write() → 硬件
         * ================================================================ */
        {
            uint8_t txData[8];
            txData[0]=cnt&0xFF;
            txData[1]=(cnt>>8)&0xFF;
            txData[2]=0xAA;
            txData[3]=0x55;
            txData[4]=0xAA;
            txData[5]=0x55;
            txData[6]=(cnt>>16)&0xFF;
            txData[7]=(cnt>>24)&0xFF;

            CanIf_PduType txPdu = {
                .id     = CANIF_PDU_ID_TX_0x123,
                .length = 8U,
                .data   = txData
            };

            PINS_DRV_ClearPins(PTD, 1u<<0);
            uint8_t ret = CanIf_Transmit(0, &txPdu);
            PINS_DRV_SetPins(PTD, 1u<<0);

            PINS_DRV_TogglePins(PTD, 1u<<15);           /* 绿心跳 */
            if(ret != E_OK) PINS_DRV_ClearPins(PTD, 1u<<1);   /* 红=TX失败 */
            else            PINS_DRV_SetPins(PTD, 1u<<1);
        }

        /* ================================================================
         * RX 路径: Can_Read() → CanIf_RxIndication()
         * ================================================================ */
        {
            Can_PduType rxCanPdu;
            if(Can_Read(0, RX_MB, &rxCanPdu)==STATUS_SUCCESS)
            {
                /* CAN ID → PDU ID (CanIf 层职责) */
                CanIf_PduIdType pduId = CanIf_FindPduIdByCanId(rxCanPdu.id);
                if (pduId < CANIF_PDU_COUNT) {
                    CanIf_PduType rxIfPdu = {
                        .id     = pduId,
                        .length = rxCanPdu.length,
                        .data   = rxCanPdu.data
                    };
                    CanIf_RxIndication(0, &rxIfPdu);
                }
                PINS_DRV_TogglePins(PTD, 1u<<16);       /* 蓝=RX */
            }
        }

        cnt++; delay_ms(500);
    }
    return 0;
}
