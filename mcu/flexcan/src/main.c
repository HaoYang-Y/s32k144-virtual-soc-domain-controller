/*
 * @brief FlexCAN 模块主程序 — CAN 通信自收发测试
 *        对应书籍：第11章 CAN 总线应用实验
 *
 * 学习目标：
 *   1. 掌握 FlexCAN 模块初始化流程
 *   2. 实现 CAN 数据帧的发送和接收
 *   3. 验证 CAN 通信链路
 *
 * 硬件连接：
 *   - CAN0_TX: PTD1 (F1 CAN)
 *   - CAN0_RX: PTD0 (F1 CAN)
 *   - CAN 收发器接 120Ω 终端电阻
 *
 * TODO 第11章学习后实现：
 *   1. 调用 flexcan_init() 初始化 CAN
 *   2. 在 while(1) 中发送 CAN 消息
 *   3. 检查是否有接收到的消息并处理
 *   4. LED 指示发送/接收状态
 */
#include "flexcan_driver.h"

/* ========== CAN 帧参数 ========== */
#define CAN_CH          CAN_CHANNEL_0
#define CAN_BITRATE     500000  /* 500 kbps */
#define CAN_MSG_ID      0x123

/**
 * @brief 主函数
 * @return 不会返回
 *
 * TODO §11.3：实现 CAN 自收发
 *   1. flexcan_init(CAN_CH, CAN_BITRATE) 初始化
 *   2. 构造发送帧:
 *      - id = CAN_MSG_ID
 *      - format = CAN_FRAME_STD
 *      - type = CAN_FRAME_DATA
 *      - dlc = 4
 *      - data = 递增计数器
 *   3. 在 while(1) 中:
 *      a. flexcan_send_msg() 发送
 *      b. flexcan_recv_msg() 自收自检
 *      c. 适当延时
 */
int main(void) {
    /* TODO：初始化 CAN */

    /* TODO：初始化 UART 用于调试输出 */

    /* TODO：初始化 LED 用于指示 */

    while (1) {
        /* TODO：发送 CAN 帧 */

        /* TODO：检查是否有接收帧 */

        /* TODO：延时 */
    }

    return 0;
}

/* ========================================================================
 * 启动代码 (Startup Code)
 * 提示：与 gpio/ 模块共用相同的启动框架
 * ======================================================================== */

/* TODO：从 gpio/main.c 复制启动代码 */
