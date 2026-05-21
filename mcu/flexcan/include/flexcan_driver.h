/*
 * @brief FlexCAN 驱动头文件 (S32K144)
 *        封装 CAN 模块的寄存器操作，提供 CAN 收发功能
 *
 * @note 学习目标：理解 CAN 总线协议和 FlexCAN 模块
 *       对应书籍：第11章 CAN 总线
 *
 * TODO：看书后实现以下函数
 *       1. flexcan_init()     — 初始化 FlexCAN 模块
 *       2. flexcan_set_bitrate() — 配置波特率
 *       3. flexcan_send_msg() — 发送 CAN 帧
 *       4. flexcan_recv_msg() — 接收 CAN 帧
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== FlexCAN 基地址 ========== */
/* TODO §11.2：从 S32K144 参考手册中找到 FlexCAN 寄存器基地址 */
#define CAN0_BASE       (0x40024000u)
#define CAN1_BASE       (0x40025000u)

/* ========== 标准帧/扩展帧 ID 最大值 ========== */
#define CAN_STDID_MAX   (0x7FFu)    /* 11 位标准帧 ID */
#define CAN_EXTID_MAX   (0x1FFFFFFFu) /* 29 位扩展帧 ID */

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
 *        1. 使能 CAN 时钟 (PCC_CAN[CGC])
 *        2. 软件复位 (MCR[SOFT_RST])
 *        3. 进入冻结模式 (MCR[FRZ] = 1, MCR[HALT] = 1)
 *        4. 配置 CAN 时序参数 (CBT / CTRL1)
 *        5. 退出冻结模式 (MCR[FRZ] = 0)
 *        6. 等待同步
 *
 * @param[in]  channel      CAN 通道 (0/1)
 * @param[in]  bitrate      波特率 (125k/250k/500k)
 * @return     0=成功, -1=参数错误
 *
 * TODO §11.2.2：实现 FlexCAN 初始化
 *       关键寄存器：
 *         - MCR:  模块控制 (SOFT_RST, FRZ, HALT, MDIS)
 *         - CTRL1: 时钟源、波特率分频、采样点
 *         - CBT:  位时序配置 (位段 1/2、跳跃宽度)
 *         - TIMER: 定时器
 */
int flexcan_init(can_channel_t channel, uint32_t bitrate);

/**
 * @brief 发送 CAN 数据帧（阻塞）
 *        将一帧数据写入发送缓冲区并等待发送完成
 *
 * @param[in]  channel   CAN 通道
 * @param[in]  frame     待发送的帧结构体
 * @return     0=成功, -1=发送超时
 *
 * TODO §11.2.3：实现 CAN 发送
 *       1. 查找空闲发送缓冲区 (IFLAG1[BUFxI] & 传输状态)
 *       2. 写 MBx (消息缓冲区):
 *          - CS:  控制状态 (IDE, RTR, DLC, CODE)
 *          - ID:  帧 ID
 *          - DATA: 数据域
 *       3. 请求发送: 写 MBx[CS][CODE] = 0b1000
 *       4. 等待发送完成: 查询 IFLAG1[BUFxI]
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
 * TODO §11.2.3：实现 CAN 接收
 *       1. 轮询 IFLAG1[BUFxI] 是否有接收完成标志
 *       2. 读取 MBx (消息缓冲区):
 *          - CS:  检查 CODE 是否为 0b0100 (已接收)
 *          - ID:  帧 ID
 *          - DATA: 数据域
 *       3. 清标志: IFLAG1[BUFxI] = 1
 *       4. 设置 MBx[CS][CODE] = 0b0100 (空，继续接收)
 */
int flexcan_recv_msg(can_channel_t channel, can_frame_t *frame);
