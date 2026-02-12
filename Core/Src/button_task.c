/**
 * @file   button_task.c
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

#include "rtos_init.h"
#include "button_task.h"


/**
 * @brief  FreeRTOS task: scan 3 tactile buttons with debounce and long press.
 *
 *         Runs at BTN_TASK_DELAY (5ms) interval using vTaskDelayUntil for
 *         precise periodic execution. Reads the 3 GPIO pins (SET, INC, DEC),
 *         debounces them (BTN_DEBOUNCE = 20ms), and detects short vs long press.
 *
 *         Button protocol:
 *         - Buttons are active-low (pressed=0, released=1)
 *         - Only one button can be held at a time (multi-press rejected)
 *         - Short press: fires on RELEASE if held < BTN_LONG_PRESS_TIME (1s)
 *           → sends notification 101+i (DISP_EV_BTN_SET/INC/DEC)
 *         - Long press: fires while HOLDING after BTN_LONG_PRESS_TIME elapsed
 *           → sends notification 104+i (DISP_EV_BTN_SET/INC/DEC_LONG)
 *         - After sending any notification, the task suspends itself
 *           (vTaskSuspend(NULL)) and waits for displayTask to resume it
 *           after processing the event
 *
 *         Debounce algorithm: when a pin change is detected, record the tick
 *         and set inDbnc=1. On subsequent scans, if BTN_DEBOUNCE ms have
 *         elapsed and the pin is still in the new state, confirm the change.
 *
 *         vTaskDelayUntil(&lastWakeTime, ticks): blocks until an absolute
 *         time, ensuring the task runs at a fixed frequency regardless of
 *         how long the scan loop takes. Unlike vTaskDelay (relative), this
 *         prevents drift accumulation over many cycles.
 *
 *         timeLapsed(a, b): macro that computes (a - b) using signed
 *         arithmetic to handle tick counter wraparound correctly.
 *
 * @param  parameters  Unused (NULL)
 */
void buttonTask(void *parameters) {

	tactButton_t tactButton[BTN_MAX];
	int i, j, alloff;
	TickType_t last_wakeup_time;
	uint8_t heldButton = BTN_MAX;	// Which button is held (BTN_MAX = none)
	TickType_t pressTime = 0;		// When the press started
	uint8_t longPressSent = 0;		// Prevent double-send

	// Variables initialization
	last_wakeup_time = xTaskGetTickCount();

	for (i = 0; i < BTN_MAX; i++) {
		tactButton[i].status = 1;  // Buttons when are pressed have a logic value 0
		tactButton[i].inDbnc = 0;
		tactButton[i].tikCnt = xTaskGetTickCount();
	}

	while (1) {
		// Scan if any button has been pressed
		tactButton[BTN_SET].actual = HAL_GPIO_ReadPin(BTN_SET_GPIO_Port, BTN_SET_Pin);
		tactButton[BTN_INC].actual = HAL_GPIO_ReadPin(BTN_INC_GPIO_Port, BTN_INC_Pin);
		tactButton[BTN_DEC].actual = HAL_GPIO_ReadPin(BTN_DEC_GPIO_Port, BTN_DEC_Pin);

		for (i = 0; i < BTN_MAX; i++) {
			if ((tactButton[i].actual != tactButton[i].status) && (tactButton[i].inDbnc == 0)) {
				tactButton[i].tikCnt = xTaskGetTickCount();
				tactButton[i].inDbnc = 1;
			}

			if (timeLapsed(xTaskGetTickCount(), tactButton[i].tikCnt) > pdMS_TO_TICKS(BTN_DEBOUNCE)) {
				tactButton[i].inDbnc = 0;
				if (tactButton[i].actual != tactButton[i].status) {
					if (tactButton[i].actual == 0) {  // Button pressed (logic low)
						alloff = 0;
						for (j = 0; j < BTN_MAX; j++) {
							alloff += tactButton[j].status;
						}
						if (alloff == BTN_MAX) {  // No other button pressed
							tactButton[i].status = tactButton[i].actual;
							heldButton = i;
							pressTime = xTaskGetTickCount();
							longPressSent = 0;
						}
					} else {  // Button released (logic high)
						tactButton[i].status = tactButton[i].actual;
						if (i == heldButton) {
							if (!longPressSent) {
								heldButton = BTN_MAX;
								xTaskNotify(displayTaskHandle, (uint32_t)(101 + i), eSetValueWithOverwrite);
								vTaskSuspend(NULL);
							} else {
								heldButton = BTN_MAX;
							}
						}
					}
				}
			}
		}  // For loop end

		// Long press detection (after scanning all buttons)
		if (heldButton < BTN_MAX && !longPressSent
				&& timeLapsed(xTaskGetTickCount(), pressTime) >= pdMS_TO_TICKS(BTN_LONG_PRESS_TIME)) {
			longPressSent = 1;
			xTaskNotify(displayTaskHandle, (uint32_t)(104 + heldButton), eSetValueWithOverwrite);
			vTaskSuspend(NULL);
		}

		vTaskDelayUntil(&last_wakeup_time, pdMS_TO_TICKS(BTN_TASK_DELAY));

	} // While loop end
}
