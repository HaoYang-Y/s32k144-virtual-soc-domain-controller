/**
* @page misra_violations MISRA-C:2012 violations
*
* @section [global]
* Violates MISRA 2012 Required Rule 9.4, Duplicate initialization of object element.
* It's the only way to initialize an array that is member of struct.
*
* @section [global]
* Violates MISRA 2012 Advisory Rule 8.7, External variable could be made static.
* The external variables will be used in other source files in application code.
*/

/* TEXT BELOW IS USED AS SETTING FOR TOOLS *************************************
!!GlobalInfo
product: Clocks v7.0
processor: S32K144
package_id: S32K144_LQFP100
mcu_data: s32sdk_s32k1xx_rtm_402
processor_version: 0.0.0
 * BE CAREFUL MODIFYING THIS COMMENT - IT IS YAML SETTINGS FOR TOOLS **********/

#include "clock_config.h"

/* *************************************************************************
* Configuration structure for peripheral clock configuration 0
* ************************************************************************* */
/*! @brief peripheral clock configuration 0 */
peripheral_clock_config_t peripheralClockConfig0[NUM_OF_PERIPHERAL_CLOCKS_0] = {
    {
        .clockName = ADC0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = ADC1_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPSPI0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPSPI1_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPSPI2_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPUART0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SOSC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPUART1_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SOSC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPUART2_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SOSC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPI2C0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPIT0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = LPTMR0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FTM0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV1,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FTM1_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV1,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FTM2_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV1,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FTM3_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV1,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FLEXIO0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SIRC_DIV2,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = CMP0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = CRC0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = DMAMUX0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = EWM0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FTFC0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PDB0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PDB1_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = RTC0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FlexCAN0_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_SOSC_DIV1,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FlexCAN1_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = FlexCAN2_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PORTA_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PORTB_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PORTC_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PORTD_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
    {
        .clockName = PORTE_CLK,
        .clkGate = true,
        .clkSrc = CLK_SRC_OFF,
        .frac = MULTIPLY_BY_ONE,
        .divider = DIVIDE_BY_ONE,
    },
};

/* *************************************************************************
* Configuration structure for Clock Configuration 0
* ************************************************************************* */
/*! @brief User Configuration structure clock_managerCfg_0 */
clock_manager_user_config_t clockMan1_InitConfig0 = {
    .scgConfig =
    {
        .sircConfig =
        {
            .initialize = true,
            .enableInStop = true,
            .enableInLowPower = true,
            .locked = false,
            .range = SCG_SIRC_RANGE_HIGH,
            .div1 = SCG_ASYNC_CLOCK_DIV_BY_1,
            .div2 = SCG_ASYNC_CLOCK_DIV_BY_1,
        },
        .fircConfig =
        {
            .initialize = true,
            .regulator = true,
            .locked = false,
            .range = SCG_FIRC_RANGE_48M,
            .div1 = SCG_ASYNC_CLOCK_DIV_BY_1,
            .div2 = SCG_ASYNC_CLOCK_DIV_BY_1,
        },
        .rtcConfig =
        {
            .initialize = false,
        },
        .soscConfig =
        {
            .initialize = true,
            .freq = 8000000U,
            .monitorMode = SCG_SOSC_MONITOR_DISABLE,
            .locked = false,
            .extRef = SCG_SOSC_REF_OSC,
            .gain = SCG_SOSC_GAIN_LOW,
            .range = SCG_SOSC_RANGE_HIGH,
            .div1 = SCG_ASYNC_CLOCK_DIV_BY_1,
            .div2 = SCG_ASYNC_CLOCK_DIV_BY_1,
        },
        .spllConfig =
        {
            .initialize = true,
            .monitorMode = SCG_SPLL_MONITOR_DISABLE,
            .locked = false,
            .prediv = (uint8_t)SCG_SPLL_CLOCK_PREDIV_BY_1,
            .mult = (uint8_t)SCG_SPLL_CLOCK_MULTIPLY_BY_28,
            .src = 0U,
            .div1 = SCG_ASYNC_CLOCK_DIV_BY_2,
            .div2 = SCG_ASYNC_CLOCK_DIV_BY_4,
        },
        .clockOutConfig =
        {
            .initialize = true,
            .source = SCG_CLOCKOUT_SRC_FIRC,
        },
        .clockModeConfig =
        {
            .initialize = true,
            .rccrConfig =
            {
                .src = SCG_SYSTEM_CLOCK_SRC_FIRC,
                .divCore = SCG_SYSTEM_CLOCK_DIV_BY_1,
                .divBus = SCG_SYSTEM_CLOCK_DIV_BY_1,
                .divSlow = SCG_SYSTEM_CLOCK_DIV_BY_2,
            },
            .vccrConfig =
            {
                .src = SCG_SYSTEM_CLOCK_SRC_SIRC,
                .divCore = SCG_SYSTEM_CLOCK_DIV_BY_2,
                .divBus = SCG_SYSTEM_CLOCK_DIV_BY_1,
                .divSlow = SCG_SYSTEM_CLOCK_DIV_BY_4,
            },
            .hccrConfig =
            {
                .src = SCG_SYSTEM_CLOCK_SRC_SYS_PLL,
                .divCore = SCG_SYSTEM_CLOCK_DIV_BY_1,
                .divBus = SCG_SYSTEM_CLOCK_DIV_BY_2,
                .divSlow = SCG_SYSTEM_CLOCK_DIV_BY_4,
            },
        },
    },
    .pccConfig =
    {
        .peripheralClocks = peripheralClockConfig0,
        .count = NUM_OF_PERIPHERAL_CLOCKS_0,
    },
    .simConfig =
    {
        .clockOutConfig =
        {
            .initialize = true,
            .enable = true,
            .source = SIM_CLKOUT_SEL_SYSTEM_SCG_CLKOUT,
            .divider = SIM_CLKOUT_DIV_BY_1,
        },
        .lpoClockConfig =
        {
            .initialize = true,
            .enableLpo1k = true,
            .enableLpo32k = true,
            .sourceLpoClk = SIM_LPO_CLK_SEL_LPO_128K,
            .sourceRtcClk = SIM_RTCCLK_SEL_SOSCDIV1_CLK,
        },
        .platGateConfig =
        {
            .initialize = true,
            .enableEim = true,
            .enableErm = true,
            .enableDma = true,
            .enableMpu = true,
            .enableMscm = true,
        },
        .tclkConfig =
        {
            .initialize = true,
            .tclkFreq[0] = 2000000U,
            .tclkFreq[1] = 0U,
            .tclkFreq[2] = 0U,
            .extPinSrc[0] = 0U,
            .extPinSrc[1] = 0U,
            .extPinSrc[2] = 0U,
            .extPinSrc[3] = 0U,
        },
        .traceClockConfig =
        {
            .initialize = true,
            .divEnable = true,
            .source = CLOCK_TRACE_SRC_CORE_CLK,
            .divider = 0U,
            .divFraction = false,
        },
    },
    .pmcConfig =
    {
        .lpoClockConfig =
        {
            .initialize = true,
            .enable = true,
            .trimValue = 0,
        },
    },
};

/*! @brief Array of pointers to User configuration structures */
clock_manager_user_config_t const *g_clockManConfigsArr[] = {
    &clockMan1_InitConfig0
};

/*! @brief Array of pointers to User defined Callbacks configuration structures */
clock_manager_callback_user_config_t *g_clockManCallbacksArr[] = {(void *)0};
