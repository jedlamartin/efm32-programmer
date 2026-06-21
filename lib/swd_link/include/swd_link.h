#ifndef SWD_LINK_H
#define SWD_LINK_H

#include <stdint.h>

#include <type_traits>

#include "driver/gpio.h"
#include "soc/gpio_struct.h"

constexpr gpio_num_t PIN_SWDIO = GPIO_NUM_0;
constexpr gpio_num_t PIN_SWCLK = GPIO_NUM_1;

/**
 * @brief Represents physical wire ownership of the bi-directional SWDIO line.
 */
typedef enum {
    SWD_DIR_HOST = 0,     // ESP32 is driving the line (Output)
    SWD_DIR_TARGET = 1    // Target Gecko is driving the line (Input to ESP32)
} swd_dir_t;

/**
 * @brief Encapsulates a complete Serial Wire Debug transaction frame.
 */
struct swd_frame_t {
    bool is_ap;       // true for Access Port (AP), false for Debug Port (DP)
    bool is_read;     // true for Read operation, false for Write operation
    uint8_t addr;     // Target register address index (0x00, 0x04, 0x08, 0x0C)
    uint32_t data;    // Read destination or Write source payload
    swd_ack_t ack;    // Target response status populated during transaction
    bool parity_error;    // Flagged true if a data phase parity check fails
};

/**
 * @brief SWD Response ACK codes returned by the target.
 * Defined by the ARM Debug Interface (ADIv5) specification.
 */
enum swd_ack_t {
    SWD_ACK_OK = 0x01,      // Target successfully accepted the transaction
    SWD_ACK_WAIT = 0x02,    // Target is busy, transaction must be retried
    SWD_ACK_FAULT = 0x04    // Protocol or parity error, sticky fault set
};

// ============================================================================
// LAYER 0: Physical Hardware Control
// ============================================================================

/**
 * @brief Configures the ESP32 GPIO pins for SWD (SWCLK as output, SWDIO as
 * output). Sets initial line states (Clock low, Data high).
 */
void swd_init_hardware(void);

/**
 * @brief Manually toggles the GPIO direction of the SWDIO pin.
 * @param is_output true to set ESP32 as master (output), false for target
 * (input).
 */
void swd_set_dio_direction(swd_dir_t direction);

// ============================================================================
// LAYER 1: Wire-Level Protocol Sequences
// ============================================================================

/**
 * @brief Executes an SWD Line Reset sequence.
 * Clocks at least 50 continuous cycles with SWDIO held HIGH.
 */
void swd_line_reset(void);

/**
 * @brief Sends the explicit 16-bit JTAG-to-SWD switching sequence.
 * Typically 0x79E7 (transmitted LSB first).
 */
void swd_switch_jtag_to_swd(void);

// ============================================================================
// LAYER 2: Unified Transaction Engine
// ============================================================================

/**
 * @brief The core packet runner. Handles request, turnaround, ACK, and data
 * phases.
 * @param is_ap     true if targeting an Access Port (AP), false for Debug Port
 * (DP).
 * @param is_read   true for a read operation, false for a write operation.
 * @param addr      The 2-bit register address (A[3:2]). Valid values: 0x00,
 * 0x04, 0x08, 0x0C.
 * @param data      Pointer to data payload (read destination or write source).
 * @param ack_out   Pointer to store the 3-bit ACK response returned by the
 * target.
 * @return true if the transaction completed with SWD_ACK_OK, false otherwise.
 */
bool swd_transaction(swd_frame_t* frame);

// ============================================================================
// LAYER 3: High-Level DP/AP Abstraction (Exposed to Flash Driver)
// ============================================================================

/**
 * @brief Reads a 32-bit register from the Debug Port (DP).
 * Automatically maps to the base swd_transaction setup.
 */
bool swd_read_dp(uint8_t addr, uint32_t* value);

/**
 * @brief Writes a 32-bit register to the Debug Port (DP).
 */
bool swd_write_dp(uint8_t addr, uint32_t value);

/**
 * @brief Reads a 32-bit register from the currently selected Access Port (AP).
 * Note: Assumes the correct AP bank is already selected via the DP SELECT
 * register.
 */
bool swd_read_ap(uint8_t addr, uint32_t* value);

/**
 * @brief Writes a 32-bit register to the currently selected Access Port (AP).
 */
bool swd_write_ap(uint8_t addr, uint32_t value);

/**
 * @brief Forcefully clears any sticky fault flags (STICKYERR, STICKYCMP, etc.)
 * by writing to the DP ABORT register. Essential for recovering from ACK_FAULT
 * loops.
 * @return true if the clear operation succeeded.
 */
bool swd_clear_errors(void);

#endif    // SWD_LINK_H