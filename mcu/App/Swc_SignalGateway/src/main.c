/**
 * @file    main.c
 * @brief   AUTOSAR CP 完整通信链路 Demo — SWC → RTE → COM → PduR → CanTp → CanIf → Can
 *
 * @note    本 demo 展示 AUTOSAR 通信栈的完整数据流:
 *
 *          TX 路径 (SWC 发送信号到 CAN 总线):
 *            SWC 调用 Rte_Write → RTE → Com_SendSignal (打包信号到 I-PDU)
 *            → Com_MainFunction (500ms 周期触发) → PduR_ComTransmit
 *            → CanTp_Transmit (SF/FF 分段) → CanIf_Transmit → Can_Write → 硬件
 *
 *          RX 路径 (CAN 总线信号到 SWC):
 *            CAN 硬件中断 → CanIf_RxIndication → PduR_CanIfRxIndication
 *            → CanTp_RxIndication (重组) → PduR_CanTpRxIndication
 *            → Com_RxIndication (解包 I-PDU → 更新信号 Shadow Buffer)
 *            → SWC 调用 Rte_Read → RTE → Com_ReceiveSignal → 读取信号值
 *
 *          与旧版 (直接 CanTp_Transmit) 的区别:
 *          - 旧版: main.c 手工构造 PduInfoType → 裸调 CanTp (绕过 Com 和 RTE)
 *          - 新版: main.c 调 Rte_Write → 经 Com 打包 → MainFunction 自动发送
 *          - 好处: SWC 不感知 I-PDU 结构、CAN 帧格式、发送时机——全部由配置决定
 *
 *          信号布局 (来自 Com_Cfg.h):
 *            I-PDU 0x123 (TX, 周期 500ms):
 *              byte[0:3] = TestTxCounter (32-bit intel, TRIGGERED)
 *              byte[4]   = TestTxMagic0  (8-bit, PENDING)
 *              byte[5]   = TestTxMagic1  (8-bit, NONE - 仅周期)
 *            I-PDU 0x100 (RX, 事件触发):
 *              byte[0:7] = TestRxData (64-bit intel)
 *
 *          GPIO LED:
 *            PTD0=橙(TX) PTD1=红(错误) PTD15=绿(心跳) PTD16=蓝(RX)
 */

#include "Com.h"             /* Com_SendSignal, Com_ReceiveSignal */
#include "EcuM.h"            /* EcuM_Init, EcuM_MainFunction */
#include "Rte.h"             /* Rte_Write_VehicleSignal, Rte_Read_VehicleSignal */
#include "pins_driver.h"
#include <stdint.h>
#include <string.h>

/* TX 定时: 主循环 ~1ms/次, 每 1000 次写一次信号 (~1 Hz 更新信号值)
 * 但实际发送频率由 Com_IPduConfig[0].cycle_time_ms = 500ms 控制
 * 这意味着: SWC 每秒写 1 次信号值, COM 每 500ms 发送一次 I-PDU */
#define SIGNAL_UPDATE_DIVIDER  1000U

