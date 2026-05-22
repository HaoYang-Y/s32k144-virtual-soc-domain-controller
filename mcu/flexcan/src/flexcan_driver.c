/*
 * @brief FlexCAN 驱动实现 (S32K144)
 *        通过直接操作寄存器实现 CAN 收发
 *
 * @note 参考: S32K1xx Reference Manual, Chapter 60 (FlexCAN)
 *       对应书籍：第11章 CAN 总线
 *       教学文档：docs/CAN从零到驱动.md
 *
 *       寄存器布局见 docs 第4篇，位时序计算见第2篇，
 *       发送流程见第6篇，接收流程见第7篇。
 */
#include "flexcan_driver.h"

#include <stddef.h>    /* NULL */

/* ========== 寄存器结构体定义 ========== */

/**
 * @brief FlexCAN 模块寄存器映射
 *        对应参考手册 FlexCAN Memory Map 表
 *
 * @note 偏移地址见 docs 第8篇附录B
 *       中间的保留位必须用占位成员补齐，确保后续成员偏移正确
 */
typedef struct {
    volatile uint32_t MCR;        /* 0x00: 模块配置寄存器 */
    volatile uint32_t CTRL1;      /* 0x04: 控制寄存器 1（位时序） */
    volatile uint32_t TIMER;      /* 0x08: 定时器 */
    uint32_t        _reserved0;  /* 0x0C: 保留 */
    volatile uint32_t RXMGMASK;   /* 0x10: 接收全局掩码 */
    volatile uint32_t RXFGMASK;   /* 0x14: 接收 FIFO 全局掩码 */
    volatile uint32_t RXFIR;      /* 0x18: 接收 FIFO ID 寄存器 */
    uint32_t        _reserved1;  /* 0x1C: 保留 */
    volatile uint32_t CBT;        /* 0x20: 位时序配置 */
    uint32_t        _reserved2;  /* 0x24: 保留 */
    volatile uint32_t IMASK1;     /* 0x28: 中断掩码寄存器 */
    uint32_t        _reserved3;  /* 0x2C: 保留 */
    volatile uint32_t IFLAG1;     /* 0x30: 中断标志寄存器 */
    volatile uint32_t CTRL2;      /* 0x34: 控制寄存器 2 */
    volatile uint32_t ESR2;       /* 0x38: 错误状态寄存器 2 */
} flexcan_regs_t;

/**
 * @brief 消息缓冲区 (Message Buffer) 结构
 *        每个 MB 16 字节，布局见 docs 第3.4节
 */
typedef struct {
    volatile uint32_t CS;     /* +0x00: 控制/状态 (CODE, IDE, RTR, DLC, TIME) */
    volatile uint32_t ID;     /* +0x04: 帧 ID (标准 11位 / 扩展 29位) */
    volatile uint32_t WORD0;  /* +0x08: 数据字节 [3:0] */
    volatile uint32_t WORD1;  /* +0x0C: 数据字节 [7:4] */
} flexcan_mb_t;

/* ========== 寄存器基址映射 ========== */

/** @brief CAN0 寄存器基址 (见 flexcan_driver.h) */
#define CAN0_REGS    ((flexcan_regs_t *)CAN0_BASE)

/** @brief CAN1 寄存器基址 (见 flexcan_driver.h) */
#define CAN1_REGS    ((flexcan_regs_t *)CAN1_BASE)

/**
 * @brief 获取指定 CAN 通道的消息缓冲区指针
 *
 * @param[in]  channel        CAN 通道
 * @param[in]  mb_index       消息缓冲区编号 (0~15)
 * @return     flexcan_mb_t*  MB 指针，参数错误返回 NULL
 *
 * @note MB_n 基址 = CANn_BASE + 0x80 + n × 0x10
 *       (见 docs 第3.5节 "MB 基址计算")
 */
static flexcan_mb_t *get_mb_ptr(can_channel_t channel, uint32_t mb_index)
{
    uint32_t base;

    if (mb_index > 15u) {
        return NULL;
    }

    switch (channel) {
        case CAN_CHANNEL_0:
            base = CAN0_BASE;
            break;
        case CAN_CHANNEL_1:
            base = CAN1_BASE;
            break;
        default:
            return NULL;
    }

    /* MB_n 地址 = 基址 + 0x80 + n × 0x10 */
    return (flexcan_mb_t *)(base + 0x80u + (mb_index * 0x10u));
}

/* ========== 位掩码定义 ========== */

/* MB_CS 相关 */
#define MB_CS_CODE_SHIFT    (24u)
#define MB_CS_CODE_MASK     (0xFFu << MB_CS_CODE_SHIFT)
#define MB_CS_IDE_MASK      (1u << 22u)
#define MB_CS_RTR_MASK      (1u << 21u)
#define MB_CS_DLC_SHIFT     (16u)
#define MB_CS_DLC_MASK      (0x0Fu << MB_CS_DLC_SHIFT)

