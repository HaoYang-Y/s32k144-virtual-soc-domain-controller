/**
 * @file    Rte.c
 * @brief   [AUTOSAR CP] Runtime Environment 实现
 *
 * @note    RTE 是 SWC 与 BSW 之间的胶水层。
 *          AUTOSAR 的 RTE 通常由工具从 ARXML 自动生成。
 *          这里手动实现，核心逻辑:
 *            Rte_Write → Com_SendSignal (信号→I-PDU→CAN)
 *            Rte_Read  → Com_ReceiveSignal (Shadow Buffer→信号)
 *
 *          AUTOSAR 学习要点:
 *          - RTE 不包含业务逻辑，只做信号映射
 *          - RTE 的上层是 SWC（应用层），下层是 COM（BSW）
 *          - RTE 可以包含类型转换 (raw value ↔ physical value)
 *            如: 原始值 500 × 0.01 = 5.00 km/h
 *            当前版本暂不做 scale/offset 转换，直接透传 raw 值。
 */

#include "Rte.h"
#include "Com.h"
#include "Com_Cfg.h"
#include "Log.h"

/* ===================================================================
 *  RTE 初始化 & 主函数
 * =================================================================== */

void Rte_Init(void)
{
    LOG_I("RTE", "Init done");
}

void Rte_Start(void)
{
    LOG_I("RTE", "Start done");
}

void Rte_MainFunction(void)
{
    /* RTE 的 MainFunction 处理 SWC 到 BSW 的周期任务。
     * 例如: 检查信号更新、触发数据转换等。
     * 当前实现: 无额外逻辑，SWC 通过其自身的 MainFunction 驱动。 */
}

/* ===================================================================
 *  RTE 信号读写接口 (SWC ↔ COM 桥接)
 *
 *  当前 Rte_Write_VehicleSignal / Rte_Read_VehicleSignal 是通用接口。
 *  每个信号通过 SignalId 区分。
 *
 *  AUTOSAR 标准实践:
 *  工具生成逐信号的专用接口，如 Rte_Write_VehicleSpeed(uint16)，
 *  内部调用 Com_SendSignal(COM_SIGNAL_ID_VEHICLE_SPEED, &speed)。
 *  这样做的好处: 编译期类型检查、减少运行时查表开销。
 * =================================================================== */

/**
 * @brief SWC 写信号 (Rte_Write → Com_SendSignal)
 *
 * @param SignalId  信号 ID (运行时决定，非 AUTOSAR 标准做法但灵活)
 * @param Data      信号数据指针
 * @param Length    数据长度 (bytes)
 */
void Rte_Write_VehicleSignal(uint8_t SignalId, const uint8_t *Data, uint8_t Length)
{
    if (Data == NULL || Length == 0U) {
        return;
    }

    LOG_D("RTE", "Write SignalId=%u, len=%u", (unsigned int)SignalId, (unsigned int)Length);
    Com_SendSignal((Com_SignalIdType)SignalId, (const void *)Data);
}

/**
 * @brief SWC 读信号 (Rte_Read → Com_ReceiveSignal)
 *
 * @param SignalId  信号 ID
 * @param Data      输出缓冲区
 * @param Length    输入: 缓冲区大小; 输出: 实际数据长度
 */
void Rte_Read_VehicleSignal(uint8_t SignalId, uint8_t *Data, uint8_t *Length)
{
    if (Data == NULL || Length == NULL || *Length == 0U) {
        return;
    }

    LOG_D("RTE", "Read SignalId=%u", (unsigned int)SignalId);
    Com_ReceiveSignal((Com_SignalIdType)SignalId, (void *)Data);
}
