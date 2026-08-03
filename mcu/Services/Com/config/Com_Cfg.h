/**
 * @file    Com_Cfg.h
 * @brief   COM 模块配置 — 信号 ↔ I-PDU 映射表
 *
 * @note    AUTOSAR 核心概念：静态配置驱动
 *          - 每个信号属于一个 I-PDU（多个信号共享一个 CAN 帧）
 *          - 配置表在编译时确定，运行时只读
 *          - 信号在 I-PDU 中的位置由 bit_position + bit_size 定义
 *
 *          I-PDU (Interaction Layer PDU) = 一个 CAN 帧的数据载荷 (0~8 bytes)
 *          Signal = I-PDU 中的一个位段，是 SWC 操作的最小数据单元
 *
 *          AUTOSAR 信号→I-PDU 映射示例 (对应 config/signals.yaml):
 *
 *          I-PDU 0x123 (TX, DLC=8, 周期500ms):
 *          ┌──────────┬──────────┬──────────┐
 *          │ byte 0-3 │ byte 4   │ byte 5   │
 *          │ Counter  │ Magic0   │ Magic1   │
 *          │ 32-bit   │ 8-bit    │ 8-bit    │
 *          │ intel    │ intel    │ intel    │
 *          └──────────┴──────────┴──────────┘
 *
 *          I-PDU 0x100 (RX, DLC=8, 事件触发):
 *          ┌─────────────────────────────────┐
 *          │          byte 0-7               │
 *          │          RxData 64-bit          │
 *          │          intel                  │
 *          └─────────────────────────────────┘
 */

#ifndef COM_CFG_H
#define COM_CFG_H

#include "Std_Types.h"

/* ===================================================================
 *  基本类型定义 (AUTOSAR 标准: Com_SignalIdType, Com_IPduIdType)
 *
 *  放在 Com_Cfg.h 中是因为 Com_SignalConfigType 需要它们。
 *  Com.h 会 include Com_Cfg.h，所以上层也能使用这些类型。
 * =================================================================== */
typedef uint16_t Com_SignalIdType;
typedef uint16_t Com_IPduIdType;

/* ===================================================================
 *  信号 ID 枚举
 *
 *  AUTOSAR 中每个信号有全局唯一的 SignalId，由配置工具生成。
 *  这里手动定义，与 signals.yaml 保持一致。
 * =================================================================== */
#define COM_SIGNAL_ID_TEST_TX_COUNTER   ((Com_SignalIdType)0U)
#define COM_SIGNAL_ID_TEST_TX_MAGIC0    ((Com_SignalIdType)1U)
#define COM_SIGNAL_ID_TEST_TX_MAGIC1    ((Com_SignalIdType)2U)
#define COM_SIGNAL_ID_TEST_RX_DATA      ((Com_SignalIdType)3U)
#define COM_SIGNAL_COUNT                ((uint16_t)4U)

/* ===================================================================
 *  I-PDU ID 枚举
 *
 *  注意: I-PDU ID 必须与 PduR 路由表中使用的 PduIdType 一致。
 *        这里 I-PDU 0 对应 CanTp TP PDU 0，I-PDU 1 对应 CanTp TP PDU 1。
 * =================================================================== */
#define COM_IPDU_ID_TX_0x123   ((Com_IPduIdType)0U)
#define COM_IPDU_ID_RX_0x100   ((Com_IPduIdType)1U)
#define COM_IPDU_COUNT         ((uint16_t)2U)

/* ===================================================================
 *  I-PDU 组 ID 枚举
 *
 *  AUTOSAR IPduGroup 用于控制一组 I-PDU 的收发开关。
 *  例如: 诊断模式只收发诊断 PDU，应用模式只收发应用 PDU。
 *  当前只有一个组（全部 I-PDU），后续可按业务拆分。
 * =================================================================== */
#define COM_IPDU_GROUP_ALL     ((Com_IPduIdType)0U)
#define COM_IPDU_GROUP_COUNT   ((uint16_t)1U)

/* ===================================================================
 *  信号传输属性 (Transfer Property)
 *
 *  定义信号更新后如何触发 I-PDU 发送。AUTOSAR 标准 3 种模式:
 *
 *  TRIGGERED — 信号值改变后立即触发发送（不等待 MainFunction）
 *      ⚠ 如果多个信号同时改变且在同 I-PDU，它们共享一次发送
 *  PENDING   — 信号值改变后标记 dirty，由 Com_MainFunction 统一发送
 *      ✅ 天然批处理，同一周期内多次写只发一次
 *  NONE      — 信号改变不触发发送，I-PDU 仅按周期时间发送
 *      ✅ 纯周期信号，如心跳帧
 *
 *  一个 I-PDU 可能包含 3 种不同属性信号。发送规则:
 *    - 任一 TRIGGERED 信号改变 → 立即发送整个 I-PDU
 *    - 无 TRIGGERED 但有 PENDING 信号改变 → MainFunction 中发送
 *    - 所有信号都是 NONE → 仅按 cycle_time_ms 周期发送
 * =================================================================== */
