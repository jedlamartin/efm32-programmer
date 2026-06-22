#include "efr32_msc.h"

static const char* TAG = "EFR32_MSC";

// Core Debug registers from the ARMv7-M architectural block
constexpr uint32_t REG_DHCSR = 0xE000EDF0;
constexpr uint32_t REG_DEMCR = 0xE000EDFC;
constexpr uint32_t REG_AIRCR = 0xE000ED0C;

/**
 * @brief Private block to poll the status register until the hardware is idle.
 */
static bool msc_wait_until_ready(void) {
    uint32_t status = 0;
    // Timeout tracking loop to prevent hanging the host indefinitely
    for(size_t timeout = 0; timeout < 2000; ++timeout) {
        if(!swd_read_mem32(MSC_STATUS, &status)) {
            return false;
        }
        // If the BUSY bit is 0, the internal flash sequencer is idle
        if((status & MSC_STATUS_BUSY) == 0) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    ESP_LOGE(TAG, "MSC Operation Timeout Failure!");
    return false;
}

bool efr32_msc_halt_target(void) {
    ESP_LOGI(TAG, "Isolating target core execution state...");
    if(!swd_write_mem32(REG_DHCSR, 0xA05F0003)) return false;

    // 2. Set the VC_CORERESET bit in DEMCR to intercept the system reboot cycle
    uint32_t demcr_val = 0;
    if(!swd_read_mem32(REG_DEMCR, &demcr_val)) return false;
    demcr_val |= (1 << 0);
    if(!swd_write_mem32(REG_DEMCR, demcr_val)) return false;

    // 3. Command an immediate local core reset via AIRCR [cite: 1698]
    if(!swd_write_mem32(REG_AIRCR, 0xFA050004)) return false;

    ESP_LOGI(TAG, "Target core trapped successfully in HALT-ON-RESET mode.");
    return true;
}

bool efr32_msc_erase_page(uint32_t page_address) {
    // Basic alignment validation guard
    if(page_address % EFR32_PAGE_SIZE != 0) {
        ESP_LOGE(TAG,
                 "Erase rejected: Address 0x%08lX is not page-aligned!",
                 page_address);
        return false;
    }

    if(!swd_write_mem32(MSC_LOCK, MSC_UNLOCK_KEY)) return false;
    if(!swd_write_mem32(MSC_WRITECTRL, MSC_WRITECTRL_WREN)) return false;

    if(!msc_wait_until_ready()) return false;

    if(!swd_write_mem32(MSC_ADDRB, page_address)) return false;
    if(!swd_write_mem32(MSC_WRITECMD, MSC_CMD_LADDRIM)) return false;
    if(!swd_write_mem32(MSC_WRITECMD, MSC_CMD_ERASEPAGE)) return false;

    return msc_wait_until_ready();
}

bool efr32_msc_write_word(uint32_t address, uint32_t data) {
    // Basic word boundaries verification guard
    if(address % 4 != 0) {
        ESP_LOGE(TAG,
                 "Write rejected: Address 0x%08lX is not 32-bit aligned!",
                 address);
        return false;
    }

    if(!swd_write_mem32(MSC_LOCK, MSC_UNLOCK_KEY)) return false;
    if(!swd_write_mem32(MSC_WRITECTRL, MSC_WRITECTRL_WREN)) return false;

    if(!msc_wait_until_ready()) return false;

    if(!swd_write_mem32(MSC_ADDRB, address)) return false;
    if(!swd_write_mem32(MSC_WRITECMD, MSC_CMD_LADDRIM)) return false;
    if(!swd_write_mem32(MSC_WDATA, data)) return false;
    if(!swd_write_mem32(MSC_WRITECMD, MSC_CMD_WRITEONCE)) return false;

    return msc_wait_until_ready();
}
