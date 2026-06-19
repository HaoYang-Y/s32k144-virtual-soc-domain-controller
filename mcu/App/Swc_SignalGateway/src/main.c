/**
 * @file    main.c
 * @brief   S32K144 CAN 0 测试固件 - 通过 MCAL Can 模块循环发送 CAN 消息
 * @note    使用 MCAL Can 模块封装 flexcan_driver，CAN 2.0 500kbps
 * @note    PTD15 LED 心跳指示，USBCAN 连接 CAN0 (PTE4=CAN0_RX, PTE5=CAN0_TX)
 * @note    日志通过 LPUART0 (PTA1=RX, PTA2=TX) 输出到 Ubuntu uart_logger
 * @note    candump can0 接收
 */

#include "Port.h"
#include "clock_config.h"
#include "interrupt_manager.h"
#include "Can.h"
#include "pins_driver.h"
#include "Uart.h"
#include "Log.h"
#include <stdint.h>
#include <stdbool.h>

/* =====================================================================
 * 宏定义
 * ===================================================================== */
#define LED_GPIO         PTD                     /* LED0 心跳引脚所属 GPIO 端口 */
#define LED_PIN          15U                     /* LED0 心跳引脚号 (PTD15) */
#define LED_ERR_GPIO     PTD                     /* LED1 失败指示引脚所属 GPIO 端口 */
#define LED_ERR_PIN      16U                     /* LED1 失败指示引脚号 (PTD16) */
#define TX_MAILBOX_IDX   0U                      /* 发送 Mailbox 索引 (TX MB 从索引 0 开始) */
#define TX_MSG_ID        0x123UL                 /* CAN 消息 ID */
#define CAN_CONTROLLER   0U                      /* 使用 FlexCAN0 */
#define LOOP_DELAY       48000000UL              /* 约 500ms (48MHz × 0.5s) */

/* =====================================================================
 * 静态配置：CAN 0 发送邮箱和接收邮箱硬件对象
 * ===================================================================== */

/** 发送邮箱 ID 过滤表配置 */
static const Can_HardwareObject can0_tx_mailboxes[] = {
    { .id = TX_MSG_ID, .is_extended = false, .is_remote = false }
};

/** 接收邮箱 ID 过滤表配置（暂不使用，预留） */
static const Can_HardwareObject can0_rx_mailboxes[] = {
    { .id = 0x100UL, .is_extended = false, .is_remote = false }
};

/** CAN 控制器配置 — 纯 MCAL 类型，不含任何 FlexCAN SDK 字段 */
static const Can_ConfigType can0_config = {
    .controller        = 0U,
    .max_num_mb        = 16U,
    .num_id_filters    = 0U,                     /* 未使用 RxFIFO */
    .is_rx_fifo_needed = false,
    .flexcan_mode      = CAN_MODE_NORMAL,
    .prop_seg          = 7U,
    .phase_seg1        = 4U,
    .phase_seg2        = 1U,
    .pre_divider       = 0U,                     /* 8MHz/(0+1)/16TQ = 500kbps */
    .r_jumpwidth       = 1U,
    .num_tx_mailboxes  = 1U,
    .num_rx_mailboxes  = 1U,
    .tx_mailboxes      = can0_tx_mailboxes,
    .rx_mailboxes      = can0_rx_mailboxes
};

/* =====================================================================
 * 函数原型声明
 * ===================================================================== */
static void BoardInit(void);
static void LogInit(void);
static void GPIOInit(void);
static void CAN0_Init(void);
static void Delay(uint32_t loops);

/* =====================================================================
 * 函数: main
 * ===================================================================== */
