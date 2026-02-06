/**
 2020 by Stefan Wagner
 Project Files (Github): https://github.com/wagiminator/ATtiny13-TinyOLEDdemo
 License: http://creativecommons.org/licenses/by-sa/3.0/

 Adapted for STM32 and implemented font magnification
 by Alfredo Cortellini May 2021
**/

#ifndef _SSD1306_H_
#define _SSD1306_H_

#include "main.h"

// I2C parameters
#define SSD1306_I2C_ADDR        0x3C << 1 // Alternate address 0x3D - When shifted 0x78 and 0x7A
#define SSD1306_I2C_TIMEOUT     10
#define SSD1306_I2C_CMD         0x00
#define SSD1306_I2C_DATA        0x40

// Display dimensions
#define SSD1306_WIDTH           128
#define SSD1306_HEIGHT          64

// Display flip Screen
#define SSD1306_INIT_LEN		20 // 18: no screen flip, 20: screen flip

// Single character  width
#define SSD1306_CHAR_WIDTH		6

//  Function declaration
void ssd1306_Init(I2C_HandleTypeDef *hi2c);			// Init function pass the pointer to the I2C handle structure
void ssd1306_ClearScreen(void);						// Clear Screen
void ssd1306_WriteChar(char ch, uint8_t fsize);		// Font size 0: 5x8, 1: 10x16, 2: 15x24 3: 20x32
void ssd1306_WriteString(char *msg, uint8_t fsize);	// Font size 0: 5x8, 1: 10x16, 2: 15x24 3: 20x32
void ssd1306_SetCursor(uint8_t xpos, uint8_t ypos); // Vertical value is with increments of 8 pixels
void ssd1306_SetContrast(uint8_t contrast);			// Set the display contrast 0 - 255
void ssd1306_SetDisplayOnOff(uint8_t onOff); 		// 1 = Display on, 0 = Display off

#endif  // _SSD1306_H_
