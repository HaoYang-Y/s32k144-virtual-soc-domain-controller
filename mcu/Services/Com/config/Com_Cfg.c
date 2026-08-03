/**
 * @file    Com_Cfg.c
 * @brief   COM 模块配置实例 — 信号与 I-PDU 映射表
 *
 * @note    AUTOSAR 配置数据的特点:
 *          - 全部 const，编译后存入 ROM（Flash），不占 RAM
 *          - 由配置工具 (Vector DaVinci / EB tresos) 根据 ARXML 生成
 *          - 本工程手工编写以学习配置结构，数据源自 config/signals.yaml
 *
 *          I-PDU 0x123 (TX) 信号布局:
 *          ┌────────────┬──────────┬──────────┬──────────┐
 *          │  byte 0-3  │ byte 4   │ byte 5   │ byte 6-7 │
 *          ├────────────┼──────────┼──────────┼──────────┤
 *          │ TestTx     │ TestTx   │ TestTx   │ 未使用    │
 *          │ Counter    │ Magic0   │ Magic1   │          │
 *          │ 32-bit     │ 8-bit    │ 8-bit    │          │
 *          │ intel      │ intel    │ intel    │          │
 *          │ TRIGGERED  │ PENDING  │ NONE     │          │
 *          └────────────┴──────────┴──────────┴──────────┘
 *
 *          I-PDU 0x100 (RX) 信号布局:
 *          ┌────────────────────────────────────────────────┐
 *          │                 byte 0-7                       │
 *          ├────────────────────────────────────────────────┤
 *          │                 TestRxData                     │
 *          │                 64-bit intel                   │
 *          │                 (无发送属性, 接收信号)           │
 *          └────────────────────────────────────────────────┘
 *
 *          为什么要学会手写配置表？
 *          因为 AUTOSAR 通信栈的一切行为都由配置决定，不读配置表就看不懂代码。
 */

#include "Com_Cfg.h"

/* ===================================================================
 *  信号配置表
 *
 *  约定: 按 signal_id 升序排列，查找时可用二分索引（当前用线性搜索）。
 *
 *  信号布局说明 (AUTOSAR start_bit 约定):
 *    start_bit 从 0 开始编号，表示信号在 I-PDU 数据区的起始位位置。
 *    bit 0 = byte[0] 的 bit 0 (LSB)。
 *    例: start_bit=32 表示从 byte[4] 的 bit 0 开始。
 * =================================================================== */
const Com_SignalConfigType Com_SignalConfig[COM_SIGNAL_COUNT] = {

    /* ---- I-PDU 0x123 下的 3 个 TX 信号 ---- */

    {
        .signal_id         = COM_SIGNAL_ID_TEST_TX_COUNTER,
        .ipdu_id           = COM_IPDU_ID_TX_0x123,    /* 属于 I-PDU 0x123 */
        .direction         = COM_SEND,                /* TX 信号 */
        .bit_position      = 0U,                      /* 从 byte[0] bit0 开始 */
        .bit_size          = 32U,                     /* 占 4 字节 */
        .byte_order        = COM_BYTE_ORDER_INTEL,    /* LSB first */
        .is_signed         = 0U,                      /* 无符号 */
        .transfer_property = COM_TRANSFER_TRIGGERED,  /* 更新即发送 */
        .has_update_bit    = 0U,                      /* TX 信号通常不需要 Update Bit */
        .timeout_ms        = 0U                       /* TX 信号无需超时检测 */
    },
    {
        .signal_id         = COM_SIGNAL_ID_TEST_TX_MAGIC0,
        .ipdu_id           = COM_IPDU_ID_TX_0x123,
        .direction         = COM_SEND,
        .bit_position      = 32U,                     /* 从 byte[4] bit0 开始 */
        .bit_size          = 8U,                      /* 占 1 字节 */
        .byte_order        = COM_BYTE_ORDER_INTEL,
        .is_signed         = 0U,
        .transfer_property = COM_TRANSFER_PENDING,    /* 标记后 MainFunction 发 */
        .has_update_bit    = 0U,
        .timeout_ms        = 0U
    },
    {
        .signal_id         = COM_SIGNAL_ID_TEST_TX_MAGIC1,
        .ipdu_id           = COM_IPDU_ID_TX_0x123,
        .direction         = COM_SEND,
        .bit_position      = 40U,                     /* 从 byte[5] bit0 开始 */
        .bit_size          = 8U,                      /* 占 1 字节 */
        .byte_order        = COM_BYTE_ORDER_INTEL,
        .is_signed         = 0U,
        .transfer_property = COM_TRANSFER_NONE,       /* 仅周期发送，更新不触发 */
        .has_update_bit    = 0U,
        .timeout_ms        = 0U
    },

    /* ---- I-PDU 0x100 下的 1 个 RX 信号 ---- */

    {
        .signal_id         = COM_SIGNAL_ID_TEST_RX_DATA,
        .ipdu_id           = COM_IPDU_ID_RX_0x100,    /* 属于 I-PDU 0x100 */
        .direction         = COM_RECEIVE,             /* RX 信号 */
        .bit_position      = 0U,                      /* 从 byte[0] bit0 开始 */
        .bit_size          = 64U,                     /* 占 8 字节 */
        .byte_order        = COM_BYTE_ORDER_INTEL,
        .is_signed         = 0U,
        .transfer_property = COM_TRANSFER_NONE,       /* 接收信号无需触发发送 */
        .has_update_bit    = 1U,                      /* ★ RX 信号启用 Update Bit */
        .timeout_ms        = 3000U                    /* ★ 3 秒没收到数据 → TIMEOUT */
    },
};

