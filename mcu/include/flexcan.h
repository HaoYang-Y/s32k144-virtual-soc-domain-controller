/*
 * @brief FlexCAN 驱动头文件 (S32K144)
 *        封装基于 NXP S32 SDK 的 CAN 收发功能
 *
 * @note 本驱动使用 NXP S32 SDK FLEXCAN_DRV API 实现
 *       涉及的头文件：flexcan_driver.h
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== 标准帧/扩展帧 ID 最大值 ========== */
#define CAN_STDID_MAX   (0x7FFu)        /* 11 位标准帧 ID */
#define CAN_EXTID_MAX   (0x1FFFFFFFu)   /* 29 位扩展帧 ID */

/* ========== 枚举定义 ========== */

/** @brief CAN 通道 */
typedef enum {
    CAN_CHANNEL_0 = 0,
    CAN_CHANNEL_1 = 1,
} can_channel_t;

/** @brief CAN 帧格式 */
typedef enum {
    CAN_FRAME_STD = 0,   /**< 标准帧 (11 位 ID) */
    CAN_FRAME_EXT = 1,   /**< 扩展帧 (29 位 ID) */
} can_frame_format_t;

/** @brief CAN 帧类型 */
typedef enum {
    CAN_FRAME_DATA  = 0,  /**< 数据帧 */
    CAN_FRAME_REMOTE = 1, /**< 远程帧 */
} can_frame_type_t;

/* ========== CAN 帧结构体 ========== */

/** @brief CAN 消息帧结构体 */
typedef struct {
    uint32_t            id;          /**< 帧 ID (11/29 位) */
    can_frame_format_t  format;       /**< 帧格式: 标准/扩展 */
    can_frame_type_t    type;         /**< 帧类型: 数据/远程 */
    uint8_t             dlc;          /**< 数据长度 (0~8) */
    uint8_t             data[8];      /**< 数据域 */
} can_frame_t;

/* ========== 函数声明 ========== */

/**
 * @brief 初始化 FlexCAN 模块
 *        通过 NXP S32 SDK 配置 CAN 时钟、时序参数和波特率
 *
 * @param[in]  channel      CAN 通道 (0/1)
 * @param[in]  bitrate      波特率 (125k/250k/500k)
 * @return     0=成功, -1=参数错误
 *
 * @note SDK API: FLEXCAN_DRV_Init()
 *       SDK 配置结构体: flexcan_user_config_t
 */
int flexcan_init(can_channel_t channel, uint32_t bitrate);

/**
 * @brief 发送 CAN 数据帧（阻塞）
 *        通过 SDK API 将一帧数据写入发送缓冲区并等待发送完成
 *
 * @param[in]  channel   CAN 通道
 * @param[in]  frame     待发送的帧结构体
 * @return     0=成功, -1=发送超时
 *
 * @note SDK API: FLEXCAN_DRV_SendBlocking()
 */
int flexcan_send_msg(can_channel_t channel, const can_frame_t *frame);

/**
 * @brief 接收 CAN 帧（阻塞/非阻塞）
 *        检查是否有接收到的帧，有则读取
 *
 * @param[in]   channel   CAN 通道
 * @param[out]  frame     接收到的帧数据
 * @return      0=成功, -1=无数据
 *
 * @note SDK API: FLEXCAN_DRV_ReceiveBlocking() / FLEXCAN_DRV_GetReceiveMessage()
 */
int flexcan_recv_msg(can_channel_t channel, can_frame_t *frame);
