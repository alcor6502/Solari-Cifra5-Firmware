# STM32G0 Firmware for Solari Cifra5 Clock

## Follow the project on [Hackaday](https://hackaday.io/project/183679-solari-cifra-5-hack)

This implementation doesn't use the CMSIS FreeRtos provided by the STM32Cube IDE; I studied all the functions from the FreeRtos manual, so I found the CMSIS wrapper confusing.

The functionality to control the clock is quite simple; The human interface required to set the clock time represents most of the code. I still have to implement the clock frequency adjustment menu at the moment, I assigned the value directly in the source code.

The display driver is an evolution of the one developed by Stefan Wagner, subsequently improved, and ported to STM32 by me. The main characteristic of this driver is that it doesn't require a display memory map inside the MCU, so in a tight memory configuration like this one, it can be handy. If you are interested in its functionality, I suggest reading the full article published [here](https://hackaday.io/project/181543-no-buffer-ssd1306-display-driver-for-stm32) on Hackaday.