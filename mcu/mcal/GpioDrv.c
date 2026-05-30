/*
 * @brief AUTOSAR MCAL GpioDrv 驱动实现 (S32K144)
 *        通过 NXP SDK PINS_DRV API 实现 GPIO 输入/输出功能
 *
 * @note 使用 S32K1xx SDK platform/drivers 层的标准接口：
 *       - PINS_DRV_Init()   批量初始化引脚
 *       - PINS_DRV_WritePin() 写引脚
 *       - PINS_DRV_ReadPins() 读引脚
 *       - PINS_DRV_SetPins() / PINS_DRV_ClearPins() 置位/清零
 *
 *       原版本（commit f7a8573）通过直接操作寄存器实现：
 *       GPIO PDOR/PSOR/PCOR, PORT PCR, PCC 时钟门控
 *       现统一委托给 SDK 驱动层，提升可移植性。
 */
#include "GpioDrv.h"
#include <stddef.h>

/* ========== SDK 基地址指针映射表 ========== */

/**
 * @brief GPIO 基地址指针数组，索引 0~4 对应 PTA~PTE
 */
static GPIO_Type *const g_gpio_base[] = {
    PTA,  /* 端口 A */
    PTB,  /* 端口 B */
    PTC,  /* 端口 C */
    PTD,  /* 端口 D */
    PTE,  /* 端口 E */
};

/**
 * @brief PORT 基地址指针数组，索引 0~4 对应 PORTA~PORTE
 */
static PORT_Type *const g_port_base[] = {
    PORTA,  /* 端口 A */
    PORTB,  /* 端口 B */
    PORTC,  /* 端口 C */
    PORTD,  /* 端口 D */
    PORTE,  /* 端口 E */
};

/* ========== 辅助函数 ========== */

/**
 * @brief 从通道 ID 提取端口号
 *
 * @param[in]  ch    GPIO 通道 ID
 * @return     端口号 (0=A, 1=B, 2=C, 3=D, 4=E)
 */
static uint8_t channel_port(Gpio_ChannelType ch)
{
    return (uint8_t)(ch >> 8);
}

/**
 * @brief 从通道 ID 提取引脚号
 *
 * @param[in]  ch    GPIO 通道 ID
 * @return     引脚号 (0~31)
 */
static uint8_t channel_pin(Gpio_ChannelType ch)
{
    return (uint8_t)(ch & 0xFFu);
}

/**
 * @brief 检查通道是否有效
 *
 * @param[in]  channel   GPIO 通道 ID
 * @return     0=有效, -1=端口超范围或引脚超范围
 */
static int check_channel(Gpio_ChannelType channel)
{
    uint8_t port = channel_port(channel);
    uint8_t pin  = channel_pin(channel);

    if (port > 4) { return -1; }
    if (pin > 31) { return -1; }
    return 0;
}

/**
 * @brief 将 MCAL 上拉类型转换为 SDK pull_config
 *
 * @param[in]  pull   MCAL 上拉/下拉类型
 * @return     SDK port_pull_config_t 枚举值
 */
static port_pull_config_t convert_pull(GpioDrv_PullType pull)
{
    switch (pull)
    {
        case GPIO_CFG_PULL_UP:
            return PORT_INTERNAL_PULL_UP_ENABLED;
        case GPIO_CFG_PULL_DOWN:
            return PORT_INTERNAL_PULL_DOWN_ENABLED;
        case GPIO_CFG_PULL_DISABLE:
        default:
            return PORT_INTERNAL_PULL_NOT_ENABLED;
    }
}

/**
 * @brief 将 MCAL 方向类型转换为 SDK direction
 *
 * @param[in]  dir    MCAL 方向类型
 * @return     SDK port_data_direction_t 枚举值
 */
static port_data_direction_t convert_direction(GpioDrv_DirectionType dir)
{
    if (dir == GPIO_CFG_DIR_OUTPUT)
    {
        return GPIO_OUTPUT_DIRECTION;
    }
    return GPIO_INPUT_DIRECTION;
}

/* ========== AUTOSAR MCAL 接口实现 ========== */

