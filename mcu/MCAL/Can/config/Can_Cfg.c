/**
 * @file    Can_Cfg.c
 * @brief   CAN 驱动配置实例
 *
 * @note    邮箱和位时序从 main.c 迁移至此，供 EcuM 引用
 */

#include "Can_Cfg.h"

static const Can_HardwareObject tx_mailboxes[] = {{.id = 0x123UL}};
static const Can_HardwareObject rx_mailboxes[] = {{.id = 0x100UL}};

const Can_ConfigType Can_Config = {
    .controller=0,.max_num_mb=16,.flexcan_mode=CAN_MODE_NORMAL,
    .prop_seg=7,.phase_seg1=4,.phase_seg2=1,.pre_divider=0,.r_jumpwidth=1,
    .num_tx_mailboxes=1,.num_rx_mailboxes=1,
    .tx_mailboxes=tx_mailboxes,.rx_mailboxes=rx_mailboxes
};
