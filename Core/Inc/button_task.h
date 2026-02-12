/**
 * @file   button_task.h
 * @brief  Tactile button scanning with debounce and long press detection.
 *
 * @version 2.0
 * @date    12/02/2026
 * @author  Alfredo Cortellini
 *
 * @copyright Copyright (c) 2026 Alfredo Cortellini.
 *            Licensed under CC BY-NC-SA 4.0.
 *            See https://creativecommons.org/licenses/by-nc-sa/4.0/
 */

#ifndef _BUTTON_TASK_H_
#define _BUTTON_TASK_H_

#include "rtos_init.h"

void buttonTask(void *parameters);

/*
 *   Button Task Structure and definitions
 */
 
// Timeouts and delays
#define BTN_MAX				3 	// Remember to update this value with the actual number of Button installed on the board
#define BTN_DEBOUNCE		20 	// De-bounce time in multiple of 5ms
#define BTN_LONG_PRESS_TIME	1000	// Long press threshold in ms
#define BTN_TASK_DELAY		5 	// Frequency button scan

// Button data structure
typedef struct  {
	uint8_t actual;
	uint8_t status;
	uint8_t inDbnc;
	TickType_t tikCnt;
} tactButton_t;

// Button selection enumerator
enum btnFuncEnum{
	BTN_SET,
	BTN_INC,
	BTN_DEC
};
#endif /* _BUTTON_TASK_H_ */
