/**
 * @file    Com.c
 * @brief   [AUTOSAR CP] Communication 模块实现 — Services 层
 *
 * @note    信号级通信：SWC ↔ I-PDU 编解码 + 发送时机管理
 *
 *          ── AUTOSAR 学习笔记 ──
 *
 *          1. 静态配置驱动
 *             所有行为由 Com_Cfg.h 中的配置表决定，运行时零动态分配。
 *             这是 AUTOSAR 与 Linux 应用编程最大的差异——没有 malloc。
 *
 *          2. Shadow Buffer 模式
 *             Com_SendSignal 不直接发送，而是:
 *               a) 将信号值打包到 I-PDU byte buffer
 *               b) 标记 I-PDU dirty
 *               c) 由 Com_MainFunction 统一发送（或事件立即触发）
 *             这解耦了 SWC 写信号的时机和 CAN 总线发送的时机。
 *
 *          3. MainFunction 驱动 vs OS 线程
 *             AUTOSAR CP 没有抢占式线程。所有"并发"都是状态机，
 *             由 EcuM 周期调用各模块的 MainFunction 推进。
 *             Com_MainFunction 在这里扮演"发送调度器"的角色。
 *
 *          4. 分层回调链
 *             发送: COM → PduR_ComTransmit → CanTp → CanIf → Can
 *             接收: Can ISR → CanIf → CanTp → PduR → Com_RxIndication
 *             确认: TX ISR → CanIf → CanTp → PduR → Com_TxConfirmation
 *             每层只关心自己的抽象（信号/I-PDU/N-PDU/CAN帧）。
 *
 *          5. 信号布局 (I-PDU = 最多 8 字节的 CAN 数据载荷)
 *
 *             I-PDU 0x123 (TX, DLC=8):
 *             ┌──────────┬──────────┬──────────┬──────────┐
 *             │ byte 0-3 │ byte 4   │ byte 5   │ byte 6-7 │
 *             ├──────────┼──────────┼──────────┼──────────┤
 *             │ Counter  │ Magic0   │ Magic1   │  unused  │
 *             │ 32-bit   │ 8-bit    │ 8-bit    │          │
 *             │ TRIGGERED│ PENDING  │ NONE     │          │
 *             └──────────┴──────────┴──────────┴──────────┘
 *
 *             发送优先级: Counter 更新→立即发送，
 *                        Magic0 更新→等 MainFunction,
 *                        Magic1 更新→不触发发送，等 500ms 周期发送
 *
 *             I-PDU 0x100 (RX, DLC=8):
 *             ┌────────────────────────────────────────────────┐
 *             │                 byte 0-7                       │
 *             ├────────────────────────────────────────────────┤
 *             │              RxData (64-bit)                   │
 *             └────────────────────────────────────────────────┘
 *
 *
 *          ═══════════  A U T O S A R   C P   学 习 重 点  ═══════════
 *
 *          位编码 (Bit Encoding):
 *          ─────────────────────
 *          CAN 帧是字节数组，信号不一定从字节边界开始。
 *          AUTOSAR 用 start_bit + bit_size 描述信号在一组字节中的位置。
 *
 *          start_bit 含义 (AUTOSAR 约定):
 *            start_bit 从 0 开始，是信号最低有效位 (LSB) 的位位置。
 *            bit 0 = byte[0] 的 bit 0 (0x01)。
 *            bit 7 = byte[0] 的 bit 7 (0x80)。
 *            bit 8 = byte[1] 的 bit 0。
 *
 *          Intel (Little-Endian) 格式:
 *            信号值按 LSB-first 存储。位 i 在 bit position = start_bit + i。
 *            例: 0x01020304 (32-bit), start_bit=0
 *              byte[0] = 0x04  (bit 0-7)
 *              byte[1] = 0x03  (bit 8-15)
 *              byte[2] = 0x02  (bit 16-23)
 *              byte[3] = 0x01  (bit 24-31)
 *            S32K144 是小端处理器，Intel 格式直接匹配 CPU 字节序。
 *
 *          Motorola (Big-Endian) 格式:
 *            信号值按 MSB-first 跨字节存储。
 *            例: 0x01020304 (32-bit), start_bit=0
 *              byte[0] = 0x01  (bit 0-7, 存储 MSB)
 *              byte[1] = 0x02
 *              byte[2] = 0x03
 *              byte[3] = 0x04
 *
 *          重要: Intel/Motorola 差异仅在跨字节时有意义。
 *          单字节信号 (length ≤ 8) 两种格式结果相同！
 *
 *          本例中所有信号都是 Intel 格式，但代码同时支持两种。
 */

#include "Com.h"
#include "Com_Cfg.h"
#include "ComStack_Types.h"
#include "PduR.h"
#include "Det.h"
#include "Log.h"
#include <string.h>

/* ===================================================================
 *  内部常量
 * =================================================================== */

/** @brief Com_MainFunction 调用周期 (ms)
 *
 * 当前裸机主循环 ~1ms/次，EcuM_MainFunction 每循环调用一次。
 * 所以 Com_MainFunction 实际频率 ≈ 1kHz，每次推进 1ms。
 * 正式 AUTOSAR 系统中 MainFunction 由 OS 定时器以固定周期调度。
 *
 * 周期计算: cycle_time_ms=500 → 每 500 次 MainFunction 发一次 (~500ms)
 */
#define COM_MAINFUNCTION_PERIOD_MS  1U

/** @brief I-PDU DLC 最大值 */
#define COM_IPDU_MAX_DATA_LEN       8U

/** @brief 信号 Shadow Buffer 最大值 (单位: bytes, 对应 64-bit 信号) */
#define COM_SIGNAL_MAX_BYTES        8U

