/**
 * @file    Com.h
 * @brief   [AUTOSAR CP] Communication 模块 — Services 层
 *
 * @note    信号级通信：将 SWC 的信号值打包/解包为 I-PDU
 *
 *          AUTOSAR COM 层职责:
 *          - 信号 ↔ I-PDU 编解码 (Com_SendSignal / Com_ReceiveSignal)
 *          - I-PDU 发送时机管理 (周期 / 事件触发)
 *          - IPduGroup 通信模式控制 (Start / Stop)
 *          - RX 接收回调 + TX 发送确认
 *
 *          在通信栈中的位置:
 *            SWC (Rte_Write/Read) → COM → PduR → CanTp → CanIf → CAN Driver
 *
 *          COM 层术语:
 *          - Signal: SWC 操作的最小数据单元，如"车速信号" (16-bit, 0.01km/h)
 *          - I-PDU:  一组信号的集合，映射到一个 CAN 帧 (0~8 bytes)
 *          - Shadow Buffer: 信号的缓存副本，用于收发解耦
 *          - Dirty Flag: 标记 I-PDU 数据已改变，需要发送
 */

#ifndef COM_H
#define COM_H

#include "Std_Types.h"
#include "ComStack_Types.h"        /* PduInfoType, PduIdType, PduLengthType */
#include "Com_Cfg.h"               /* Com_SignalIdType, Com_IPduIdType, 配置结构 */

/* ===================================================================
 *  类型定义
 *
 *  注: Com_SignalIdType 和 Com_IPduIdType 定义在 Com_Cfg.h 中，
 *      本文件通过 include Com_Cfg.h 获得。
 * =================================================================== */

/** @brief 信号值结构（通用，void* 传递） */
typedef struct {
    uint8_t  *data;
    uint16_t  length;
} Com_SignalType;

/* ===================================================================
 *  API 函数声明
 * =================================================================== */

/**
 * @brief 初始化 COM 模块
 *
 * 初始化所有 I-PDU buffer 和信号 shadow buffer。
 * 由 EcuM_Init 在 Services 层初始化阶段调用。
 */
void Com_Init(void);

/**
 * @brief COM 模块 MainFunction（周期调用）
 *
 * 由 EcuM_MainFunction 周期驱动，负责:
 * - 检查 I-PDU 周期发送时间
 * - 发送 PENDING 属性的 dirty I-PDU
 * - 超时检测
 */
void Com_MainFunction(void);

/**
 * @brief 发送信号 (SWC → COM → CAN 总线)
 *
 * AUTOSAR 核心 API: SWC 通过 RTE 调用此函数发送一个信号值。
 *
 * 函数行为:
 * 1. 查找信号配置表，获取 bit_position / bit_size / byte_order
 * 2. 将信号值打包到所属 I-PDU buffer 的指定位段
 * 3. 根据 Transfer Property 决定何时发送:
 *    - TRIGGERED: 立即调用 PduR_ComTransmit (需 MainFunction 环境)
 *    - PENDING:   标记 I-PDU dirty，等待 MainFunction 统一发送
 *    - NONE:      仅写入 buffer，不触发发送
 *
 * @param SignalId   信号 ID (见 Com_Cfg.h 中 COM_SIGNAL_ID_*)
 * @param SignalData 指向信号数据的指针 (类型由信号配置决定)
 */
void Com_SendSignal(Com_SignalIdType SignalId, const void *SignalData);

/**
 * @brief 接收信号 (CAN 总线 → COM → SWC)
 *
 * AUTOSAR 核心 API: SWC 通过 RTE 调用此函数读取一个接收到的信号值。
 *
 * @param SignalId   信号 ID
 * @param SignalData 输出缓冲区指针，用于存放信号值
 */
void Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalData);

/**
 * @brief 发送信号组
 *
 * 信号组 = 一组语义相关的信号 (如 "门的 4 个状态传感器")。
 * 调用此函数将所有组内信号的值打包到各自 I-PDU 并触发发送。
 *
 * @param GroupId  信号组 ID
 * @return         0=成功, 非0=失败
 */
uint8_t Com_SendSignalGroup(Com_SignalIdType GroupId);

/**
 * @brief 接收信号组
 *
 * @param GroupId  信号组 ID
 * @return         0=成功, 非0=失败
 */
uint8_t Com_ReceiveSignalGroup(Com_SignalIdType GroupId);

/**
 * @brief 启动 I-PDU 组（使能通信）
 *
 * 启动指定 IPduGroup 的收发功能。
 * 如: Com_IPduGroupStart(诊断组) — 启用诊断 CAN 通信。
 *
 * @param GroupId  I-PDU 组 ID
 */
void Com_IPduGroupStart(Com_IPduIdType GroupId);

/**
 * @brief 停止 I-PDU 组（停止通信）
 *
 * 停止指定 IPduGroup 的收发功能。
 * 停止后该组的周期发送和事件触发均被禁用。
 *
 * @param GroupId  I-PDU 组 ID
 */
void Com_IPduGroupStop(Com_IPduIdType GroupId);

/* ===================================================================
 *  信号状态 API (AUTOSAR Update Bit + Timeout)
 * =================================================================== */

/**
 * @brief 信号状态枚举
 *
 * COM_SIGNAL_OK      — 信号正常（数据新鲜，未超时）
 * COM_SIGNAL_TIMEOUT — 信号超时（超过 timeout_ms 未收到新数据）
 */
typedef enum {
    COM_SIGNAL_OK      = 0,  /**< 信号正常 */
    COM_SIGNAL_TIMEOUT = 1   /**< 信号超时 */
} Com_SignalStatusType;

/**
 * @brief 获取信号更新标志位 (Update Bit)
 *
 * AUTOSAR 核心模式: SWC 不直接比较信号值，而是轮询 Update Bit
 * 来判断是否有新数据到达。这样做的优势:
 *   - 避免重复处理同一数据
 *   - 避免漏掉变化后又变回原值的"闪变"
 *   - 状态机驱动的处理方式，符合 AUTOSAR 运行模型
 *
 * 调用约定: 本函数读取后自动清 0（下次收新数据再置 1）。
 *
 * @param SignalId  信号 ID
 * @return          1=有新数据, 0=无新数据
 */
uint8_t Com_GetUpdateBit(Com_SignalIdType SignalId);

/**
 * @brief 获取信号超时状态
 *
 * AUTOSAR Deadline Monitoring: RX 信号如果超过配置的 timeout_ms
 * 未收到新数据，状态变为 COM_SIGNAL_TIMEOUT。
 * SWC 应在每次读信号前先检查状态。
 *
 * @param SignalId  信号 ID
 * @return          COM_SIGNAL_OK 或 COM_SIGNAL_TIMEOUT
 */
Com_SignalStatusType Com_GetSignalStatus(Com_SignalIdType SignalId);

/* ===================================================================
 *  回调接口 (被 PduR 调用)
 * =================================================================== */

/** @brief 接收指示回调: PduR 收到 I-PDU 后通知 COM */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);

/** @brief 发送确认回调: PduR 完成 I-PDU 发送后通知 COM */
void Com_TxConfirmation(PduIdType TxPduId);

#endif /* COM_H */
