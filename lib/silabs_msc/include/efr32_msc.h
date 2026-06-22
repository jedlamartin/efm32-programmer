#ifndef EFR32_MSC_H
#define EFR32_MSC_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "swd_link.h"

// ============================================================================
// EFR32 Series 1 Memory System Controller (MSC) Register Map
// ============================================================================

constexpr uint32_t MSC_BASE = 0x400E0000;

constexpr uint32_t MSC_WRITECTRL = MSC_BASE + 0x008;    // Write Control
constexpr uint32_t MSC_WRITECMD = MSC_BASE + 0x00C;     // Write Command
constexpr uint32_t MSC_ADDRB = MSC_BASE + 0x010;        // Address Buffer
constexpr uint32_t MSC_WDATA = MSC_BASE + 0x018;        // Write Data
constexpr uint32_t MSC_STATUS = MSC_BASE + 0x01C;       // Controller Status
constexpr uint32_t MSC_LOCK = MSC_BASE + 0x040;         // Configuration Lock

// ============================================================================
// Bit Definitions
// ============================================================================

constexpr uint32_t MSC_UNLOCK_KEY = 0x1B71;

// MSC_STATUS Register Bits
constexpr uint32_t MSC_STATUS_BUSY = (1 << 0);    // Flash operation in progress

// MSC_WRITECTRL Register Bits
constexpr uint32_t MSC_WRITECTRL_WREN = (1 << 0);    // Write Enable bit

// MSC_WRITECMD Register Bits
constexpr uint32_t MSC_CMD_LADDRIM = (1 << 0);      // Load Address Immediate
constexpr uint32_t MSC_CMD_ERASEPAGE = (1 << 1);    // Erase Page Command
constexpr uint32_t MSC_CMD_WRITEONCE = (1 << 3);    // Write Once Command

// Target Flash Parameters
constexpr uint32_t EFR32_PAGE_SIZE = 2048;    // EFR32MG12 page size is 2 kB

// ============================================================================
// API Function Prototyping
// ============================================================================

/**
 * @brief Performs core halt-on-reset sequence to safely isolate the target.
 */
bool efr32_msc_halt_target(void);

/**
 * @brief Erases an entire flash sector/page on the EFR32 target.
 */
bool efr32_msc_erase_page(uint32_t page_address);

/**
 * @brief Writes a single 32-bit word directly to a specified flash address.
 */
bool efr32_msc_write_word(uint32_t address, uint32_t data);

#endif    // EFR32_MSC_H