typedef enum {
    COM_TRANSFER_TRIGGERED = 0,  /**< 信号变化立即发送 */
    COM_TRANSFER_PENDING   = 1,  /**< 信号变化标记，MainFunction 发送 */
    COM_TRANSFER_NONE      = 2   /**< 信号变化不触发发送，仅周期发送 */
} Com_TransferPropertyType;

/* ===================================================================
 *  信号方向
 * =================================================================== */
typedef enum {
    COM_SEND = 0,     /**< SWC → COM → CAN 总线 (发送) */
    COM_RECEIVE = 1   /**< CAN 总线 → COM → SWC (接收) */
} Com_SignalDirectionType;

/* ===================================================================
 *  字节序
 *
 *  Intel (LITTLE_ENDIAN):  低有效位在低地址字节 — S32K144 是
 *  Motorola (BIG_ENDIAN):  高有效位在高地址字节
 *
 *  例: 信号值 0x0102 (16-bit), start_bit=0, LENGTH=16
 *    Intel:    byte[0]=0x02, byte[1]=0x01  (LSB first)
 *    Motorola: byte[0]=0x01, byte[1]=0x02  (MSB first)
 * =================================================================== */
typedef enum {
    COM_BYTE_ORDER_INTEL    = 0,  /**< Intel 格式 (小端, LSB first) */
    COM_BYTE_ORDER_MOTOROLA = 1   /**< Motorola 格式 (大端, MSB first) */
} Com_ByteOrderType;

/* ===================================================================
 *  配置结构体
 * =================================================================== */

/**
 * @brief 信号配置 (AUTOSAR ComSignal)
 *
 * 描述一个信号在 I-PDU 内的位布局和传输行为。
 */
typedef struct {
    Com_SignalIdType        signal_id;          /**< 信号全局唯一 ID */
    Com_IPduIdType          ipdu_id;            /**< 所属 I-PDU ID */
    Com_SignalDirectionType direction;          /**< 发送/接收 */
    uint8_t                 bit_position;       /**< 信号在 I-PDU 中的起始位 */
    uint8_t                 bit_size;           /**< 信号位宽 (1~64) */
    Com_ByteOrderType       byte_order;         /**< Intel / Motorola */
    uint8_t                 is_signed;          /**< 0=无符号, 1=有符号 */
    Com_TransferPropertyType transfer_property; /**< 传输属性 */
    uint8_t                 has_update_bit;     /**< 1=启用更新标志位 (Update Bit) */
    uint16_t                timeout_ms;         /**< 信号超时 (ms), 0=不检测 */
} Com_SignalConfigType;

/**
 * @brief I-PDU 配置 (AUTOSAR ComIPdu)
 *
 * 描述一个 I-PDU 的 CAN 帧属性和发送周期。
 */
typedef struct {
    Com_IPduIdType  ipdu_id;        /**< I-PDU 全局唯一 ID */
    Com_IPduIdType  group_id;       /**< 所属 I-PDU 组 */
    uint8_t         dlc;            /**< CAN 帧数据长度 (0~8 bytes) */
    uint32_t        can_id;         /**< CAN 报文 ID (仅文档用途，实际路由由 PduR 负责) */
    uint16_t        cycle_time_ms;  /**< 周期发送间隔 (ms)，0=事件触发无周期发送 */
} Com_IPduConfigType;

/* ===================================================================
 *  配置实例声明 (定义在 Com_Cfg.c)
 * =================================================================== */
extern const Com_SignalConfigType Com_SignalConfig[COM_SIGNAL_COUNT];
extern const Com_IPduConfigType   Com_IPduConfig[COM_IPDU_COUNT];

/* ===================================================================
 *  配置查找辅助宏 / 函数
 * =================================================================== */

/**
 * @brief 根据信号 ID 查找信号配置
 * @param signal_id  信号 ID
 * @return           配置指针，未找到返回 NULL
 */
const Com_SignalConfigType *Com_FindSignalConfig(Com_SignalIdType signal_id);

/**
 * @brief 根据 I-PDU ID 查找 I-PDU 配置
 * @param ipdu_id  I-PDU ID
 * @return         配置指针，未找到返回 NULL
 */
const Com_IPduConfigType *Com_FindIPduConfig(Com_IPduIdType ipdu_id);

#endif /* COM_CFG_H */
