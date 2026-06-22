#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#include "driver/gpio.h"

// ============================================================================
// SWD Programmer Pin Assignments (ESP32 Side)
// ============================================================================

/**
 * @brief The Serial Wire Debug Data I/O pin.
 * This pin changes direction dynamically during transactions.
 */
constexpr gpio_num_t PIN_SWDIO = GPIO_NUM_0;

/**
 * @brief The Serial Wire Debug Clock pin.
 * Driven strictly as an output by the ESP32 host.
 */
constexpr gpio_num_t PIN_SWCLK = GPIO_NUM_1;

#endif    // PIN_CONFIG_H