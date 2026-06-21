#include "swd_link.h"

static inline void swd_clk_high() { GPIO.out_w1ts = (1ULL << PIN_SWCLK); }
static inline void swd_clk_low() { GPIO.out_w1tc = (1ULL << PIN_SWCLK); }
static inline void swd_dio_high() { GPIO.out_w1ts = (1ULL << PIN_SWDIO); }
static inline void swd_dio_low() { GPIO.out_w1tc = (1ULL << PIN_SWDIO); }
static inline bool swd_dio_read() { return (GPIO.in >> PIN_SWDIO) & 1; }

/**
 * @brief Ultra-short clock stretching via CPU assembly NOPs.
 * Prevents overrunning older or unconfigured ARM cores.
 */
static inline void swd_delay() { asm volatile("nop; nop; nop; nop;"); }

/**
 * @brief Private helper to clock out a single bit.
 * Target registers data on the rising edge of SWCLK.
 */
static inline void swd_write_bit(bool bit) {
    if(bit) {
        swd_dio_high();
    } else {
        swd_dio_low();
    }

    swd_delay();
    swd_clk_high();
    swd_delay();
    swd_clk_low();
}

/**
 * @brief Serializes any integral data type out to the SWDIO line, LSB-first.
 * @tparam T An unsigned integral type (uint8_t, uint16_t, uint32_t).
 * @param data The payload value to transmit.
 */
template<typename T>
static inline void swd_write_payload(T data) {
    static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>,
                  "swd_write_payload requires an unsigned integral type.");
    constexpr size_t bitcount = sizeof(T) * 8;
    for(size_t i = 0; i < bitcount; ++i) {
        swd_write_bit((data >> i) & 1);
    }
}

void swd_init_hardware(void) {
    const gpio_config_t clk_config {.pin_bit_mask = (1ULL << PIN_SWCLK),
                                    .mode = GPIO_MODE_OUTPUT,
                                    .pull_up_en = GPIO_PULLUP_DISABLE,
                                    .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                    .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&clk_config);

    const gpio_config_t dio_config {.pin_bit_mask = (1ULL << PIN_SWDIO),
                                    .mode = GPIO_MODE_INPUT_OUTPUT,
                                    .pull_up_en = GPIO_PULLUP_ENABLE,
                                    .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                    .intr_type = GPIO_INTR_DISABLE};

    gpio_config(&dio_config);

    // Default line idle states
    swd_clk_low();
    swd_dio_high();
}

void swd_set_dio_direction(swd_dir_t direction) {
    if(direction == SWD_DIR_HOST) {
        GPIO.enable_w1ts = (1ULL << PIN_SWDIO);
    } else {
        GPIO.enable_w1tc = (1ULL << PIN_SWDIO);
    }
}

void swd_line_reset(void) {
    swd_set_dio_direction(SWD_DIR_HOST);

    constexpr size_t reset_cycles = 60;    // Should be more than 50
    for(size_t i = 0; i < reset_cycles; ++i) {
        swd_write_bit(true);
    }
}

void swd_switch_jtag_to_swd(void) {
    swd_set_dio_direction(SWD_DIR_HOST);

    constexpr uint16_t seq = 0xE79E;

    swd_write_payload(seq);
}

bool swd_transaction(swd_frame_t* frame) {
    if(!frame) return false;

    uint8_t ap_bit = frame->is_ap ? 1 : 0;
    uint8_t rnw_bit = frame->is_read ? 1 : 0;
    uint8_t addr2_bit = (frame->addr >> 2) & 1;
    uint8_t addr3_bit = (frame->addr >> 3) & 1;
    uint8_t parity = ap_bit ^ rnw_bit ^ addr2_bit ^ addr3_bit;

    uint8_t request = (1 << 0) | (ap_bit << 1) | (rnw_bit << 2) |
                      (addr2_bit << 3) | (addr3_bit << 4) | (parity << 5) |
                      (0 << 6) | (1 << 7);

    // 2. Transmit request
    swd_set_dio_direction(SWD_DIR_HOST);
    swd_write_payload(request);

    // 3. Turnaround to Target
    swd_delay();
    swd_clk_high();
    swd_delay();
    swd_clk_low();

    // 4. Read ACK directly into the struct context
    uint8_t ack_val = 0;
    for(size_t i = 0; i < 3; ++i) {
        swd_delay();
        swd_clk_high();
        swd_delay();
        if(swd_dio_read()) ack_val |= (1 << i);
        swd_clk_low();
    }
    frame->ack = (swd_ack_t) ack_val;

    if(frame->ack != SWD_ACK_OK) {
        // Return line control cleanly on failure
        swd_delay();
        swd_clk_high();
        swd_delay();
        swd_clk_low();
        swd_set_dio_direction(SWD_DIR_HOST);
        return false;
    }

    // 5. Run Data phase
    if(frame->is_read) {
        uint32_t read_val = 0;
        uint8_t calc_parity = 0;

        for(size_t i = 0; i < 32; ++i) {
            swd_delay();
            swd_clk_high();
            swd_delay();
            if(swd_dio_read()) {
                read_val |= (1UL << i);
                calc_parity ^= 1;
            }
            swd_clk_low();
        }

        swd_delay();
        swd_clk_high();
        swd_delay();
        bool target_parity = swd_dio_read();
        swd_clk_low();

        swd_delay();
        swd_clk_high();
        swd_delay();
        swd_clk_low();
        swd_set_dio_direction(SWD_DIR_HOST);

        frame->parity_error = (calc_parity != target_parity);
        if(frame->parity_error) return false;

        frame->data = read_val;
    } else {
        swd_delay();
        swd_clk_high();
        swd_delay();
        swd_clk_low();
        swd_set_dio_direction(SWD_DIR_HOST);

        uint32_t write_val = frame->data;
        uint8_t calc_parity = 0;

        for(size_t i = 0; i < 32; ++i) {
            bool bit = (write_val >> i) & 1;
            swd_write_bit(bit);
            calc_parity ^= (bit ? 1 : 0);
        }
        swd_write_bit(calc_parity);
    }

    // 6. Clock trailing idle cycles
    swd_dio_low();
    for(size_t i = 0; i < 8; ++i) {
        swd_delay();
        swd_clk_high();
        swd_delay();
        swd_clk_low();
    }

    return true;
}
