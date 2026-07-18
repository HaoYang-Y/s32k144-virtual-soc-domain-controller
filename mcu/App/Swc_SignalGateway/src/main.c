/**
 * CAN 测试 — 使用 AUTOSAR MCAL Can 接口（底层封装 CAN PAL）
 * PTD0=橙(TX) PTD1=红(错误) PTD15=绿(心跳) PTD16=蓝(RX)
 */
#include "Port.h"
#include "clock_config.h"
#include "Can.h"
#include "pins_driver.h"
#include <stdint.h>

#define TX_MB  0U
#define RX_MB  1U  /* idx = TX数量 + 0 = 1 */

static const Can_HardwareObject tx_mb[] = {{.id=0x123UL}};
static const Can_HardwareObject rx_mb[] = {{.id=0x100UL}};

static const Can_ConfigType can0_cfg = {
    .controller=0,.max_num_mb=16,.flexcan_mode=CAN_MODE_NORMAL,
    .prop_seg=7,.phase_seg1=4,.phase_seg2=1,.pre_divider=0,.r_jumpwidth=1,
    .num_tx_mailboxes=1,.num_rx_mailboxes=1,
    .tx_mailboxes=tx_mb,.rx_mailboxes=rx_mb
};

static void delay_ms(uint32_t ms){
    volatile uint32_t i; for(;ms>0;ms--) for(i=0;i<12000U;i++)__asm__("nop");
}

int main(void)
{
    uint32_t cnt=0;
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    Port_Init();
    PINS_DRV_SetPins(PTD, (1u<<0)|(1u<<1)|(1u<<15)|(1u<<16));

    if(Can_Init(&can0_cfg)!=STATUS_SUCCESS){
        PINS_DRV_ClearPins(PTD, 1u<<1); /* 红=失败 */
        for(;;){}
    }
    Can_SetControllerMode(0, CAN_CS_STARTED);

    for(;;)
    {
        Can_PduType tx={.id=0x123UL,.length=8U};
        tx.data[0]=cnt&0xFF; tx.data[1]=(cnt>>8)&0xFF;
        tx.data[2]=0xAA; tx.data[3]=0x55; tx.data[4]=0xAA; tx.data[5]=0x55;
        tx.data[6]=(cnt>>16)&0xFF; tx.data[7]=(cnt>>24)&0xFF;

        PINS_DRV_ClearPins(PTD, 1u<<0);
        status_t s=Can_Write(0,TX_MB,&tx);
        PINS_DRV_SetPins(PTD, 1u<<0);

        PINS_DRV_TogglePins(PTD, 1u<<15);           /* 绿心跳 */
        if(s!=0) PINS_DRV_ClearPins(PTD, 1u<<1);   /* 红=TX失败 */
        else     PINS_DRV_SetPins(PTD, 1u<<1);

        Can_PduType rx;
        if(Can_Read(0,RX_MB,&rx)==STATUS_SUCCESS)
            PINS_DRV_TogglePins(PTD, 1u<<16);       /* 蓝=RX */

        cnt++; delay_ms(500);
    }
    return 0;
}