/* ===================================================================
 *  I-PDU 配置表
 *
 *  cycle_time_ms 的含义:
 *    0    — 事件触发: 无周期发送，仅信号变化触发（或纯接收）
 *    >0   — 周期发送: MainFunction 按此间隔重复发送
 *
 *  I-PDU 组: 所有 I-PDU 属于 group 0 (COM_IPDU_GROUP_ALL)
 * =================================================================== */
const Com_IPduConfigType Com_IPduConfig[COM_IPDU_COUNT] = {

    {
        .ipdu_id       = COM_IPDU_ID_TX_0x123,
        .group_id      = COM_IPDU_GROUP_ALL,
        .dlc           = 6U,           /* 6 字节: Counter(4B)+Magic0(1B)+Magic1(1B) */
        .can_id        = 0x123U,       /* CAN ID (文档用途, 实际路由由 PduR 处理) */
        .cycle_time_ms = 500U          /* 每 500ms 周期发送一次 (保底机制) */
    },

    {
        .ipdu_id       = COM_IPDU_ID_RX_0x100,
        .group_id      = COM_IPDU_GROUP_ALL,
        .dlc           = 7U,           /* SF 单帧兼容: CAN 帧 1B PCI + 7B 数据 */
        .can_id        = 0x100U,
        .cycle_time_ms = 0U            /* 纯接收/事件触发，无周期发送 */
    },
};

/* ===================================================================
 *  配置查找函数
 *
 *  注: AUTOSAR 标准配置容器 (ComSignalId) 要求信号 ID 从 0 开始连续编号，
 *      可以直接用 signal_id 作为数组下标 O(1) 查找。
 *      这里提供线性搜索作为通用方案，同时支持 O(1) 快速路径。
 * =================================================================== */

/**
 * @brief 根据信号 ID 查找信号配置
 * @param signal_id  信号 ID
 * @return           配置指针，未找到返回 NULL
 */
const Com_SignalConfigType *Com_FindSignalConfig(Com_SignalIdType signal_id)
{
    /* O(1) 快速路径: 信号 ID 连续从 0 开始 */
    if (signal_id < COM_SIGNAL_COUNT) {
        return &Com_SignalConfig[signal_id];
    }
    return NULL;
}

/**
 * @brief 根据 I-PDU ID 查找 I-PDU 配置
 * @param ipdu_id  I-PDU ID
 * @return         配置指针，未找到返回 NULL
 */
const Com_IPduConfigType *Com_FindIPduConfig(Com_IPduIdType ipdu_id)
{
    /* O(1) 快速路径: I-PDU ID 连续从 0 开始 */
    if (ipdu_id < COM_IPDU_COUNT) {
        return &Com_IPduConfig[ipdu_id];
    }
    return NULL;
}