int main(void)
{
    uint32_t cnt    = 0U;
    uint32_t tx_cnt = 0U;     /* 递增计数器, 作为 TestTxCounter 信号值 */

    /* BSW 全栈初始化 (自底向上): 时钟→Port→Can→CanIf→PduR→CanTp→Com→RTE */
    EcuM_Init();

    /* LED 初始化: 全灭 */
    PINS_DRV_SetPins(PTD, (1u << 0) | (1u << 1) | (1u << 15) | (1u << 16));

    for (;;) {
        /* ================================================================
         *  BSW 周期处理 (MainFunction 链)
         *
         *  Com_MainFunction:  周期发送 dirty I-PDU (500ms)
         *  CanTp_MainFunction: 多帧流控状态机 (FF→FC→CF) + 超时检测
         *  Can_MainFunctionRx: 消费 RX 中断 → CanIf → PduR → CanTp 重组
         *  Can_MainFunctionWrite: 消费 TX 中断 → 回调链
         *  Rte_MainFunction:   SWC 周期任务
         * ================================================================ */
        EcuM_MainFunction();

        /* ================================================================
         *  SWC 业务逻辑: 周期更新信号值 (模拟真实传感器数据)
         *
         *  AUTOSAR 中 SWC 不直接操作硬件，而是通过 Rte_Write 发送信号。
         *  RTE 将调用转发给 Com_SendSignal，后者将信号打包到 I-PDU。
         *  Com_MainFunction 负责按 cycle_time_ms 周期发送。
         * ================================================================ */
        if ((cnt % SIGNAL_UPDATE_DIVIDER) == 0U) {
            tx_cnt++;

            /* TestTxCounter: 32-bit 递增计数器 (TRIGGERED: 立即触发发送) */
            Rte_Write_VehicleSignal(COM_SIGNAL_ID_TEST_TX_COUNTER,
                                    (const uint8_t *)&tx_cnt, 4U);

            /* TestTxMagic0: 8-bit 魔数 0xAA (PENDING: 等 MainFunction 发送) */
            {
                uint8_t magic0 = 0xAAU;
                Rte_Write_VehicleSignal(COM_SIGNAL_ID_TEST_TX_MAGIC0,
                                        &magic0, 1U);
            }

            /* TestTxMagic1: 8-bit 魔数 0x55 (NONE: 仅周期发送, 不单独触发) */
            {
                uint8_t magic1 = 0x55U;
                Rte_Write_VehicleSignal(COM_SIGNAL_ID_TEST_TX_MAGIC1,
                                        &magic1, 1U);
            }

            /* LED: TX 指示 (橙灯闪一次) */
            PINS_DRV_ClearPins(PTD, 1u << 0);               /* 橙亮 */
            for (volatile uint32_t d = 0U; d < 480000U; d++) { __asm__("nop"); }
            PINS_DRV_SetPins(PTD, 1u << 0);                 /* 橙灭 */

            PINS_DRV_TogglePins(PTD, 1u << 15);             /* 绿 = 心跳翻转 */
        }

        /* ================================================================
         *  RX 检查: AUTOSAR Update Bit + Deadline Monitoring
         *
         *  蓝灯 = Update Bit 指示 — 收到新数据后亮 1 个循环周期
         *  红灯 = Timeout 指示  — 超时亮
         * ================================================================ */
        {
            static uint8_t blue_counter = 0U;

            /* 检查 Update Bit */
            if (Com_GetUpdateBit(COM_SIGNAL_ID_TEST_RX_DATA)) {
                uint64_t rx_data      = 0ULL;
                uint8_t  rx_buf[8]    = {0U};
                uint8_t  rx_len       = 8U;

                Rte_Read_VehicleSignal(COM_SIGNAL_ID_TEST_RX_DATA,
                                       rx_buf, &rx_len);
                (void)memcpy(&rx_data, rx_buf, sizeof(rx_data));

                if (rx_data != 0ULL) {
                    PINS_DRV_ClearPins(PTD, 1u << 16);  /* 蓝灯亮 = RX OK */
                    blue_counter = 50U;                  /* 持续 ~50ms */
                }
            }

            /* 蓝灯延时熄灭 (让它能被人眼看到) */
            if (blue_counter > 0U) {
                blue_counter--;
                if (blue_counter == 0U) {
                    PINS_DRV_SetPins(PTD, 1u << 16);    /* 蓝灯灭 */
                }
            }

            /* 超时检测 */
            if (Com_GetSignalStatus(COM_SIGNAL_ID_TEST_RX_DATA)
                == COM_SIGNAL_TIMEOUT) {
                PINS_DRV_ClearPins(PTD, 1u << 1);       /* 红灯亮 = 超时 */
            } else {
                PINS_DRV_SetPins(PTD, 1u << 1);         /* 红灯灭 = 正常 */
            }
        }

        cnt++;
        /* 主循环延时 ~1ms (裸机忙等, 无 RTOS tick) */
        for (volatile uint32_t i = 0U; i < 4000U; i++) { __asm__("nop"); }
    }
    return 0;
}