/* MB CODE 值 (见 docs 第4.3节 表CODE值速查) */
#define CODE_TX_ACTIVE      (0x0Cu)  /* 发送进行中（硬件正在发送） */
#define CODE_TX_INACTIVE    (0x08u)  /* 发送邮箱空闲 */
#define CODE_INACTIVE       (0x00u)  /* MB 未使用 */
#define CODE_RX_EMPTY       (0x04u)  /* 接收邮箱等待接收 */
#define CODE_RX_FULL        (0x0Cu)  /* 接收完成（有数据待取） */

/* IFLAG1 相关 */
#define IFLAG1_BUF0_MASK    (1u << 0u)   /* MB0 事件标志 */
#define IFLAG1_BUF1_MASK    (1u << 1u)   /* MB1 事件标志 */

/* ========== 函数实现 ========== */

int flexcan_init(can_channel_t channel, uint32_t bitrate)
{
    /*
     * TODO §11.2.2：实现 FlexCAN 初始化
     * 步骤（见 docs 第5篇）：
     *   1. 使能 CAN 时钟 (PCC_CANn[CGC])
     *   2. 等待 MCR[LOM] 和 MCR[HALT] 置位 (冻结模式)
     *   3. MCR[SOFT_RST] = 1 软件复位
     *   4. 配置 MCR:
     *      - FRZ = 1 (使能冻结)
     *      - HALT = 1 (进入冻结)
     *      - WRN_EN = 0
     *      - SRX_DIS = 0
     *   5. 配置 CTRL1:
     *      - PRESDIV = 波特率预分频
     *      - PROPSEG = 传播段
     *      - PSEG1 = 相位段 1
     *      - PSEG2 = 相位段 2
     *      - RJW = 重同步跳跃宽度
     *   6. 配置所有 MB 控制状态为 0
     *   7. 退出冻结: MCR[HALT] = 0
     *   8. 等待 FRZ_ACK 清零
     *   9. 返回 0
     */
    (void)channel;
    (void)bitrate;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 见 docs 第5篇 "初始化分步流程" */ }

    return 0;
}

