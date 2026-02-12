# STM32G0 Firmware for Solari Cifra5 Clock

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC_BY--NC--SA_4.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Documentation](https://img.shields.io/badge/docs-Doxygen-blue.svg)](https://alcor6502.github.io/Solari-Cifra5-Firmware/)

## Follow the project on [Hackaday](https://hackaday.io/project/183679-solari-cifra-5-hack)

FreeRTOS firmware for the STM32G031K8 that drives a Solari Cifra 5 mechanical flip clock. Controls hour flaps via servo, minute flaps via electromagnetic coil, and provides a full UI on an SSD1306 OLED display with 3 tactile buttons.

### Features

- **3 FreeRTOS tasks** — display (UI state machine), button (scan + debounce + long press), clock (sync + tick)
- **Mechanical synchronization** — sensor-based zero search and fast re-sync from backup registers
- **Silent period** — configurable hours when the mechanism stays quiet (e.g. 22:00-09:00)
- **RTC smooth calibration** — adjustable crystal compensation (0-511 pulses per 32s window)
- **Settings persistence** — silent hours and calibration stored in Flash, restored on battery loss
- **First-boot setup wizard** — guides through silent hours, calibration, and time setting

### Notes

This implementation doesn't use the CMSIS FreeRTOS provided by the STM32Cube IDE; I studied all the functions from the FreeRTOS manual, so I found the CMSIS wrapper confusing.

The display driver is an evolution of the one developed by Stefan Wagner, subsequently improved, and ported to STM32 by me. The main characteristic of this driver is that it doesn't require a display memory map inside the MCU, so in a tight memory configuration like this one, it can be handy. If you are interested in its functionality, I suggest reading the full article published [here](https://hackaday.io/project/181543-no-buffer-ssd1306-display-driver-for-stm32) on Hackaday.

## License

Licensed under **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International** (CC BY-NC-SA 4.0). See [LICENSE](https://creativecommons.org/licenses/by-nc-sa/4.0/) for full license text.