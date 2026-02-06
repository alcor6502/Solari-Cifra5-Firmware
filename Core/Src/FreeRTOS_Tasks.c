/*
 * task_handlers.c
 *
 *  Created on: May 12, 2021
 *      Author: Alfredo Cortellini
 */


#include <string.h>

#include "FreeRTOS_Tasks.h"
#include "ssd1306.h"

/*
 *  MCU Peripherals handlers
 */
static TIM_HandleTypeDef *htimHandle = NULL;
static RTC_HandleTypeDef *hrtcHandle  = NULL;

// Struct to set and read the real time clock
static RTC_TimeTypeDef RTC_Time = { 0 };
static RTC_DateTypeDef RTC_Date = { 0 };

// Task Handles
static TaskHandle_t displayTaskHandle;
static TaskHandle_t buttonTaskHandle;
static TaskHandle_t clockTaskHandle;
static TaskHandle_t syncTaskHandle;

// Messages array
const char dispMsg[9][2][11] = {
		{DISP_MSG01_ROW01, DISP_MSG01_ROW02},
		{DISP_MSG02_ROW01, DISP_MSG02_ROW02},
		{DISP_MSG03_ROW01, DISP_MSG03_ROW02},
		{DISP_MSG04_ROW01, DISP_MSG04_ROW02},
		{DISP_MSG05_ROW01, DISP_MSG05_ROW02},
		{DISP_MSG06_ROW01, DISP_MSG06_ROW02},
		{DISP_MSG07_ROW01, DISP_MSG07_ROW02},
		{DISP_MSG08_ROW01, DISP_MSG08_ROW02},
		{DISP_MSG09_ROW01, DISP_MSG09_ROW02},
};

// Titles array
const char dispTitle[4][11] = {
		{DISP_TITLE01},
		{DISP_TITLE02},
		{DISP_TITLE03},
		{DISP_TITLE04},
};

const uint8_t setTimeDigitPos[4] = { // Array in which is stored the position of the digit same index
		DIGIT_TEEN_HRS_X,
		DIGIT_UNIT_HRS_X,
		DIGIT_TEEN_MINS_X,
		DIGIT_UNIT_MINS_X,
};


/*
 * ################################
 * #         DISPLAY TASK         #
 * ################################
 */