int flexcan_send_msg(can_channel_t channel, const can_frame_t *frame)
{
    /*
     * 发送流程（见 docs 第6篇 "发送分步流程"）：
     *   步骤1: 查找空闲发送缓冲区 (CODE == INACTIVE/RETIRED)
     *   步骤2: 填写 MB ID 寄存器
     *   步骤3: 填写 WORD0/WORD1 数据
     *   步骤4: 写 CS 寄存器 (CODE=TX_ACTIVE) 触发发送
     *   步骤5: 等待发送完成 (IFLAG1 对应位置位 或 CODE 变回 INACTIVE)
     *   步骤6: 清除完成标志 (IFLAG1 写 1)
     */

    flexcan_mb_t *mb;
    uint32_t     cs_code;
    uint32_t     timeout;
    uint32_t     id_reg;
    uint32_t     word0;
    uint32_t     word1;
    uint32_t     dlc;
    uint8_t      i;

    /* ---- 参数合法性检查 ---- */
    if (frame == NULL) {
        return -1;
    }

    /* 使用 MB0 作为发送缓冲区 */
    mb = get_mb_ptr(channel, 0u);
    if (mb == NULL) {
        return -1;
    }

    /* 限制 DLC 范围 (CAN 协议规定 0~8) */
    dlc = (frame->dlc > 8u) ? 8u : frame->dlc;

    /* ========== 步骤1：检查 MB 是否空闲 ========== */
    /*
     * 读取 MB0 的 CS 寄存器，检查 CODE 值。
     * 如果 CODE == TX_ACTIVE (0x0C)，说明硬件正在发送上一帧，
     * 需要等待发送完成。用有限轮询(超时)避免死锁。
     *
     * CODE 字段在 CS 的 bit[31:24] (见 docs 第4.3节)
     */
    timeout = 10000u;
    do {
        cs_code = (mb->CS >> MB_CS_CODE_SHIFT) & 0xFFu;

        /* TX_ACTIVE=0x0C 表示正在发送，需要等待 */
        if (cs_code == CODE_TX_ACTIVE) {
            /* 简短延迟，让出 CPU 或等待硬件 */
            for (volatile uint32_t d = 0u; d < 10u; d++) { }
            timeout--;
        } else {
            break;  /* 不是发送中，可以继续 */
        }
    } while (timeout > 0u);

    if (timeout == 0u) {
        /* 发送缓冲区卡住了（可能总线关闭） */
        return -1;
    }

    /* ========== 步骤2：填写 MB ID 寄存器 ========== */
    /*
     * 标准帧: ID 放在 bit[28:18]（左移 18 位）
     * 扩展帧: ID 放在 bit[28:0]  （直接赋值）
     * 参考 docs 第4.4节 "MB ID 寄存器"
     *
     * 注意：先填 ID 再填 CS，因为写 CS 可能触发发送。
     *       如果 ID 还没填好就触发发送，总线上的数据会错乱。
     */
    if (frame->format == CAN_FRAME_EXT) {
        /* 扩展帧: 29 位 ID 直接填入 bit[28:0] */
        id_reg = frame->id & CAN_EXTID_MAX;
    } else {
        /* 标准帧: 11 位 ID 左移到 bit[28:18] */
        id_reg = (frame->id & CAN_STDID_MAX) << 18u;
    }
    mb->ID = id_reg;

    /* ========== 步骤3：填写数据 (WORD0 / WORD1) ========== */
    /*
     * 小端序存放: data[0] 在最低 8 位, data[1] 在次低 8 位...
     * 参考 docs 第3.4节 "每个 MB 的 16 字节结构"
     *
     * WORD0 (偏移 +0x08): data[3] | data[2] | data[1] | data[0]
     * WORD1 (偏移 +0x0C): data[7] | data[6] | data[5] | data[4]
     */
    word0 = 0u;
    word1 = 0u;

    for (i = 0u; i < dlc; i++) {
        if (i < 4u) {
            /* data[0]~data[3] 放入 WORD0 */
            word0 |= ((uint32_t)frame->data[i] << (i * 8u));
        } else {
            /* data[4]~data[7] 放入 WORD1 */
            word1 |= ((uint32_t)frame->data[i] << ((i - 4u) * 8u));
        }
    }

    mb->WORD0 = word0;
    mb->WORD1 = word1;

    /* ========== 步骤4：填写 CS 触发发送 ========== */
    /*
     * 构造 CS 值并写入，CODE=TX_ACTIVE(0x0C) 触发发送：
     *   bit[31:24] = CODE = 0x0C (TX_ACTIVE)  ← 触发发送
     *   bit[22]    = IDE  (0=标准帧, 1=扩展帧)
     *   bit[21]    = RTR  (0=数据帧, 1=远程帧)
     *   bit[20:16] = DLC  (数据长度)
     * 参考 docs 第4.3节 "MB CS 寄存器"
     *
     * 当 CODE=0x0C 写入 CS 的瞬间，硬件检测到 CODE 从旧值
     * 变成 TX_ACTIVE，于是开始仲裁、发送。
     */
    {
        uint32_t cs_val;

        cs_val = ((uint32_t)CODE_TX_ACTIVE << MB_CS_CODE_SHIFT)
               | ((uint32_t)(frame->format) << 22u)
               | ((uint32_t)(frame->type) << 21u)
               | ((uint32_t)dlc << MB_CS_DLC_SHIFT);

        mb->CS = cs_val;
    }

    /* ========== 步骤5：等待发送完成 ========== */
    /*
     * 方法一（查 IFLAG1）:
     *   轮询 IFLAG1[0] 是否置位，置位表示 MB0 发送完成
     *   参考 docs 第4.6节 "IFLAG1 寄存器"
     *
     * 超时机制：防止总线故障导致无限等待
     */
    timeout = 10000u;
    {
        volatile flexcan_regs_t *regs;

        regs = (channel == CAN_CHANNEL_0) ? CAN0_REGS : CAN1_REGS;

        do {
            if (regs->IFLAG1 & IFLAG1_BUF0_MASK) {
                break;   /* 发送完成 */
            }
            for (volatile uint32_t d = 0u; d < 10u; d++) { }
            timeout--;
        } while (timeout > 0u);

        if (timeout == 0u) {
            /* 发送超时（可能总线故障） */
            return -1;
        }

        /* ========== 步骤6：清除完成标志 ========== */
        /*
         * IFLAG1 写 1 清零，写 0 无影响
         * 必须清除标志，否则下次查询会误认为有新事件
         */
        regs->IFLAG1 = IFLAG1_BUF0_MASK;
    }

    return 0;
}

int flexcan_recv_msg(can_channel_t channel, can_frame_t *frame)
{
    /*
     * TODO §11.2.3：实现 CAN 接收
     * 步骤（见 docs 第7篇 "接收分步流程"）：
     *   1. 轮询 IFLAG1 检查接收完成标志
     *   2. 读取 MBx: CS (检查 CODE==RX_FULL), ID, DATA
     *   3. 填充 frame 结构体
     *   4. 清 IFLAG1 对应位 (写 1)
     *   5. 设置 CS[CODE]=RX_EMPTY (继续接收)
     *   6. 返回 0
     */
    (void)channel;
    (void)frame;

    /* TODO：删除下面这行，填入你的实现 */
    while (1) { /* 见 docs 第7篇 "接收分步流程" */ }

    return 0;
}