/* ===================================================================
 *  I-PDU 内部运行时状态
 *
 *  每个 I-PDU 在运行时维护:
 *  - data[8]:   数据字节数组 (CAN 帧载荷)
 *  - last_tx_ms: 上次发送时的系统时间 (ms)，用于周期判断
 *  - dirty:     数据已被信号写入但尚未发送 (1=需要发送)
 *  - enabled:   IPduGroup 开关状态 (1=允许收发)
 * =================================================================== */
typedef struct {
    uint8_t  data[COM_IPDU_MAX_DATA_LEN]; /**< I-PDU 数据缓冲区 */
    uint32_t last_tx_ms;                  /**< 上次发送时间 (ms) */
    uint8_t  dirty;                       /**< 脏标记: 1=有未发送更新 */
    uint8_t  enabled;                     /**< 使能标记: 1=允许收发 */
} Com_IPduBufferType;

/* ===================================================================
 *  全局状态
 * =================================================================== */

/** @brief 系统时间计数器 (ms)
 *
 * 没有 RTOS 时，由 Com_MainFunction 以固定周期推进。
 * 计数精度 = COM_MAINFUNCTION_PERIOD_MS (10ms)。 */
static uint32_t Com_SystemTimeMs = 0U;

/** @brief I-PDU 缓冲区数组，按 IPDU ID 索引 */
static Com_IPduBufferType Com_IPduBuffers[COM_IPDU_COUNT];

/** @brief 信号 Shadow Buffer
 *  存储每个信号的上次写入/接收值 (最大 64-bit)。
 *  Com_SendSignal 写 → Com_ReceiveSignal 读。 */
static uint64_t Com_SignalValues[COM_SIGNAL_COUNT];

/** @brief 信号时间戳 (上次更新时的系统时间) */
static uint32_t Com_SignalTimestamps[COM_SIGNAL_COUNT];

/**
 * @brief 信号更新标志位 (Update Bit)
 *
 * AUTOSAR 核心模式: 每收到/写入新数据时置 1，SWC 通过
 * Com_GetUpdateBit 读取并清 0。SWC 轮询此位而不是直接比较值。
 */
static uint8_t Com_UpdateBits[COM_SIGNAL_COUNT];

/**
 * @brief 信号超时状态 (Deadline Monitoring)
 *
 * 由 Com_MainFunction 更新: 如果信号距上次更新超过 timeout_ms，
 * 标记为 COM_SIGNAL_TIMEOUT。
 */
static Com_SignalStatusType Com_SignalStatus[COM_SIGNAL_COUNT];

/** @brief COM 模块是否已初始化 (供 DET 检查用) */
static uint8_t Com_IsInitialized = 0U;

/* ===================================================================
 *  内部辅助函数声明
 * =================================================================== */

/**
 * @brief 将信号值打包到 I-PDU 字节数组 (位操作核心)
 *
 * 这是 COM 层最核心的算法——将信号值写入 CAN 帧的指定位段。
 *
 * 算法 (INTEL 格式):
 *   for i = 0 to bit_size-1:
 *       target_bit = bit_position + i
 *       byte_idx   = target_bit / 8
 *       bit_idx    = target_bit % 8
 *       将 signal_value 的第 i 位写入 buffer[byte_idx] 的 bit_idx
 *
 * 算法 (MOTOROLA 格式):
 *   for i = 0 to bit_size-1:
 *       byte_offset_from_end = (bit_size - 1 - i) / 8
 *       bit_within_byte = ...  (见实现)
 *       因为 MSB 在最低地址字节，需要反序处理
 *
 * @param signal  信号配置 (bit_position, bit_size, byte_order, is_signed)
 * @param value   信号值
 * @param buffer  目标 I-PDU 字节数组 (输出)
 */
static void Com_PackSignal(const Com_SignalConfigType *signal,
                           uint64_t value, uint8_t *buffer);

/**
 * @brief 从 I-PDU 字节数组提取信号值 (解包)
 *
 * Com_PackSignal 的逆操作。
 *
 * @param signal  信号配置
 * @param buffer  源 I-PDU 字节数组
 * @return        提取出的信号值 (uint64_t 通用, 由调用者按实际大小使用)
 */
static uint64_t Com_UnpackSignal(const Com_SignalConfigType *signal,
                                 const uint8_t *buffer);

/**
 * @brief 根据 IPDU ID 查找该 I-PDU 下所有信号的信号 ID
 *
 * 用于 RX 路径: 收到 I-PDU 后解包其中所有信号。
 *
 * @param ipdu_id      I-PDU ID
 * @param signal_ids   输出数组，存放属于此 I-PDU 的所有信号 ID
 * @param max_count    数组容量
 * @return             找到的信号数量
 */
static uint8_t Com_GetSignalsInIPdu(Com_IPduIdType ipdu_id,
                                    Com_SignalIdType *signal_ids,
                                    uint8_t max_count);

/**
 * @brief 发送指定 I-PDU（内部函数）
 *
 * 构建 PduInfoType → 调用 PduR_ComTransmit。
 * 调用此函数前，I-PDU data[] 必须已完成信号打包。
 *
 * @param ipdu  I-PDU buffer 指针
 * @param cfg   I-PDU 配置指针
 */
static void Com_TriggerIPduSend(Com_IPduBufferType *ipdu,
                                const Com_IPduConfigType *cfg);

/* ===================================================================
 *  实现: 信号编解码引擎
 * =================================================================== */

