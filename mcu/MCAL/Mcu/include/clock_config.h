#ifndef CLOCK_CONFIG_H
#define CLOCK_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "clock_manager.h"
#include "device_registers.h"

extern clock_manager_user_config_t clockMan1_InitConfig0;
#define CLOCK_MANAGER_CONFIG_CNT         1U
extern clock_manager_user_config_t const *g_clockManConfigsArr[];
extern peripheral_clock_config_t peripheralClockConfig0[];
#define NUM_OF_PERIPHERAL_CLOCKS_0       17U
#define CLOCK_MANAGER_CALLBACK_CNT       0U
extern clock_manager_callback_user_config_t *g_clockManCallbacksArr[];

#endif
