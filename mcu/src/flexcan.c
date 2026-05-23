/*
 * @brief FlexCAN 驱动实现 (S32K144)
 *        基于 NXP S32 SDK FLEXCAN_DRV API 实现 CAN 收发
 *
 * @note 本驱动封装 NXP S32 SDK 的 FlexCAN 驱动层 API
 *
 *       SDK API 涉及:
 *         - FLEXCAN_DRV_Init():         初始化 CAN 模块
 *         - FLEXCAN_DRV_SendBlocking(): 阻塞发送
 *         - FLEXCAN_DRV_ReceiveBlocking(): 阻塞接收
 *         - FLEXCAN_DRV_GetReceiveMessage(): 非阻塞接收
 *
 *       SDK 配置结构体:
 *         - flexcan_user_config_t:  包含波特率、时序参数、中断配置等
 *         - flexcan_message_t:      SDK 定义的 CAN 帧结构体
 */
#include "flexcan.h"

/* NXP S32 SDK FlexCAN 驱动头文件 */
#include "flexcan_driver.h"

/* NXP S32 SDK 时钟管理头文件 (用于使能 CAN 时钟) */
#include "clock_manager.h"

/**
 * @brief 将自定义 can_frame_t 转换为 SDK flexcan_message_t
 *
 * @param[in]   src   应用层帧结构体
 * @param[out]  dst   SDK 帧结构体
 */
static void frame_to_sdk_msg(const can_frame_t *src,
                             flexcan_message_t *dst)
{
    uint8_t i;

    dst->msgId          = src->id;
    dst->isRemote       = (src->type == CAN_FRAME_REMOTE) ? true : false;
    dst->isExtended     = (src->format == CAN_FRAME_EXT) ? true : false;
    dst->dataLen        = (src->dlc > 8u) ? 8u : src->dlc;
    dst->timestamp      = 0u;
    dst->cs             = 0u;
    dst->fdFlags        = 0u;
    dst->fdDataLen      = 0u;
    dst->isFlexibleData = false;

    /* 拷贝数据 */
    for (i = 0u; i < dst->dataLen; i++)
    {
        dst->data[i] = src->data[i];
    }

    /* 填充剩余字节为 0 */
    for (i = dst->dataLen; i < 8u; i++)
    {
        dst->data[i] = 0u;
    }
}

/**
 * @brief 将 SDK flexcan_message_t 转换为自定义 can_frame_t
 *
 * @param[in]   src   SDK 帧结构体
 * @param[out]  dst   应用层帧结构体
 */
static void sdk_msg_to_frame(const flexcan_message_t *src,
                             can_frame_t *dst)
{
    uint8_t i;

    dst->id       = src->msgId;
    dst->format   = src->isExtended ? CAN_FRAME_EXT : CAN_FRAME_STD;
    dst->type     = src->isRemote ? CAN_FRAME_REMOTE : CAN_FRAME_DATA;
    dst->dlc      = src->dataLen;

    /* 拷贝数据 */
    for (i = 0u; i < dst->dlc; i++)
    {
        dst->data[i] = src->data[i];
    }
}

/* ========== 函数实现 ========== */

int flexcan_init(can_channel_t channel, uint32_t bitrate)
{
    flexcan_user_config_t canConfig;
    uint32_t ret;

    /* 参数检查 */
    if ((channel > CAN_CHANNEL_1) || (bitrate == 0u))
    {
        return -1;
    }

    /*
     * 使用 SDK 默认配置初始化 flexcan_user_config_t
     * SDK 提供 FLEXCAN_DRV_GetDefaultConfig() 填充默认值
     */
    FLEXCAN_DRV_GetDefaultConfig(&canConfig);
    canConfig.bitrate = bitrate;

    /*
     * 调用 SDK API 初始化 CAN 模块
     * 内部自动完成:
     *   1. 使能 PCC_CANn 时钟
     *   2. 进入冻结模式 (FRZ | HALT)
     *   3. 配置 MCR 寄存器
     *   4. 配置 CTRL1/CBT 寄存器 (位时序)
     *   5. 配置消息缓冲区
     *   6. 退出冻结模式
     */
    ret = FLEXCAN_DRV_Init((uint32_t)channel, &canConfig);
    if (ret != 0u)
    {
        return -1;
    }

    return 0;
}

int flexcan_send_msg(can_channel_t channel, const can_frame_t *frame)
{
    flexcan_message_t sdkMsg;
    uint32_t ret;

    /* 参数检查 */
    if ((channel > CAN_CHANNEL_1) || (frame == NULL))
    {
        return -1;
    }

    /* 将应用层帧结构体转换为 SDK 格式 */
    frame_to_sdk_msg(frame, &sdkMsg);

    /*
     * 调用 SDK API 阻塞发送
     * SDK 内部自动处理:
     *   1. 查找空闲发送缓冲区
     *   2. 写入 MB (ID / 数据 / CS CODE)
     *   3. 等待发送完成
     *   4. 清除中断标志
     */
    ret = FLEXCAN_DRV_SendBlocking((uint32_t)channel, &sdkMsg,
                                   10000u);  /* 10ms 超时 */
    if (ret != 0u)
    {
        return -1;
    }

    return 0;
}

int flexcan_recv_msg(can_channel_t channel, can_frame_t *frame)
{
    flexcan_message_t sdkMsg;
    uint32_t ret;

    /* 参数检查 */
    if ((channel > CAN_CHANNEL_1) || (frame == NULL))
    {
        return -1;
    }

    /*
     * 调用 SDK API 阻塞接收
     * SDK 内部自动处理:
     *   1. 轮询 IFLAG1 检查接收完成
     *   2. 读取 MB (CS / ID / 数据)
     *   3. 清除中断标志
     *   4. 将 MB 重新设置为 RX_EMPTY
     */
    ret = FLEXCAN_DRV_ReceiveBlocking((uint32_t)channel, &sdkMsg,
                                      10000u);  /* 10ms 超时 */
    if (ret != 0u)
    {
        return -1;
    }

    /* 将 SDK 帧结构体转换为应用层格式 */
    sdk_msg_to_frame(&sdkMsg, frame);

    return 0;
}