/**
 * @brief 将信号值打包到 I-PDU 字节数组
 *
 * 算法详解:
 *
 *   Intel 格式 (bit-by-bit LSB first):
 *     以 start_bit=0, length=32, value=0x01020304 为例:
 *       bit 0  (0x04 bit0) → byte[0] bit 0 = buffer[0] |= 0x01
 *       bit 1  (0x04 bit1) → byte[0] bit 1 = buffer[0] |= 0x02
 *       ...
 *       bit 7  (0x04 bit7) → byte[0] bit 7 = buffer[0] |= 0x80
 *       bit 8  (0x03 bit0) → byte[1] bit 0 = buffer[1] |= 0x01
 *       ...
 *       bit 24 (0x01 bit0) → byte[3] bit 0 = buffer[3] |= 0x01
 *       ...
 *       bit 31 (0x01 bit7) → byte[3] bit 7 = buffer[3] |= 0x80
 *     结果: buffer[0]=0x04, [1]=0x03, [2]=0x02, [3]=0x01 ✓
 *
 *   Motorola 格式:
 *     先计算信号的"字节矩形"范围，跨字节时 MSB→低地址。
 *     例: start_bit=0, length=32, value=0x01020304
 *       MSB byte (0x01) → buffer[0]  (最低地址)
 *       ...
 *       LSB byte (0x04) → buffer[3]  (最高地址)
 *     结果: buffer[0]=0x01, [1]=0x02, [2]=0x03, [3]=0x04 ✓
 *
 *     非字节对齐 Motorola 例: start_bit=14, length=12
 *       start_byte = 14/8 = 1, bit in byte = 14%8 = 6
 *       信号占 byte[1] bits 6-7, byte[2] bits 0-7, byte[3] bits 0-1
 *       byte[1] bit 7 = MSB, byte[3] bit 0 = LSB
 */
static void Com_PackSignal(const Com_SignalConfigType *signal,
                           uint64_t value, uint8_t *buffer)
{
    uint8_t  bit_size   = signal->bit_size;
    uint8_t  start_bit  = signal->bit_position;
    uint16_t i;

    if (bit_size == 0U || buffer == NULL) {
        return;
    }

    /* 掩码: 只保留 bit_size 位有效数据 */
    if (bit_size < 64U) {
        value &= ((1ULL << bit_size) - 1ULL);
    }

    if (signal->byte_order == COM_BYTE_ORDER_INTEL) {
        /* ================================================================
         *  Intel (Little-Endian) 打包 — LSB first
         *
         *  位 i 映射到 buffer 位置: (start_bit + i)
         *  线程安全: 只操作 buffer 的对应位，不影响其他位
         * ================================================================ */
        for (i = 0U; i < bit_size; i++) {
            uint16_t target_bit      = (uint16_t)start_bit + i;
            uint8_t  target_byte     = (uint8_t)(target_bit >> 3U);   /* /8 */
            uint8_t  target_bit_mask = (uint8_t)(1U << (target_bit & 0x07U)); /* 1<<(%8) */

            if ((value >> i) & 1ULL) {
                buffer[target_byte] |= target_bit_mask;
            } else {
                buffer[target_byte] &= (uint8_t)(~target_bit_mask);
            }
        }
    } else {
        /* ================================================================
         *  Motorola (Big-Endian) 打包 — MSB first
         *
         *  与 Intel 的区别: 字节跨越时的字节顺序相反。
         *  信号 bit 15 (MSB) → 低地址字节
         *  信号 bit 0  (LSB) → 高地址字节 (start_bit 所在位置)
         *
         *  位 i 的映射: 从 MSB 开始计数，MSB 去低地址字节的最高有效位。
         *
         *  算法:
         *    对每个 i (0 = LSB):
         *      bit_from_msb = length - 1 - i  (该位是从 MSB 起第几位)
         *      字节偏移 = byte_in_group - (bit_from_msb / 8) - 1
         *      其中 byte_in_group = (start_bit + length) / 8 (向上取整)
         *
         *  简化实现: 逐位映射
         *    - 计算信号占据的总字节范围
         *    - 对每个位, 确定它属于哪个字节的哪个位置
         *
         *  跨字节 Motorola 的关键认知:
         *    信号 MSB 在 低地址字节的高 bit 位
         *    信号 LSB 在 start_bit 位置
         *    byte[low_addr] bit7 = signal MSB
         *    byte[low_addr] bit6 = signal MSB-1
         *    ...
         * ================================================================ */
        uint8_t start_byte = (uint8_t)(start_bit >> 3U);   /* /8 */
        uint8_t end_bit    = start_bit + bit_size - 1U;
        uint8_t end_byte   = (uint8_t)(end_bit >> 3U);

        for (i = 0U; i < bit_size; i++) {
            /* bit i 从 LSB 算起, 它对应的 Motorola 排列位置 */
            uint8_t bit_from_msb = (uint8_t)(bit_size - 1U - i);

            /* 确定该位所在的字节 */
            uint8_t byte_idx = end_byte - (bit_from_msb >> 3U);

            /* 确定该位在该字节中的位置 */
            uint8_t bit_in_byte;
            if (byte_idx == start_byte) {
                /* 第一个字节: 从 (start_bit % 8) 开始填到 bit 7 */
                uint8_t start_bit_in_byte = (uint8_t)(start_bit & 0x07U);
                uint8_t bits_in_this_byte = 8U - start_bit_in_byte;
                uint8_t local_bit = bit_from_msb;
                if (local_bit < bits_in_this_byte) {
                    /* 在第一个字节的高位 */
                    bit_in_byte = start_bit_in_byte + bits_in_this_byte - 1U - local_bit;
                } else {
                    /* 不在这字节, 后续再处理 */
                    continue;
                }
            } else if (byte_idx == end_byte) {
                /* 最后一个字节: 从 bit 0 开始填 */
                uint8_t local_bit = bit_from_msb;
                uint8_t prev_bytes_bits = (uint8_t)((end_byte - start_byte) * 8U
                                          - (start_bit & 0x07U));
                if (local_bit >= prev_bytes_bits) {
                    bit_in_byte = (local_bit - prev_bytes_bits);
                } else {
                    continue;
                }
            } else {
                /* 中间字节: 填满 bit 7→0 */
                uint8_t start_bit_in_byte = (uint8_t)(start_bit & 0x07U);
                uint8_t offset_in_group = bit_from_msb - (8U - start_bit_in_byte)
                                          - (uint8_t)((end_byte - byte_idx - 1U) * 8U);
                bit_in_byte = 7U - (offset_in_group & 0x07U);
            }

            uint8_t target_bit_mask = (uint8_t)(1U << bit_in_byte);

            if ((value >> i) & 1ULL) {
                buffer[byte_idx] |= target_bit_mask;
            } else {
                buffer[byte_idx] &= (uint8_t)(~target_bit_mask);
            }
        }
    }
}

