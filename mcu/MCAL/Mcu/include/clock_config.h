#ifndef CLOCK_CONFIG_H
#define CLOCK_CONFIG_H

#include "clock_manager.h"
#include <stdbool.h>
#include <stdint.h>

/*! @brief Count of user configuration structures */
#define CLOCK_MANAGER_CONFIG_CNT       1U

/*! @brief Count of user Callbacks structures */
#define CLOCK_MANAGER_CALLBACK_CNT     0U

/*! @brief Count of peripheral clock user configuration 0 */
#define NUM_OF_PERIPHERAL_CLOCKS_0     32U

/*! @brief User configuration structure 0 */
extern clock_manager_user_config_t clockMan1_InitConfig0;

/*! @brief User peripheral configuration structure 0 */
extern peripheral_clock_config_t peripheralClockConfig0[NUM_OF_PERIPHERAL_CLOCKS_0];

/*! @brief Array of User callbacks */
extern clock_manager_callback_user_config_t *g_clockManCallbacksArr[];

/*! @brief Array of pointers to User configuration structures */
extern clock_manager_user_config_t const *g_clockManConfigsArr[];

#endif /* CLOCK_CONFIG_H */
