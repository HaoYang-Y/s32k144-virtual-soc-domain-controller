/**
 * CAN TP 测试 — CanTp 流控状态机 (SF/MF 交替)
 *
 * BSW 初始化由 EcuM 统一管理:
 *   EcuM_Init(): 时钟→Port→Can→CanIf→PduR→CanTp→中断使能
 *
 * 主循环高频轮询 EcuM_MainFunction 以保证 CF 流控节奏 (STmin),
 * TX 通过分频计数器控制发送速率 (~2 Hz).
 *
 * PTD0=橙(TX) PTD1=红(错误) PTD15=绿(心跳) PTD16=蓝(RX)
 */
#include "CanTp.h"           /* CanTp_Transmit (含 PduInfoType) */
#include "EcuM.h"            /* EcuM_Init, EcuM_MainFunction */
#include "pins_driver.h"
#include <stdint.h>

/* TX 分频: 主循环 ~1ms/次, 每 500 次迭代发一帧 → ~2 Hz */
#define TX_DIVIDER  500U

int main(void)
{
    uint32_t cnt = 0U;

    /* BSW 全栈初始化: 时钟→Port→Can→CanIf→PduR→CanTp→中断使能 */
    EcuM_Init();

    /* LED (Port_Init 在 EcuM_Init 内部已完成, GPIO 已可用) */
    PINS_DRV_SetPins(PTD, (1u<<0)|(1u<<1)|(1u<<15)|(1u<<16));
    PINS_DRV_SetPins(PTD, 1u<<1);  /* 红灯初始灭 */

    for (;;)
    {
        /* BSW 周期处理: CanTp 流控 + CAN RX 消费 */
        EcuM_MainFunction();

        /* TX: 分频发送, SF(7B) 和 MF(20B) 交替 */
        if ((cnt % TX_DIVIDER) == 0U) {
            uint8_t  txData[32];
            uint16_t dataLen;

            if (((cnt / TX_DIVIDER) & 1U) == 0U) {
                dataLen = 7U;                                    /* SF: 单帧直发 */
                for (uint8_t i = 0U; i < dataLen; i++) txData[i] = (uint8_t)(cnt + i);
            } else {
                dataLen = 20U;                                   /* MF: FF+CF+CF */
                for (uint8_t i = 0U; i < dataLen; i++) txData[i] = (uint8_t)(cnt + i);
            }

            PduInfoType txPdu = { .SduId = 0U, .SduLength = dataLen, .SduDataPtr = txData };

            PINS_DRV_ClearPins(PTD, 1u<<0);                    /* 橙亮 */
            Std_ReturnType ret = CanTp_Transmit(0U, &txPdu);
            for(volatile uint32_t d=0;d<480000U;d++)__asm__("nop"); /* ~10ms */
            PINS_DRV_SetPins(PTD, 1u<<0);                    /* 橙灭 */
            PINS_DRV_TogglePins(PTD, 1u<<15);                    /* 绿=心跳 */
            if (ret != E_OK) PINS_DRV_ClearPins(PTD, 1u<<1);    /* 红=错误 */
            else            PINS_DRV_SetPins(PTD, 1u<<1);
        }

        cnt++;
        for (volatile uint32_t i = 0U; i < 4000U; i++) { __asm__("nop"); }
    }
    return 0;
}