/**
 * @brief 从 I-PDU 字节数组提取信号值
 *
 * Com_PackSignal 的逆操作。算法与 Pack 对称。
 */
static uint64_t Com_UnpackSignal(const Com_SignalConfigType *signal,
                                 const uint8_t *buffer)
{
    uint8_t  bit_size  = signal->bit_size;
    uint8_t  start_bit = signal->bit_position;
    uint64_t value     = 0ULL;
    uint16_t i;

    if (bit_size == 0U || buffer == NULL) {
        return 0ULL;
    }

    if (signal->byte_order == COM_BYTE_ORDER_INTEL) {
        /* Intel 解包: LSB first */
        for (i = 0U; i < bit_size; i++) {
            uint16_t target_bit      = (uint16_t)start_bit + i;
            uint8_t  target_byte     = (uint8_t)(target_bit >> 3U);
            uint8_t  target_bit_mask = (uint8_t)(1U << (target_bit & 0x07U));

            if (buffer[target_byte] & target_bit_mask) {
                value |= (1ULL << i);
            }
        }
    } else {
        /* Motorola 解包: MSB first (与 Pack 对称) */
        uint8_t start_byte = (uint8_t)(start_bit >> 3U);
        uint8_t end_bit    = start_bit + bit_size - 1U;
        uint8_t end_byte   = (uint8_t)(end_bit >> 3U);

        for (i = 0U; i < bit_size; i++) {
            uint8_t bit_from_msb = (uint8_t)(bit_size - 1U - i);
            uint8_t byte_idx = end_byte - (bit_from_msb >> 3U);
            uint8_t bit_in_byte;

            if (byte_idx == start_byte) {
                uint8_t start_bit_in_byte = (uint8_t)(start_bit & 0x07U);
                uint8_t bits_in_this_byte = 8U - start_bit_in_byte;
                uint8_t local_bit = bit_from_msb;
                if (local_bit < bits_in_this_byte) {
                    bit_in_byte = start_bit_in_byte + bits_in_this_byte - 1U - local_bit;
                } else {
                    continue;
                }
            } else if (byte_idx == end_byte) {
                uint8_t local_bit = bit_from_msb;
                uint8_t prev_bytes_bits = (uint8_t)((end_byte - start_byte) * 8U
                                          - (start_bit & 0x07U));
                if (local_bit >= prev_bytes_bits) {
                    bit_in_byte = (local_bit - prev_bytes_bits);
                } else {
                    continue;
                }
            } else {
                uint8_t start_bit_in_byte = (uint8_t)(start_bit & 0x07U);
                uint8_t offset_in_group = bit_from_msb - (8U - start_bit_in_byte)
                                          - (uint8_t)((end_byte - byte_idx - 1U) * 8U);
                bit_in_byte = 7U - (offset_in_group & 0x07U);
            }

            if (buffer[byte_idx] & (1U << bit_in_byte)) {
                value |= (1ULL << i);
            }
        }
    }

    /* 有符号扩展 (Sign Extension)
     *
     * 如果信号是有符号的，需要将符号位扩展到高位。
     * 例: 8-bit signed 信号值 = 0xFF (= -1 作为 int8)
     *     如果不扩展，uint64_t 会得到 255。
     *     符号扩展后: 0xFFFFFFFFFFFFFFFF (= -1 作为 int64)。
     */
    if (signal->is_signed && bit_size < 64U) {
        uint64_t sign_bit_mask = (1ULL << (bit_size - 1U));
        if (value & sign_bit_mask) {
            /* 符号位=1，填充高位全 1 */
            value |= ~((1ULL << bit_size) - 1ULL);
        }
    }

    return value;
}

/* ===================================================================
 *  实现: I-PDU 管理
 * =================================================================== */

/**
 * @brief 根据信号 ID 获取信号值 (从 shadow buffer)
 *
 * 注意: 返回的是上次 Com_SendSignal 写入或 Com_RxIndication 接收的值，
 *       不是 I-PDU buffer 中当前的值。
 *       Shadow buffer 是信号的"最后已知值"。
 */
static void Com_WriteSignalToBuffer(Com_SignalIdType signal_id, uint64_t value)
{
    Com_SignalValues[signal_id]     = value;
    Com_SignalTimestamps[signal_id] = Com_SystemTimeMs;
    /* Update Bit: 有新数据时置 1，SWC 通过 Com_GetUpdateBit 读取并清 0 */
    if (Com_SignalConfig[signal_id].has_update_bit) {
        Com_UpdateBits[signal_id] = 1U;
    }
    /* 收到数据即恢复 OK 状态 (清除之前的 TIMEOUT) */
    Com_SignalStatus[signal_id] = COM_SIGNAL_OK;
}