int main(void)
{
    uint32_t counter = 0U;
    status_t canStatus;

    /* 1. 时钟 + 引脚复用（含 LPUART0 引脚） */
    BoardInit();

    /* 2. 日志系统: UART → Log → 绑定输出 */
    LogInit();

    LOG_I("main", "=== S32K144 Domain Controller boot ===");

    /* 3. GPIO (LED) */
    GPIOInit();

    /* 4. CAN0 */
    CAN0_Init();

    LOG_I("main", "All modules initialized, entering main loop");

    /* 主循环：每隔约 500ms 发送一帧 CAN 消息 */
    for (;;)
    {
        /* 构造 MCAL 层 CAN 报文 */
        Can_PduType txPdu;
        txPdu.id         = TX_MSG_ID;
        txPdu.length     = 8U;
        txPdu.is_extended = false;
        txPdu.is_remote  = false;
        txPdu.data[0] = (uint8_t)(counter       & 0xFFU);
        txPdu.data[1] = (uint8_t)((counter >> 8)  & 0xFFU);
        txPdu.data[2] = (uint8_t)((counter >> 16) & 0xFFU);
        txPdu.data[3] = (uint8_t)((counter >> 24) & 0xFFU);
        txPdu.data[4] = 0xAAU;
        txPdu.data[5] = 0x55U;
        txPdu.data[6] = 0xAAU;
        txPdu.data[7] = 0x55U;

        LOG_D("main", "TX frame id=0x%03lX len=8 cnt=%lu",
              (unsigned long)TX_MSG_ID, (unsigned long)counter);

        /* 通过 MCAL Can_Write 发送 CAN 消息 */
        canStatus = Can_Write(CAN_CONTROLLER, TX_MAILBOX_IDX, &txPdu);

        /* LED0 心跳：始终翻转，证明 MCU 在运行 */
        PINS_DRV_TogglePins(LED_GPIO, 1UL << LED_PIN);

        /* LED1 失败指示：发送成功灭，发送失败亮 */
        if (canStatus != STATUS_SUCCESS)
        {
            PINS_DRV_ClearPins(LED_ERR_GPIO, 1UL << LED_ERR_PIN);
            LOG_E("main", "Can_Write failed! status=0x%02lX",
                  (unsigned long)canStatus);
        }
        else
        {
            PINS_DRV_SetPins(LED_ERR_GPIO, 1UL << LED_ERR_PIN);
        }

        counter++;
        Delay(LOOP_DELAY);
    }

    return 0;
}

/* =====================================================================
 * 函数: BoardInit
 * ===================================================================== */
static void BoardInit(void)
{
    /* 初始化时钟管理器 (SDK 生成配置) */
    CLOCK_DRV_Init(&clockMan1_InitConfig0);

    /* 配置引脚复用 — 通过 MCAL Port 模块 */
    Port_Init();
}

/* =====================================================================
 * 函数: LogInit
 * ===================================================================== */
static void LogInit(void)
{
    status_t ret;

    /* 初始化日志模块（环形缓冲区 + DWT 时间戳） */
    Log_Init();

    /* 初始化 LPUART0 (115200-8-N-1) */
    ret = Uart_Init(115200UL);
    if (ret != STATUS_SUCCESS) {
        /* UART 初始化失败，日志静默输出到环形缓冲区（无 UART 输出） */
        return;
    }

    /* 绑定 UART 输出到日志系统 */
    Log_SetOutput(&Uart_SendString);
}

/* =====================================================================
 * 函数: GPIOInit
 * ===================================================================== */
static void GPIOInit(void)
{
    /* LED0 (PTD15): 心跳输出，初始高电平（灭） */
    PINS_DRV_SetPinsDirection(LED_GPIO, 1UL << LED_PIN);
    PINS_DRV_SetPins(LED_GPIO, 1UL << LED_PIN);

    /* LED1 (PTD16): 失败指示输出，初始高电平（灭） */
    PINS_DRV_SetPinsDirection(LED_ERR_GPIO, 1UL << LED_ERR_PIN);
    PINS_DRV_SetPins(LED_ERR_GPIO, 1UL << LED_ERR_PIN);
}

/* =====================================================================
 * 函数: CAN0_Init
 * ===================================================================== */
static void CAN0_Init(void)
{
    status_t ret;

    ret = Can_Init(&can0_config);
    if (ret != STATUS_SUCCESS) {
        LOG_E("main", "Can_Init failed! status=0x%02lX", (unsigned long)ret);
        PINS_DRV_ClearPins(LED_ERR_GPIO, 1UL << LED_ERR_PIN);
        return;
    }
    LOG_I("main", "CAN0 init OK, 500kbps");

    Can_SetControllerMode(CAN_CONTROLLER, CAN_CS_STARTED);
    LOG_I("main", "CAN0 controller STARTED");
}

/* =====================================================================
 * 函数: Delay
 * ===================================================================== */
static void Delay(uint32_t loops)
{
    volatile uint32_t i;
    for (i = 0U; i < loops; i++)
    {
        __asm__ volatile("nop");
    }
}