/**
 * @brief 初始化 GPIO 驱动及所有通道
 *
 * @param[in]  cfg   GPIO 通道配置数组指针
 *
 * @note 将 MCAL 配置转换为 SDK pin_settings_config_t 数组，
 *       调用 PINS_DRV_Init() 批量初始化，内部自动处理：
 *        PCC 时钟门控、PORT PCR（MUX、上下拉）、GPIO PDDR
 */
void GpioDrv_Init(const GpioDrv_ConfigType *cfg)
{
    uint8_t i;

    if ((cfg == NULL) || (cfg->channels == NULL) || (cfg->num_channels == 0))
    {
        return;
    }

    for (i = 0; i < cfg->num_channels; i++)
    {
        const GpioDrv_ChannelCfgType *ch = &cfg->channels[i];
        uint8_t port;
        uint8_t pin;
        pin_settings_config_t sdk_cfg;

        if (check_channel(ch->channel) != 0)
        {
            continue;
        }

        port = channel_port(ch->channel);
        pin  = channel_pin(ch->channel);

        /* 填充 SDK pin_settings_config_t 结构体 */
        sdk_cfg.base        = g_port_base[port];
        sdk_cfg.pinPortIdx  = pin;
        sdk_cfg.pullConfig  = convert_pull(ch->pull);
        sdk_cfg.mux         = PORT_MUX_AS_GPIO;
        sdk_cfg.gpioBase    = g_gpio_base[port];
        sdk_cfg.direction   = convert_direction(ch->direction);
        sdk_cfg.initValue   = 0U;  /* 初始输出低电平 */
#if FEATURE_PINS_HAS_SLEW_RATE
        sdk_cfg.rateSelect  = PORT_FAST_SLEW_RATE;
#endif
#if FEATURE_PINS_HAS_OPEN_DRAIN
        sdk_cfg.openDrain   = PORT_OPEN_DRAIN_DISABLED;
#endif
#if FEATURE_PINS_HAS_DRIVE_STRENGTH
        sdk_cfg.driveSelect = PORT_LOW_DRIVE_STRENGTH;
#endif
        sdk_cfg.intConfig      = PORT_INT_RISING_EDGE;
        sdk_cfg.clearIntFlag   = true;
        sdk_cfg.digitalFilter  = false;

        /* 逐个调用 SDK 初始化 */
        (void)PINS_DRV_Init(1U, &sdk_cfg);

#ifdef FEATURE_PINS_HAS_PIN_CONTROL_LOCK
        sdk_cfg.pinLock = false;
#endif
        (void)sdk_cfg.pinLock;
    }
}

/**
 * @brief 读取指定 GPIO 通道的输入电平
 *
 * @param[in]  channel   GPIO 通道 ID
 * @param[out] level     读取到的电平值
 * @return     0=成功, -1=通道无效
 *
 * @note 调用 SDK PINS_DRV_ReadPins() 读取指定端口
 */
int Gpio_ReadPin(Gpio_ChannelType channel, Gpio_PinLevelType *level)
{
    uint8_t port;
    uint8_t pin;
    pins_channel_type_t pin_val;

    if (check_channel(channel) != 0)
    {
        return -1;
    }
    if (level == NULL)
    {
        return -1;
    }

    port = channel_port(channel);
    pin  = channel_pin(channel);

    pin_val = PINS_DRV_ReadPins(g_gpio_base[port]);

    *level = (bool)((pin_val >> pin) & 1U);
    return 0;
}

/**
 * @brief 设置指定 GPIO 通道的输出电平
 *
 * @param[in]  channel   GPIO 通道 ID
 * @param[in]  level     要输出的电平值 (true=HIGH, false=LOW)
 * @return     0=成功, -1=通道无效
 *
 * @note 调用 SDK PINS_DRV_WritePin() 设置引脚输出
 */
int Gpio_WritePin(Gpio_ChannelType channel, Gpio_PinLevelType level)
{
    uint8_t port;
    uint8_t pin;

    if (check_channel(channel) != 0)
    {
        return -1;
    }

    port = channel_port(channel);
    pin  = channel_pin(channel);

    PINS_DRV_WritePin(g_gpio_base[port],
                      (pins_channel_type_t)pin,
                      (pins_level_type_t)level);
    return 0;
}