/**
 * @brief 从 shadow buffer 读取信号值
 */
static uint64_t Com_ReadSignalFromBuffer(Com_SignalIdType signal_id)
{
    return Com_SignalValues[signal_id];
}

/**
 * @brief 获取属于某 I-PDU 的所有信号 ID
 *
 * RX 路径使用: 收到 I-PDU 后需要解包其中所有信号。
 * 遍历信号配置表，收集所有属于该 I-PDU 的 signal_id。
 */
static uint8_t Com_GetSignalsInIPdu(Com_IPduIdType ipdu_id,
                                    Com_SignalIdType *signal_ids,
                                    uint8_t max_count)
{
    uint8_t count = 0U;
    uint16_t i;

    for (i = 0U; i < COM_SIGNAL_COUNT && count < max_count; i++) {
        if (Com_SignalConfig[i].ipdu_id == ipdu_id) {
            signal_ids[count] = Com_SignalConfig[i].signal_id;
            count++;
        }
    }
    return count;
}

/* ===================================================================
 *  实现: 发送引擎
 * =================================================================== */

/**
 * @brief 发送指定 I-PDU
 *
 * 构建 PduInfoType，调用 PduR_ComTransmit 发送。
 * PduR 会将此 I-PDU 路由到 CanTp（或直接到 CanIf）。
 *
 * 注意: PduIdType 在 PduR 中映射到 CanTp TP PDU ID。
 *       我们确保 COM IPDU ID == PduR PDU ID == CanTp TP PDU ID (均为 0/1)。
 */
static void Com_TriggerIPduSend(Com_IPduBufferType *ipdu,
                                const Com_IPduConfigType *cfg)
{
    PduInfoType pdu_info;

    if (!ipdu->enabled) {
        return;  /* IPduGroup 已停止，跳过发送 */
    }

    pdu_info.SduId       = (PduIdType)cfg->ipdu_id;
    pdu_info.SduLength   = (PduLengthType)cfg->dlc;
    pdu_info.SduDataPtr  = ipdu->data;

    LOG_D("Com", "TX I-PDU %u (CAN 0x%lx), DLC=%u",
          (unsigned int)cfg->ipdu_id,
          (unsigned long)cfg->can_id,
          (unsigned int)cfg->dlc);

    if (PduR_ComTransmit(pdu_info.SduId, &pdu_info) == E_OK) {
        ipdu->dirty       = 0U;
        ipdu->last_tx_ms  = Com_SystemTimeMs;
    }
    /* E_NOT_OK: CanTp 忙，下一轮 MainFunction 重试 */
}

/* ===================================================================
 *  公共 API 实现
 * =================================================================== */

/**
 * @brief 初始化 COM 模块
 *
 * 初始化顺序:
 * 1. 清零所有 I-PDU buffer
 * 2. 清零信号 Shadow Buffer
 * 3. 标记所有 I-PDU 为 enabled (= IPduGroup ALL 默认启动)
 *
 * 调用时机: EcuM_Init → Com_Init (Services 层初始化阶段)
 */
void Com_Init(void)
{
    uint16_t i;

    /* 初始化 I-PDU buffer */
    for (i = 0U; i < COM_IPDU_COUNT; i++) {
        (void)memset(Com_IPduBuffers[i].data, 0, COM_IPDU_MAX_DATA_LEN);
        Com_IPduBuffers[i].last_tx_ms = 0U;
        Com_IPduBuffers[i].dirty      = 0U;
        Com_IPduBuffers[i].enabled    = 1U;  /* 默认全部启用 */
    }

    /* 初始化信号 Shadow Buffer + Update Bit + Timeout 状态 */
    for (i = 0U; i < COM_SIGNAL_COUNT; i++) {
        Com_SignalValues[i]     = 0ULL;
        Com_SignalTimestamps[i] = 0U;
        Com_UpdateBits[i]       = 0U;
        Com_SignalStatus[i]     = COM_SIGNAL_OK;
    }

    Com_SystemTimeMs   = 0U;
    Com_IsInitialized  = 1U;

    LOG_I("Com", "Init done: %u I-PDUs, %u signals",
          (unsigned int)COM_IPDU_COUNT, (unsigned int)COM_SIGNAL_COUNT);
}

/**
 * @brief COM 模块 MainFunction（周期调用）
 *
 * 这是 COM 层的"心跳"——由 EcuM_MainFunction 周期调用。
 * 每次调用推进系统时间，并检查是否需要发送 I-PDU。
 *
 * 发送触发条件:
 * 1. I-PDU dirty 且有 PENDING 属性信号 → 发送
 *    (TRIGGERED 信号已在 Com_SendSignal 中立即发送，这里处理 PENDING)
 * 2. 距上次发送时间 >= cycle_time_ms → 周期发送
 *
 * 为什么周期和 PENDING 都在 MainFunction 处理?
 * - 两者都需要"批量发送"：合并同一 I-PDU 的多个信号更新
 * - TRIGGERED 需要"立即发送"，不等待 MainFunction
 *
 * 周期计算:
 *   Com_SystemTimeMs 每次调用增加 COM_MAINFUNCTION_PERIOD_MS
 *   对于 cycle_time_ms = 500ms: 每 50 次 MainFunction 发送一次
 */
