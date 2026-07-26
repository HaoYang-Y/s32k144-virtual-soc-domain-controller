/**
 * @file    Can_Cfg.c
 * @brief   CAN 驱动配置实例 — 每控制器一个 Can_ConfigType
 */

#include "Can_Cfg.h"

/* ===================================================================
 *  CAN0 — FlexCAN0, 500k, 1 TX + 1 RX
 * =================================================================== */

static const Can_HardwareObject tx_mb_can0[] = {{.id = 0x123UL}};
static const Can_HardwareObject rx_mb_can0[] = {{.id = 0x100UL}};

const Can_ConfigType Can_Config_CAN0 = {
    .max_num_mb       = 16,
    .flexcan_mode     = CAN_MODE_NORMAL,
    .prop_seg         = 7,
    .phase_seg1       = 4,
    .phase_seg2       = 1,
    .pre_divider      = 0,
    .r_jumpwidth      = 1,
    .num_tx_mailboxes = 1,
    .num_rx_mailboxes = 1,
    .tx_mailboxes     = tx_mb_can0,
    .rx_mailboxes     = rx_mb_can0,
};
