# STM32G0 Firmware for Solari Cifra5 Clock

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC_BY--NC--SA_4.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Documentation](https://img.shields.io/badge/docs-Doxygen-blue.svg)](https://alcor6502.github.io/Solari-Cifra5-Firmware/)

## Follow the project on [Hackaday](https://hackaday.io/project/183679-solari-cifra-5-hack)

FreeRTOS firmware for the STM32G031K8 that drives a Solari Cifra 5 mechanical flip clock. Controls hour flaps via servo, minute flaps via electromagnetic coil, and provides a full UI on an SSD1306 OLED display with 3 tactile buttons.

### Hardware

- **MCU** — STM32G031K8 (Cortex-M0+) on Nucleo-32 board, MCP100 supervisor, coin cell for RTC backup
- **Coil drive** — DRV8871 H-bridge drives the original Solari electromagnetic coil with alternating polarity pulses
- **Hour servo** — MG996R high-torque servo advances the hour lever at :00, with BSS138K level shifter
- **Sensors** — 2x A1344 Hall effect sensors with magnets in 3D-printed PETG holders (fully non-invasive)
- **Display** — SSD1306 128x64 I2C OLED with buffer-less driver
- **Power** — USB → current-limited switch (1.4A) → TPS61041 boost to 24V (coil) + LDO 3.3V (MCU/display)

### Features

- **3 FreeRTOS tasks** — display (UI state machine), button (scan + debounce + long press), clock (sync + tick)
- **Mechanical synchronization** — sensor-based zero search and fast re-sync from backup registers
- **Silent period** — configurable hours when the mechanism stays quiet (e.g. 22:00-09:00)
- **RTC smooth calibration** — adjustable crystal compensation (0-511 pulses per 32s window)
- **Settings persistence** — silent hours and calibration stored in Flash, restored on battery loss
- **First-boot setup wizard** — guides through silent hours, calibration, and time setting

### Firmware Architecture

The firmware is organized into focused modules:

| Module | Description |
|--------|-------------|
| `rtos_init` | Peripheral handles, global definitions, task creation |
| `display_task` | OLED display controller with 6-state UI state machine |
| `button_task` | 3-button scanner with debounce and long press detection |
| `clock_task` | Mechanical synchronization, servo/coil control, minute ticking |
| `rtc_helpers` | RTC backup registers, Flash persistence, calibration, silent period |
| `ssd1306` | Buffer-less I2C display driver with scalable font rendering |

Inter-task communication uses `xTaskNotify` exclusively — no queues or mutexes.

### User Interface

From the clock display, three long-press actions enter sub-menus:

| Long press | Sub-menu | Format |
|------------|----------|--------|
| SET | Set time | HH:MM digit-by-digit editing |
| INC | Silent hours | HH-HH start and end hours |
| DEC | RTC calibration | ±NNN smooth calibration value |

On first boot, a setup wizard chains all three screens automatically. The display auto-powers off after a timeout; any button press wakes it without triggering an action.

### Notes

This implementation doesn't use the CMSIS FreeRTOS provided by the STM32Cube IDE; I studied all the functions from the FreeRTOS manual, so I found the CMSIS wrapper confusing.

The display driver is an evolution of the one developed by Stefan Wagner, subsequently improved, and ported to STM32 by me. The main characteristic of this driver is that it doesn't require a display memory map inside the MCU, so in a tight memory configuration like this one, it can be handy. If you are interested in its functionality, I suggest reading the full article published [here](https://hackaday.io/project/181543-no-buffer-ssd1306-display-driver-for-stm32) on Hackaday.

## License

Licensed under **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International** (CC BY-NC-SA 4.0). See [LICENSE](https://creativecommons.org/licenses/by-nc-sa/4.0/) for full license text.