void Com_MainFunction(void)
{
    uint16_t i;

    Com_SystemTimeMs += COM_MAINFUNCTION_PERIOD_MS;

    for (i = 0U; i < COM_IPDU_COUNT; i++) {
        Com_IPduBufferType       *ipdu = &Com_IPduBuffers[i];
        const Com_IPduConfigType *cfg  = Com_FindIPduConfig((Com_IPduIdType)i);

        if (cfg == NULL) {
            continue;
        }

        /* 该 I-PDU 是否被 IPduGroup 启用? */
        if (!ipdu->enabled) {
            continue;
        }

        /* 检查周期发送 */
        if (cfg->cycle_time_ms > 0U) {
            uint32_t elapsed = Com_SystemTimeMs - ipdu->last_tx_ms;
            if (elapsed >= (uint32_t)cfg->cycle_time_ms) {
                Com_TriggerIPduSend(ipdu, cfg);
                continue;  /* 周期发送完成，跳过 PENDING 检查 (已发送) */
            }
        }

        /* 检查 PENDING: dirty I-PDU 需要发送 */
        if (ipdu->dirty) {
            /* 检查是否有 PENDING 属性的信号触发了此 dirty */
            uint8_t has_pending = 0U;
            uint16_t s;
            for (s = 0U; s < COM_SIGNAL_COUNT; s++) {
                if (Com_SignalConfig[s].ipdu_id == (Com_IPduIdType)i &&
                    Com_SignalConfig[s].transfer_property == COM_TRANSFER_PENDING) {
                    has_pending = 1U;
                    break;
                }
            }
            if (has_pending) {
                Com_TriggerIPduSend(ipdu, cfg);
            }
        }
    }

    /* ================================================================
     *  Deadline Monitoring (信号超时检测)
     *
     *  对每个 RX 信号, 如果距上次更新超过配置的 timeout_ms,
     *  标记为 COM_SIGNAL_TIMEOUT。这是 AUTOSAR 安全监控的基础。
     *
     *  例: TestRxData 配置 timeout_ms=3000→3 秒无数据超时。
     * ================================================================ */
    for (i = 0U; i < COM_SIGNAL_COUNT; i++) {
        const Com_SignalConfigType *sig_cfg = &Com_SignalConfig[i];
        if (sig_cfg->timeout_ms > 0U && sig_cfg->direction == COM_RECEIVE) {
            uint32_t elapsed = Com_SystemTimeMs - Com_SignalTimestamps[i];
            if (elapsed >= (uint32_t)sig_cfg->timeout_ms) {
                Com_SignalStatus[i] = COM_SIGNAL_TIMEOUT;
            }
        }
    }
}

/**
 * @brief 发送信号
 *
 * 流程:
 * 1. 查找信号配置 → 找到所属 I-PDU
 * 2. 将信号值写入 Shadow Buffer
 * 3. 将信号值打包到 I-PDU data[]
 * 4. 标记 I-PDU dirty
 * 5. 根据 Transfer Property 决定发送时机
 *
 * TRIGGERED 信号的处理:
 *   Com_SendSignal 中立即发送（不等 MainFunction）。
 *   注意: 多个 TRIGGERED 信号同时更新时，它们共享一次发送。
 *   因为第一个信号已将 I-PDU dirty=0（发送后清），
 *   后续信号检测到 !dirty → 需要重新触发发送。
 *
 *   但这有隐患: 如果同一 I-PDU 的 2 个 TRIGGERED 信号在极短时间内
 *   先后调用 Com_SendSignal，第二个信号会触发第二次发送。
 *   这是因为我们无法知道"第一个发送是否已完成"（异步发送）。
 *   这是简化实现的一个已知局限。
 */
void Com_SendSignal(Com_SignalIdType SignalId, const void *SignalData)
{
    const Com_SignalConfigType *cfg;
    Com_IPduBufferType         *ipdu;
    uint64_t                    raw_value;
    uint8_t                     data_bytes;

    /* DET: 模块未初始化 */
    if (!Com_IsInitialized) {
        Det_ReportError(DET_MODULE_ID_COM, 0U, 1U, COM_E_UNINIT);
        return;
    }
    /* DET: 非法参数 — NULL 指针 */
    if (SignalData == NULL) {
        Det_ReportError(DET_MODULE_ID_COM, 0U, 1U, COM_E_PARAM);
        return;
    }
    cfg = Com_FindSignalConfig(SignalId);
    /* DET: 信号 ID 不存在 */
    if (cfg == NULL) {
        Det_ReportError(DET_MODULE_ID_COM, 0U, 1U, COM_E_SIGNAL_NOT_FOUND);
        return;
    }
    ipdu = &Com_IPduBuffers[cfg->ipdu_id];

    /* 第 1 步: 将 void* 转换为 uint64_t (通用信号值)
     *
     * CAN 信号最大 64 bits (8 bytes)。
     * 对于更小的信号 (8/16/32 bit), 只读取对应字节数。
     */
    data_bytes = (cfg->bit_size + 7U) / 8U;           /* 向上取整: bits→bytes */
    if (data_bytes > COM_SIGNAL_MAX_BYTES) {
        data_bytes = COM_SIGNAL_MAX_BYTES;
    }
    raw_value = 0ULL;
    (void)memcpy(&raw_value, SignalData, data_bytes);

    /* 第 2 步: 写入 Shadow Buffer (记录信号值) */
    Com_WriteSignalToBuffer(SignalId, raw_value);

    /* 第 3 步: 打包到 I-PDU buffer */
    Com_PackSignal(cfg, raw_value, ipdu->data);
    ipdu->dirty = 1U;

    LOG_D("Com", "SendSignal id=%u, val=0x%lx, ipdu=%u",
          (unsigned int)SignalId, (unsigned long)raw_value,
          (unsigned int)cfg->ipdu_id);

    /* 第 4 步: 根据 Transfer Property 决定发送
     *
     * TRIGGERED → 立即发送 (不等 MainFunction)
     * PENDING   → 仅标记 dirty，等待 MainFunction 统一发送
     * NONE      → 仅写入 buffer，不触发发送 (等周期或别的信号触发)
     */
    if (cfg->transfer_property == COM_TRANSFER_TRIGGERED) {
        const Com_IPduConfigType *ipdu_cfg = Com_FindIPduConfig(cfg->ipdu_id);
        if (ipdu_cfg != NULL) {
            Com_TriggerIPduSend(ipdu, ipdu_cfg);
        }
    }
    /* PENDING / NONE: 等待 Com_MainFunction 处理 */
}

