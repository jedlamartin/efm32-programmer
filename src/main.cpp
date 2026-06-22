#include <stdio.h>

#include "efr32_msc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "swd_link.h"

static const char* TAG = "MAIN_PROGRAMMER";

/**
 * @brief Performs the complete physical connection and initialization sequence
 * as specified in Section 2.1.2 of AN0062.
 */
bool target_connect(void) {
    ESP_LOGI(TAG, "Initializing SWD line states...");

    swd_line_reset();
    swd_switch_jtag_to_swd();
    swd_line_reset();

    // 4. Verify line integrity by pulling the physical core IDCODE
    uint32_t idcode = 0;
    if(!swd_read_dp(0x00, &idcode)) {
        ESP_LOGE(TAG,
                 "CRITICAL: Target failed to acknowledge connection request!");
        return false;
    }

    // Verify against standard ARM Cortex-M4 IDCODE signature (0x2BA01477)
    if(idcode == 0x2BA01477) {
        ESP_LOGI(TAG, "Successfully attached to EFR32 (Cortex-M4 Core)!");
    } else {
        ESP_LOGW(TAG, "Warning: Unknown IDCODE detected: 0x%08lX", idcode);
    }

    // 5. Power up the debug domain power rails inside the target core
    // We write to CTRL/STAT (Offset 0x04) requesting system and debug power
    if(!swd_write_dp(0x04, 0x50000000)) {
        ESP_LOGE(TAG, "Failed to power up target debug power domains.");
        return false;
    }

    return true;
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "=== Starting ESP32 Silicon Labs Programmer ===");

    swd_init_hardware();

    if(!target_connect()) {
        ESP_LOGE(TAG, "Aborting operation: Connection failed.");
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    if(!efr32_msc_halt_target()) {
        ESP_LOGE(TAG, "Failed to trap target core execution state.");
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    constexpr uint32_t TARGET_FLASH_ADDR = 0x00000000;
    constexpr uint32_t VERIFICATION_PAYLOAD = 0xABCDEF12;

    // 1. Perform Page Erase to clean the sector bits back to 0xFFFFFFFF
    ESP_LOGI(TAG,
             "Executing Flash Page Erase at address 0x%08lX...",
             TARGET_FLASH_ADDR);
    if(efr32_msc_erase_page(TARGET_FLASH_ADDR)) {
        ESP_LOGI(TAG, "Page erase completed successfully.");
    } else {
        ESP_LOGE(TAG, "Flash erase operation failed!");
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // 2. Perform Memory Write to commit the verification payload
    ESP_LOGI(TAG,
             "Writing Word payload 0x%08lX to address 0x%08lX...",
             VERIFICATION_PAYLOAD,
             TARGET_FLASH_ADDR);
    if(efr32_msc_write_word(TARGET_FLASH_ADDR, VERIFICATION_PAYLOAD)) {
        ESP_LOGI(TAG, "Flash write sequence completed.");
    } else {
        ESP_LOGE(TAG, "Flash write operation aborted due to bus faults.");
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // 3. Readback Verification Check
    ESP_LOGI(TAG,
             "Reading back address 0x%08lX over SWD bus lane...",
             TARGET_FLASH_ADDR);
    uint32_t readback_buffer = 0;
    if(swd_read_mem32(TARGET_FLASH_ADDR, &readback_buffer)) {
        if(readback_buffer == VERIFICATION_PAYLOAD) {
            ESP_LOGI(TAG, "=================================================");
            ESP_LOGI(TAG, "   SUCCESS! FLASH SECTOR VERIFIED PERFECTLY      ");
            ESP_LOGI(TAG,
                     "   Expected: 0x%08lX | Read: 0x%08lX",
                     VERIFICATION_PAYLOAD,
                     readback_buffer);
            ESP_LOGI(TAG, "=================================================");
        } else {
            ESP_LOGE(TAG, "CRITICAL: Flash verification mismatch error!");
            ESP_LOGE(TAG,
                     "Expected: 0x%08lX but read back: 0x%08lX",
                     VERIFICATION_PAYLOAD,
                     readback_buffer);
        }
    } else {
        ESP_LOGE(TAG,
                 "Failed to read target memory bus during verification stage.");
    }

    ESP_LOGI(TAG, "Driver entering passive monitor state.");
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}