// Display the time with big digits
static void displayShowClock(uint8_t *time) {
	ssd1306_SetCursor(DIGIT_TEEN_HRS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[TEEN_HRS] + 48, DISP_FONT_L);
	ssd1306_SetCursor(DIGIT_UNIT_HRS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[UNIT_HRS] + 48, DISP_FONT_L);

	ssd1306_SetCursor(DIGIT_COLON_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(58, DISP_FONT_L);

	ssd1306_SetCursor(DIGIT_TEEN_MINS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[TEEN_MINS] + 48, DISP_FONT_L);
	ssd1306_SetCursor(DIGIT_UNIT_MINS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[UNIT_MINS] + 48, DISP_FONT_L);
}


// Update the array with the actual time
static void displayUpdateTimeVar(uint8_t *time) {
	time[TEEN_HRS] = RTC_Time.Hours / 10;
	time[UNIT_HRS] = RTC_Time.Hours % 10;
	time[TEEN_MINS] = RTC_Time.Minutes / 10;
	time[UNIT_MINS] = RTC_Time.Minutes % 10;
}


// Write the marker on the digit to be changed
//static void displayCursorSetTime(const uint8_t *digitPos, uint8_t digit, const uint8_t symbol) {
static void displayCursorSetTime(uint8_t digit, const uint8_t symbol) {

	ssd1306_SetCursor(setTimeDigitPos[digit], DIGIT_TIME_Y - 2);
	ssd1306_WriteChar(symbol, DISP_FONT_S);
	ssd1306_SetCursor(setTimeDigitPos[digit] + 6, DIGIT_TIME_Y - 2);
	ssd1306_WriteChar(symbol, DISP_FONT_S);
	ssd1306_SetCursor(setTimeDigitPos[digit] + 10, DIGIT_TIME_Y - 2);
	ssd1306_WriteChar(symbol, DISP_FONT_S);
}


//Display informative messages and clear the margins for odd/even message length
static void displayMessage(uint8_t msgId, char *buffer) {

	// Clear the margins on line 1
	ssd1306_SetCursor(3, 3);
	ssd1306_WriteChar(32, DISP_FONT_S);
	ssd1306_SetCursor(111, 3);
	ssd1306_WriteChar(32, DISP_FONT_S);

	// Write line 1 message
	ssd1306_SetCursor((SSD1306_WIDTH - (SSD1306_CHAR_WIDTH * 2 * strlen(dispMsg[msgId][0]))) / 2, 3);
	strcpy(buffer, dispMsg[msgId][0]);
	ssd1306_WriteString(buffer, DISP_FONT_S);

	// Clear the margins on line 2
	ssd1306_SetCursor(3, 6);
	ssd1306_WriteChar(32, DISP_FONT_S);
	ssd1306_SetCursor(111, 6);
	ssd1306_WriteChar(32, DISP_FONT_S);

	// Write line 2 message
	ssd1306_SetCursor((SSD1306_WIDTH - (SSD1306_CHAR_WIDTH * 2 * strlen(dispMsg[msgId][1]))) / 2, 6);
	strcpy(buffer, dispMsg[msgId][1]);
	ssd1306_WriteString(buffer, DISP_FONT_S);
}


// Print the title on of the display - no need to clean margins all titles are even
static void displayTitle(uint8_t msgId, char *buffer) {
	// Clear the margins
	ssd1306_SetCursor((SSD1306_WIDTH - (SSD1306_CHAR_WIDTH * 2 * strlen(dispTitle[msgId]))) / 2, 0);
	strcpy(buffer, dispTitle[msgId]);
	ssd1306_WriteString(buffer, DISP_FONT_S);
}


// Control display ON/OFF
static void displayOnOff(uint8_t newState, TickType_t *onTime) {
	static uint8_t oldState = OFF;
	if (newState != oldState) {
		ssd1306_SetDisplayOnOff(newState); // Toggle the display State
		oldState = newState;
	}
	if (newState == ON){
		*onTime = xTaskGetTickCount(); // Update display on time
	}
}


// #### Main Display Task ####
static void displayTask(void *parameters) {
	static char dispStringBuffer[11];  // buffer to hold the message
	uint8_t showTime[4]; // Array in which is stored the new clock time. index: 0=hour teens, 1=hour units, 2=min teens, 3=min units
	uint8_t dispState = DISP_SYNC;
	uint32_t eventId;
	uint8_t i = 0;
	TickType_t lastDisplayOnTime = xTaskGetTickCount();
	TickType_t lastDisplayClock = xTaskGetTickCount();


	while (1) {
		if (xTaskNotifyWait(0, 0xffffffff, &eventId, pdMS_TO_TICKS(DISPLAY_TASK_DELAY)) == pdTRUE) { // An event has been received

			// *** Special case in which you need to go straight to set time ***
			if (eventId == DISP_EV_FORCE_SET_TIME) {
				dispState = DISP_WAIT_SET_RTC;
				eventId = DISP_EV_BTN_SET;
				ssd1306_ClearScreen();
			}

			// *** BUTTONS EVENTS ***
			if ((eventId > 0) && (eventId < 200)) {	// Handle BUTTONS messages
				displayOnOff(ON, &lastDisplayOnTime);
				if (dispState != DISP_ERROR) {  // Handle the special case DISP_ERROR
					if ((dispState == DISP_CLOCK) && (eventId == DISP_EV_BTN_SET)) {
						dispState = DISP_WAIT_SET_RTC;

					} else if ((dispState == DISP_WAIT_SET_RTC) && (eventId == DISP_EV_BTN_SET)) {
						dispState = DISP_SET_RTC;
						i = 0;  //reset the counter
						displayUpdateTimeVar(showTime);

						// Write the title
						displayTitle(dispState, dispStringBuffer);

						// Write the first marker and time
						displayCursorSetTime(i, 96);
						displayShowClock(showTime);

					} else if (dispState == DISP_SET_RTC) {
						switch (eventId) {
						case DISP_EV_BTN_SET:
							// Delete the previous marker
							displayCursorSetTime(i, 32);
							i++;
							break;

						case DISP_EV_BTN_INC:
							if ((i == TEEN_HRS) && (showTime[i] < 2)) {
								showTime[i]++;
								if ((showTime[i] == 2) && (showTime[i + 1] > 3)) { // prevent to have numbers above 23
									showTime[i + 1] = 3;
								}
							}
							if (((i == UNIT_HRS) && (showTime[i] < 9) && (showTime[TEEN_HRS] < 2))
									|| ((i == 1) && (showTime[i] < 3) && (showTime[TEEN_HRS] == 2))) {
								showTime[i]++;
							}
							if ((i == TEEN_MINS) && (showTime[i] < 5)) {
								showTime[i]++;
							}
							if ((i == UNIT_MINS) && (showTime[i] < 9)) {
								showTime[i]++;
							}
							break;

						case DISP_EV_BTN_DEC:
							if (showTime[i] > 0) {
								showTime[i]--;
							}
							break;

						default:
							break;
						}
						if (i == 4) {
							// launch the Sync process
							RTC_Time.Hours = (showTime[TEEN_HRS] * 10) + showTime[UNIT_HRS];
							RTC_Time.Minutes = (showTime[TEEN_MINS] * 10) + showTime[UNIT_MINS];
							RTC_Time.Seconds = 00;
							RTC_Date.Date = 1;
							RTC_Date.Month = RTC_MONTH_JANUARY;
							RTC_Date.Year = 21;
							RTC_Date.WeekDay = RTC_WEEKDAY_FRIDAY;

							taskENTER_CRITICAL();
							HAL_RTC_SetTime(hrtcHandle, &RTC_Time, RTC_FORMAT_BIN); // set the new time
							HAL_RTC_SetDate(hrtcHandle, &RTC_Date, RTC_FORMAT_BIN);
							taskEXIT_CRITICAL();
							dispState = DISP_SYNC; // Prevent further actions
							xTaskNotify(clockTaskHandle, (uint32_t) CLOCK_EV_NEW_TIME, eSetValueWithOverwrite); // unblock SYNC Task
						} else {

							displayShowClock(showTime);
							displayCursorSetTime(i, 96);
						}
						// End of DISP_SET_RTC state
					} else {
						dispState = DISP_CLOCK;	// send the status to IDLE no further action
					}

				} // End of special case DISP_ERROR
				if (dispState != DISP_SYNC) {  // Prevent to get additional buttons
					vTaskResume(buttonTaskHandle); // Get another button event
				}

			} // End BUTTONS events

			// *** SYNC EVENTS ***
			if ((eventId > 200) && (eventId < 300)) {
				dispState = DISP_SYNC;

				displayOnOff(ON, &lastDisplayOnTime);
				lastDisplayOnTime = xTaskGetTickCount();  // Update display on time

				if (eventId == DISP_EV_SYN_START) {
					ssd1306_ClearScreen();
					displayTitle(dispState, dispStringBuffer);
				}

				displayMessage(eventId - DISP_EV_SYN_START, dispStringBuffer);

				if (eventId == DISP_EV_SYN_END) {  // Sync complete switch the display in clock mode
					dispState = DISP_CLOCK;  //Switch in Clock mode
					vTaskDelay(pdMS_TO_TICKS(1000)); // allow to show the message for 1 second
					ssd1306_ClearScreen();
					displayOnOff(ON, &lastDisplayOnTime);

					lastDisplayOnTime = xTaskGetTickCount();  // Reset display on time
					lastDisplayClock = xTaskGetTickCount() - pdMS_TO_TICKS(DISPLAY_CLOCK_INTERVAL) - 10; // Update display on time
					vTaskResume(buttonTaskHandle);  // Allow to read buttons to switch on the display
				}
			} // End SYNC events

			// *** ERROR EVENTS ***
			if ((eventId > 300) && (eventId < 400)) {  // Handle ERROR messages
				dispState = DISP_ERROR;
				displayTitle(dispState, dispStringBuffer);
				displayMessage(eventId - DISP_EV_ERR_START, dispStringBuffer);

				vTaskResume(buttonTaskHandle);  // Allow to read buttons to switch on the display
			}

		} else {  // Event wait timed out

			if ((dispState == DISP_CLOCK) || (dispState == DISP_WAIT_SET_RTC)) { // States in which you want to update the display clock
				if (timeLapsed(xTaskGetTickCount(), lastDisplayClock) > pdMS_TO_TICKS(DISPLAY_CLOCK_INTERVAL)) {
					displayTitle(DISP_CLOCK, dispStringBuffer);
					displayUpdateTimeVar(showTime);
					if (i < 4) { // check if the cursor has been drawn
						displayCursorSetTime(i, 32);
						i = 4;
					}
					displayShowClock(showTime);
					lastDisplayClock = xTaskGetTickCount();
				}
			}
			// Display auto shut off and IDLE state transition
			if ((dispState != DISP_SYNC)) {  // States in which you DO NOT shut off the display
				if (timeLapsed(xTaskGetTickCount(), lastDisplayOnTime) > pdMS_TO_TICKS(DISPLAY_OFF_TIMEOUT)) {
					displayOnOff(OFF, &lastDisplayOnTime);
					if (dispState != DISP_ERROR) {	// do not exit from ERR STATE
						dispState = DISP_CLOCK;	// send the status to IDLE no further action
					}
				}
			}
		}

	} // While loop end
}


/*
 * ################################
 * #         BUTTON TASK          #
 * ################################
 */

// #### Main Button Task ####
static void buttonTask(void *parameters) {

	tactButton_t tactButton[BTN_MAX];
	int i, j, alloff;
	TickType_t last_wakeup_time;

	//Variables initialization
	last_wakeup_time = xTaskGetTickCount();

	for (i = 0; i < BTN_MAX; i++) {
		tactButton[i].status = 1;  // Buttons when are pressed have a logic value 0
		tactButton[i].inDbnc = 0;
		tactButton[i].tikCnt = xTaskGetTickCount();
	}

	while (1) {
		// scan if any button has been pressed
		tactButton[BTN_SET].actual = HAL_GPIO_ReadPin(BTN_SET_GPIO_Port,
		BTN_SET_Pin);
		tactButton[BTN_INC].actual = HAL_GPIO_ReadPin(BTN_INC_GPIO_Port,
		BTN_INC_Pin);
		tactButton[BTN_DEC].actual = HAL_GPIO_ReadPin(BTN_DEC_GPIO_Port,
		BTN_DEC_Pin);

		for (i = 0; i < BTN_MAX; i++) {
			if ((tactButton[i].actual != tactButton[i].status) && (tactButton[i].inDbnc == 0)) { // There is a change of state in the button
				tactButton[i].tikCnt = xTaskGetTickCount();
				tactButton[i].inDbnc = 1;
			}

			if (timeLapsed(xTaskGetTickCount(), tactButton[i].tikCnt) > pdMS_TO_TICKS(BTN_DEBOUNCE)) { // Check if the button still pressed
				tactButton[i].inDbnc = 0;
				if (tactButton[i].actual != tactButton[i].status) { // The button is really pressed
					if (tactButton[i].actual == 0) { // check if the button is pressed (logic level 0)
						alloff = 0;
						for (j = 0; j < BTN_MAX; j++) { // Scan all buttons to verify if another button is already pressed
							alloff += tactButton[j].status;
						}
						if (alloff == BTN_MAX) { // No other button pressed (all logical 1)
							tactButton[i].status = tactButton[i].actual; // The state of the button has been validated and only one has been pressed
							xTaskNotify(displayTaskHandle, (uint32_t ) 101 + i, eSetValueWithOverwrite); // Send the eventId 101 to 103
							vTaskSuspend(NULL);  // Stop scanning buttons till they are processed
						}
					} else {
						tactButton[i].status = tactButton[i].actual; // The state of the button has been validated
					}
				}
			}
		}  // For loop end
		vTaskDelayUntil(&last_wakeup_time, pdMS_TO_TICKS(BTN_TASK_DELAY));

	} // While loop end
}


/*
 * ################################
 * #          SYNC TASK           #
 * ################################
 */

// Commands to the servo to go in parking position and shutdown
static void shutdownServo(void) {
	htimHandle->Instance->CCR4 = SERVO_PARKING_PWM;
	vTaskDelay(pdMS_TO_TICKS(SERVO_PARK_TIME));
	htimHandle->Instance->CCR4 = 0;
	HAL_TIM_PWM_Stop(htimHandle, TIM_CHANNEL_4); // Stop to generate PWM signal
}


// Commands to the servo to advance one hour
static void clockAdvHour(void) {
#ifndef CIFRA5_DEBUG
	htimHandle->Instance->CCR4 = SERVO_ENGAGE_PWM;
	vTaskDelay(pdMS_TO_TICKS(SERVO_ENGAGE_TIME));
	htimHandle->Instance->CCR4 = SERVO_RELEASE_PWM;
	vTaskDelay(pdMS_TO_TICKS(SERVO_ENGAGE_TIME));
#endif
}


// Advance the clock by 1 minute. Slow = 1 add extra delay for the coil
static void clockAdvMinute(uint8_t *tickType, const uint8_t slow) {
	uint32_t coilExcite = COIL_EXCITE_TIME + (COIL_EXTRA_TIME * slow);
	uint32_t coilRest = COIL_REST_TIME + (COIL_EXTRA_TIME * slow);

	if (*tickType == 0) {
#ifndef CIFRA5_DEBUG
			HAL_GPIO_WritePin(CLK_TICK_GPIO_Port, CLK_TICK_Pin, GPIO_PIN_RESET);
			vTaskDelay(pdMS_TO_TICKS(coilExcite));
			HAL_GPIO_WritePin(CLK_TICK_GPIO_Port, CLK_TICK_Pin, GPIO_PIN_SET);
			vTaskDelay(pdMS_TO_TICKS(coilRest));
#endif
		*tickType = 1;
	} else {
#ifndef CIFRA5_DEBUG
			HAL_GPIO_WritePin(CLK_TOCK_GPIO_Port, CLK_TOCK_Pin, GPIO_PIN_RESET);
			vTaskDelay(pdMS_TO_TICKS(coilExcite));
			HAL_GPIO_WritePin(CLK_TOCK_GPIO_Port, CLK_TOCK_Pin, GPIO_PIN_SET);
			vTaskDelay(pdMS_TO_TICKS(coilRest));
#endif
		*tickType = 0;
	}
}



// #### Main Sync task ####
static void syncTask(void *parameters) {
	uint8_t skipSync = *(uint8_t*) parameters; // When = 0 the clock has NOT been intialized
	uint32_t message;
	uint8_t lastTick;  // When 1 the last impulse was Tick
	uint8_t syncCount = 0;
	uint8_t prevSens;
	uint8_t i;

	while (1) {
		lastTick = 0;
		if (skipSync > 0) {
			vTaskDelay(pdMS_TO_TICKS(200)); // Wait to complete any display action
			xTaskNotify(displayTaskHandle, (uint32_t ) DISP_EV_SYN_START, eSetValueWithOverwrite);
			vTaskDelay(pdMS_TO_TICKS(1000)); // Wait to show the message on display
			if (syncCount == 3) {  // Too many syncronizations
				xTaskNotify(displayTaskHandle, (uint32_t ) DISP_EV_ERR_MANY_SYNC, eSetValueWithOverwrite);
				vTaskSuspend(NULL);  // Suspend the task end enter in locked mode
			}

			// Set the coil clock driver in brake mode
			HAL_GPIO_WritePin(CLK_TICK_GPIO_Port, CLK_TICK_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(CLK_TOCK_GPIO_Port, CLK_TOCK_Pin, GPIO_PIN_SET);

			// Start the PWM signal (duty cycle = 0)
			htimHandle->Instance->CCR4 = 0;
			HAL_TIM_PWM_Start(htimHandle, TIM_CHANNEL_4);
			vTaskDelay(pdMS_TO_TICKS(200));

			// The servo need to be placed in parked position
			htimHandle->Instance->CCR4 = SERVO_PARKING_PWM;
			vTaskDelay(pdMS_TO_TICKS(SERVO_PARK_TIME));
			htimHandle->Instance->CCR4 = 0;

			//
			// Search for 00 Minutes
			//
			xTaskNotify(displayTaskHandle, (uint32_t ) DISP_EV_SYN_SRC_HOUR, eSetValueWithOverwrite);
			i = 0;
			do {
				if (i == 62) {  // Error no Hour sensor
					shutdownServo();
					xTaskNotify(displayTaskHandle, (uint32_t ) DISP_EV_ERR_SNS_HOUR, eSetValueWithOverwrite);
					vTaskSuspend(NULL);  // The error is an Hard fault non recoverable by software
				}
				prevSens = HAL_GPIO_ReadPin(SNS_HOUR_GPIO_Port, SNS_HOUR_Pin);
				clockAdvMinute(&lastTick, 0);
				i++;
			} while (!((prevSens == 0) && (HAL_GPIO_ReadPin(SNS_HOUR_GPIO_Port, SNS_HOUR_Pin) == 1)));

			//
			// Search for 00 Hours
			//
			xTaskNotify(displayTaskHandle, (uint32_t ) DISP_EV_SYN_SRC_DAY, eSetValueWithOverwrite);
			htimHandle->Instance->CCR4 = SERVO_RELEASE_PWM; // Position the lever in release position
			vTaskDelay(pdMS_TO_TICKS(SERVO_PARK_TIME));
			i = 0;
			do {
				if (i == 25) {  // Error no Day sensor
					shutdownServo();
					xTaskNotify(displayTaskHandle, (uint32_t ) DISP_EV_ERR_SNS_DAY, eSetValueWithOverwrite);
					vTaskSuspend(NULL);  // The error is an Hard fault non recoverable by software
				}
				prevSens = HAL_GPIO_ReadPin(SNS_DAY_GPIO_Port, SNS_DAY_Pin);
				clockAdvHour();
				i++;
			} while (!((prevSens == 1) && (HAL_GPIO_ReadPin(SNS_DAY_GPIO_Port, SNS_DAY_Pin) == 0)));
			vTaskDelay(pdMS_TO_TICKS(500)); // Wait some additional time to allow the digits to set

			//
			// SET the Hours
			//
			xTaskNotify(displayTaskHandle, (uint32_t ) DISP_EV_SYN_SET_HOUR, eSetValueWithOverwrite);
			HAL_RTC_GetTime(hrtcHandle, &RTC_Time, RTC_FORMAT_BIN); // Get the current time
			HAL_RTC_GetDate(hrtcHandle, &RTC_Date, RTC_FORMAT_BIN); // Unlock the shadow registers reading also the date
			for (i = 0; i < RTC_Time.Hours; i++) {
				clockAdvHour();
			}
			shutdownServo();

			//
			// SET the Minutes
			//
			xTaskNotify(displayTaskHandle, (uint32_t ) DISP_EV_SYN_SET_MIN, eSetValueWithOverwrite);
			for (i = 0; i < RTC_Time.Minutes; i++) {
				clockAdvMinute(&lastTick, 0);
			}

			xTaskNotify(displayTaskHandle, (uint32_t ) DISP_EV_SYN_END, eSetValueWithOverwrite);
			syncCount++;  // Increment the syncro count
			message = lastTick;
			xTaskNotify(clockTaskHandle, message, eSetValueWithOverwrite); // unblock clock Task
		} else {
			skipSync = 1; // Force the next sync, the RTC is initialized do not skip anymore
			xTaskNotify(displayTaskHandle, (uint32_t ) DISP_EV_FORCE_SET_TIME, eSetValueWithOverwrite); // Go directly to Set time
		} // end of Skip Sync

		xTaskNotifyWait(0xffffffff, 0, &message, portMAX_DELAY); // Wait for a message from the clock Task
		if (message == CLOCK_EV_NEW_TIME) {
			syncCount = 0;  // reset the sync counter we have a brand new time
		}
	} // While loop end
}


/*
 * ################################
 * #          CLOCK TASK          #
 * ################################
 */

// #### Main Clock Task ####
static void clockTask(void *parameters) {
	uint8_t lastTick = 0;
	uint32_t message;
	uint8_t prevSens;
	uint8_t clockMinutes = 0;
	TickType_t waitTime = portMAX_DELAY;

	while (1) {
		if (xTaskNotifyWait(0, 0xffffffff, &message, waitTime) == pdTRUE) { // Wait for a message from the clock Task
			if (message < 9) {  // Message coming from syncTask
				clockMinutes = RTC_Time.Minutes; // get the time of the clock from sync
				lastTick = message;
				waitTime = pdMS_TO_TICKS(CLOCK_UPDATE_INTERVAL);
			} else {
				waitTime = portMAX_DELAY;
				vTaskDelay(pdMS_TO_TICKS(500));
				xTaskNotify(syncTaskHandle, message, eSetValueWithOverwrite); // Notify the SYNC Task and block the CLOCK Task
				// the message contain the same message for both task Clock and Sync
			}
		} else {
			HAL_RTC_GetTime(hrtcHandle, &RTC_Time, RTC_FORMAT_BIN); // Get the current time
			HAL_RTC_GetDate(hrtcHandle, &RTC_Date, RTC_FORMAT_BIN); // Unlock the shadow registers reading also the date
			if (RTC_Time.Minutes != clockMinutes) { // Update the clock
				prevSens = HAL_GPIO_ReadPin(SNS_HOUR_GPIO_Port, SNS_HOUR_Pin); // get the status of the passing hour sensor
				clockAdvMinute(&lastTick, 1);
				clockMinutes = RTC_Time.Minutes;

				if ((prevSens == 0) && (HAL_GPIO_ReadPin(SNS_HOUR_GPIO_Port, SNS_HOUR_Pin) == 1)
						&& (clockMinutes != 0)) { // check if there is a hour transition and minutes are not 00
					waitTime = portMAX_DELAY;
					xTaskNotify(syncTaskHandle, (uint32_t ) CLOCK_EV_RESYNC, eSetValueWithOverwrite); // Notify the SYNC Task and block the CLOCK Task
					vTaskSuspend(buttonTaskHandle); // During sync do not check the buttons
				}
			}
		}  // Event wait timed out

	} // While loop end
}

/*
 * ################################
 * #          OTHER RTOS          #
 * ################################
 */

// #### Initialize peripherals handlers for the RTOS tasks ####
void initRTOS_Periferals(TIM_HandleTypeDef *htim, RTC_HandleTypeDef *hrtc) {
	htimHandle = htim;
	hrtcHandle = hrtc;
}


// #### Task Implementation and start scheduler ####
void createRTOS_Tasks() {

	// if 0 the clock has never been set up skip the initial sync
	uint8_t syncTaskInitState = (uint8_t) (hrtcHandle->Instance->ICSR & RTC_ICSR_INITS);

	// FreeRTOS - Tasks Creation/
	configASSERT(xTaskCreate(displayTask, "Display Task", 120, NULL, 2, &displayTaskHandle) == pdPASS);
	configASSERT(xTaskCreate(buttonTask, "Button Task", 80, NULL, 2, &buttonTaskHandle) == pdPASS);
	configASSERT(xTaskCreate(clockTask, "Clock Task", 80, NULL, 2, &clockTaskHandle) == pdPASS);
	configASSERT(xTaskCreate(syncTask, "Sync Task", 80, (void *) &syncTaskInitState, 2, &syncTaskHandle) == pdPASS);

	vTaskSuspend(buttonTaskHandle); // During StartUp do not check the buttons
}


// #### Stack Overflow Hook Implementation ####
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
	Error_Handler();
}