/**
 * @brief 接收信号
 *
 * 从 Shadow Buffer 读取上次收到的信号值。
 * 信号值由 Com_RxIndication 在收到 I-PDU 时更新。
 */
void Com_ReceiveSignal(Com_SignalIdType SignalId, void *SignalData)
{
    const Com_SignalConfigType *cfg;
    uint64_t raw_value;
    uint8_t  data_bytes;

    if (!Com_IsInitialized) {
        Det_ReportError(DET_MODULE_ID_COM, 0U, 2U, COM_E_UNINIT);
        return;
    }
    if (SignalData == NULL) {
        Det_ReportError(DET_MODULE_ID_COM, 0U, 2U, COM_E_PARAM);
        return;
    }
    cfg = Com_FindSignalConfig(SignalId);
    if (cfg == NULL) {
        Det_ReportError(DET_MODULE_ID_COM, 0U, 2U, COM_E_SIGNAL_NOT_FOUND);
        return;
    }

    /* 从 Shadow Buffer 读取 */
    raw_value = Com_ReadSignalFromBuffer(SignalId);

    data_bytes = (cfg->bit_size + 7U) / 8U;
    if (data_bytes > COM_SIGNAL_MAX_BYTES) {
        data_bytes = COM_SIGNAL_MAX_BYTES;
    }
    (void)memcpy(SignalData, &raw_value, data_bytes);

    LOG_D("Com", "ReceiveSignal id=%u → val=0x%lx",
          (unsigned int)SignalId, (unsigned long)raw_value);
}

/**
 * @brief 发送信号组
 *
 * 信号组: 一组语义相关的信号的批量操作。
 * "发送信号组" = "立即发送该组所有信号"。
 *
 * 当前简化版本: 调用所有信号对应的 I-PDU 发送。
 * 未来可按信号组配置表精确控制。
 */
uint8_t Com_SendSignalGroup(Com_SignalIdType GroupId)
{
    uint16_t i;
    uint8_t  sent_count = 0U;

    LOG_D("Com", "SendSignalGroup group=%u", (unsigned int)GroupId);

    for (i = 0U; i < COM_IPDU_COUNT; i++) {
        Com_IPduBufferType       *ipdu = &Com_IPduBuffers[i];
        const Com_IPduConfigType *cfg  = Com_FindIPduConfig((Com_IPduIdType)i);

        if (cfg != NULL && cfg->group_id == (Com_IPduIdType)GroupId
            && ipdu->dirty && ipdu->enabled) {
            Com_TriggerIPduSend(ipdu, cfg);
            sent_count++;
        }
    }

    return (sent_count > 0U) ? 0U : 1U;
}

/**
 * @brief 接收信号组
 */
uint8_t Com_ReceiveSignalGroup(Com_SignalIdType GroupId)
{
    (void)GroupId;
    /* 接收路径由 Com_RxIndication 自动处理，无需主动触发 */
    return 0U;
}

/**
 * @brief 启动 I-PDU 组
 *
 * 使能指定组内所有 I-PDU 的收发。
 * 组被启动后，周期发送和事件触发恢复正常。
 */
void Com_IPduGroupStart(Com_IPduIdType GroupId)
{
    uint16_t i;

    LOG_D("Com", "IPduGroupStart group=%u", (unsigned int)GroupId);

    for (i = 0U; i < COM_IPDU_COUNT; i++) {
        const Com_IPduConfigType *cfg = Com_FindIPduConfig((Com_IPduIdType)i);
        if (cfg != NULL && cfg->group_id == GroupId) {
            Com_IPduBuffers[i].enabled = 1U;
        }
    }
}

/**
 * @brief 停止 I-PDU 组
 *
 * 禁用指定组内所有 I-PDU 的收发。
 * 组被停止后: 周期发送暂停、事件触发被忽略、接收丢弃。
 */
void Com_IPduGroupStop(Com_IPduIdType GroupId)
{
    uint16_t i;

    LOG_D("Com", "IPduGroupStop group=%u", (unsigned int)GroupId);

    for (i = 0U; i < COM_IPDU_COUNT; i++) {
        const Com_IPduConfigType *cfg = Com_FindIPduConfig((Com_IPduIdType)i);
        if (cfg != NULL && cfg->group_id == GroupId) {
            Com_IPduBuffers[i].enabled = 0U;
        }
    }
}

/* ===================================================================
 *  Update Bit & Signal Status API
 * =================================================================== */

/**
 * @brief 获取并清除信号更新标志位
 *
 * AUTOSAR Update Bit 模式:
 *   1. COM 收到新数据 → Update Bit = 1
 *   2. SWC 调用 Com_GetUpdateBit → 读到 1，同时清 0
 *   3. SWC 处理数据 → 下一轮循环再检查
 *
 *   如果 SWC 检查时发现 Update Bit = 0，跳过本次处理（无新数据）。
 *   这比"比较新旧值"更可靠——万一值没变但来源变了呢。
 */
