# efr32-programmer

An ESP32 bit-banged Serial Wire Debug (SWD) programmer for Silicon Labs EFR32 Wireless Gecko SoCs (Cortex-M4 Core), built natively using the **ESP-IDF framework** inside PlatformIO.

This project implements the low-level ARM Debug Interface Architecture Specification (ADIv5) and pairs it with a custom flash driver targeting the Silicon Labs **Memory System Controller (MSC)** to perform hardware halt-on-reset isolation, page erasure, flash word writing, and validation check cycles.

---

## Architecture

The project splits the physical transport and system-level flash algorithms into distinct, isolated layers configured as independent components:

```text
.
├── lib/
│   ├── swd_link/          # Core SWD Protocol & ARM DP/AP Transactions
│   │   ├── include/       # Link Layer headers (DAP layout primitives)
│   │   └── src/           # Bit-banged SWDIO/SWCLK timing & transaction serialization
│   └── silabs_msc/        # Silicon Labs Flash Controller Driver
│       ├── include/       # Register definitions for EFR32 Series 1 MSC block
│       └── src/           # Target halting, page erase, and flash programming state machines
├── src/
│   └── main.cpp           # System orchestrator & automated verification script
├── platformio.ini         # Environment manager and compiler config
└── CMakeLists.txt         # Root project build configuration
```

### Firmware Stack Breakdown

* **Physical Layer (`swd_link`):** Raw GPIO register optimization (`GPIO.out_w1ts` / `GPIO.out_w1tc`) executing clock stretching via inline assembly `nop` pads to control transition timing.
* **Wire Layer (`swd_link`):** Packages commands into standard LSB-first layouts, managing explicit clock turnaround phases (`Trn`) and decoding 3-bit acknowledgment frames (`ACK`).
* **Transaction Layer (`swd_link`):** Formulates 8-bit request headers with runtime parity verification to manage communication with the ARM Debug Port (`DP`) and Access Port (`AP`).
* **Memory Access Layer (`swd_link`):** Programs the internal AHB-AP bridge using the Control/Status Word register (`CSW = 0x22000002`) to handle 32-bit privileged system bus transfers. It automatically accounts for internal ARM bus pipelining delays by immediately pairing AP reads with a secondary read to the DP `RDBUFF` latch.
* **Peripheral Driver Layer (`silabs_msc`):** Directs the target's internal flash configuration matrix by managing the sequential lock bypass sequence, unlocking the peripheral bus with the specific passkey (`0x1B71`), and driving the write/erase commands.