uint8_t Com_GetUpdateBit(Com_SignalIdType SignalId)
{
    const Com_SignalConfigType *cfg;
    uint8_t bit;

    cfg = Com_FindSignalConfig(SignalId);
    if (cfg == NULL || !cfg->has_update_bit) {
        return 0U;
    }

    bit = Com_UpdateBits[SignalId];
    Com_UpdateBits[SignalId] = 0U;  /* 读取后清零 */
    return bit;
}

/**
 * @brief 获取信号超时状态
 *
 * AUTOSAR Deadline Monitoring:
 *   由 Com_MainFunction 周期性检测，超时后标记 COM_SIGNAL_TIMEOUT。
 *   SWC 在读信号值前应先检查此状态。
 *
 *   收到新数据后状态自动恢复为 COM_SIGNAL_OK。
 */
Com_SignalStatusType Com_GetSignalStatus(Com_SignalIdType SignalId)
{
    const Com_SignalConfigType *cfg;

    cfg = Com_FindSignalConfig(SignalId);
    if (cfg == NULL) {
        return COM_SIGNAL_OK;  /* 不存在的信号不报错 */
    }

    return Com_SignalStatus[SignalId];
}

/* ===================================================================
 *  回调接口实现 (被 PduR 调用)
 * =================================================================== */

/**
 * @brief 接收指示: PduR 收到 I-PDU → 通知 COM 解包信号
 *
 * 调用链: CAN ISR → CanIf_RxIndication → PduR_CanIfRxIndication
 *         → CanTp_RxIndication (SF直接交付 / MF重组)
 *         → PduR_CanTpRxIndication → Com_RxIndication
 *
 * 本函数职责:
 * 1. 将收到的 I-PDU 数据拷贝到本地 buffer
 * 2. 查找该 I-PDU 包含的所有信号
 * 3. 逐个 UnpackSignal → 更新 Shadow Buffer
 * 4. 更新信号时间戳
 *
 * 注意: 此函数在 CanTp 完成重组后调用，收到的已是完整的 I-PDU 数据。
 *       无需在此处做 PCI 解码或流控——那是 CanTp 的工作。
 *
 * @param RxPduId     PduR 路由后的 PDU ID (= COM IPDU ID)
 * @param PduInfoPtr  收到的 I-PDU 数据
 */
void Com_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr)
{
    Com_IPduBufferType         *ipdu;
    const Com_IPduConfigType   *ipdu_cfg;
    Com_SignalIdType            signal_ids[COM_SIGNAL_COUNT];
    uint8_t                     signal_count;
    uint8_t                     copy_len;
    uint8_t                     s;
    Com_IPduIdType              ipdu_id = (Com_IPduIdType)RxPduId;

    if (PduInfoPtr == NULL || PduInfoPtr->SduDataPtr == NULL) {
        return;
    }

    ipdu_cfg = Com_FindIPduConfig(ipdu_id);
    if (ipdu_cfg == NULL) {
        return;
    }
    ipdu = &Com_IPduBuffers[ipdu_id];

    /* 检查 IPduGroup 是否启用 */
    if (!ipdu->enabled) {
        return;
    }

    /* 第 1 步: 拷贝 I-PDU 数据到本地 buffer */
    copy_len = (PduInfoPtr->SduLength < ipdu_cfg->dlc)
               ? (uint8_t)PduInfoPtr->SduLength : ipdu_cfg->dlc;
    (void)memcpy(ipdu->data, PduInfoPtr->SduDataPtr, copy_len);
    ipdu->dirty = 0U;  /* 刚收到的数据是"干净"的 */

    LOG_D("Com", "RxIndication: I-PDU %u, DLC=%u",
          (unsigned int)ipdu_id, (unsigned int)copy_len);

    /* 第 2 步: 解包该 I-PDU 下的所有信号 */
    signal_count = Com_GetSignalsInIPdu(ipdu_id, signal_ids,
                                        (uint8_t)COM_SIGNAL_COUNT);

    for (s = 0U; s < signal_count; s++) {
        const Com_SignalConfigType *signal_cfg;
        uint64_t value;

        signal_cfg = Com_FindSignalConfig(signal_ids[s]);
        if (signal_cfg == NULL || signal_cfg->direction != COM_RECEIVE) {
            continue;  /* 只处理接收方向的信号 */
        }

        value = Com_UnpackSignal(signal_cfg, ipdu->data);
        Com_WriteSignalToBuffer(signal_ids[s], value);

        LOG_D("Com", "  RX signal %u = 0x%lx",
              (unsigned int)signal_ids[s], (unsigned long)value);
    }
}

/**
 * @brief 发送确认: PduR 完成 I-PDU 发送 → 通知 COM
 *
 * 调用链: CAN ISR TX done → CanIf_TxConfirmation → PduR_CanIfTxConfirmation
 *         → CanTp_TxConfirmation (聚合为 I-PDU 级确认)
 *         → PduR_CanTpTxConfirmation → Com_TxConfirmation
 *
 * COM 收到此回调后:
 * - 清 I-PDU sending 状态
 * - 如果有新的 PENDING 信号等待发送，可在下一轮 MainFunction 中处理
 *
 * @param TxPduId  已发送完成的 I-PDU ID
 */
void Com_TxConfirmation(PduIdType TxPduId)
{
    Com_IPduIdType ipdu_id = (Com_IPduIdType)TxPduId;

    if (ipdu_id >= COM_IPDU_COUNT) {
        return;
    }

    LOG_D("Com", "TxConfirmation: I-PDU %u sent OK", (unsigned int)ipdu_id);

    /* 发送完成，buffer 已清 dirty (在 Com_TriggerIPduSend 中已清) */
    /* 此处暂无额外清理工作，未来可通知 SWC */
}
