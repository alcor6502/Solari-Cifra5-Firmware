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
static TIM_HandleTypeDef *htimHandle;
static RTC_HandleTypeDef *hrtcHandle;

// Struct to set and read the real time clock
static RTC_TimeTypeDef RTC_Time;
static RTC_DateTypeDef RTC_Date;

// Task Handles
static TaskHandle_t displayTaskHandle;
static TaskHandle_t buttonTaskHandle;
static TaskHandle_t clockTaskHandle;

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
 * #   RTC BACKUP REGISTER UTILS  #
 * ################################
 */

// Get mechanical hours from backup register
static uint8_t getMechHours(void) {
	uint8_t hours = (uint8_t)(HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_MECH_HOURS) & 0xFF);
	return (hours > 23) ? 0 : hours;
}

// Get mechanical minutes from backup register
static uint8_t getMechMinutes(void) {
	uint8_t minutes = (uint8_t)(HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_MECH_MINUTES) & 0xFF);
	return (minutes > 59) ? 0 : minutes;
}

// Set mechanical position in backup registers
static void setMechPosition(uint8_t hours, uint8_t minutes) {
	HAL_RTCEx_BKUPWrite(hrtcHandle, RTC_BKP_MECH_HOURS, (uint32_t)hours);
	HAL_RTCEx_BKUPWrite(hrtcHandle, RTC_BKP_MECH_MINUTES, (uint32_t)minutes);
}

// Increment mechanical minute (handles hour rollover)
static void incrementMechMinute(void) {
	uint8_t hours = getMechHours();
	uint8_t minutes = getMechMinutes();

	minutes++;
	if (minutes >= 60) {
		minutes = 0;
		hours++;
		if (hours >= 24) {
			hours = 0;
		}
	}
	setMechPosition(hours, minutes);
}

// Increment mechanical hour (minutes reset to 0)
static void incrementMechHour(void) {
	uint8_t hours = getMechHours();
	hours++;
	if (hours >= 24) {
		hours = 0;
	}
	setMechPosition(hours, 0);
}

// Reset mechanical position to 00:00
static void resetMechPosition(void) {
	setMechPosition(0, 0);
}

// Get last tick state from backup register
static uint8_t getLastTick(void) {
	uint32_t flags = HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_FLAGS);
	return (flags & RTC_BKP_FLAG_LAST_TICK) ? 1 : 0;
}

// Set last tick state in backup register
static void setLastTick(uint8_t tickState) {
	uint32_t flags = HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_FLAGS);

	if (tickState) {
		flags |= RTC_BKP_FLAG_LAST_TICK;
	} else {
		flags &= ~RTC_BKP_FLAG_LAST_TICK;
	}

	HAL_RTCEx_BKUPWrite(hrtcHandle, RTC_BKP_FLAGS, flags);
}


/*
 * ################################
 * #     SILENT MODE HELPERS      #
 * ################################
 */

// Check if current hour is within silent period
static uint8_t isInSilentPeriod(void) {
    HAL_RTC_GetTime(hrtcHandle, &RTC_Time, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(hrtcHandle, &RTC_Date, RTC_FORMAT_BIN);
	if (SILENT_START_HOUR > SILENT_END_HOUR) {
		// Wraps around midnight (e.g., 22:00 to 09:00)
		return (RTC_Time.Hours >= SILENT_START_HOUR || RTC_Time.Hours < SILENT_END_HOUR);
	} else {
		// Same day (e.g., 02:00 to 05:00)
		return (RTC_Time.Hours >= SILENT_START_HOUR && RTC_Time.Hours < SILENT_END_HOUR);
	}
}


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
 * #          CLOCK TASK          #
 * ################################
 */

 // Initialize and position servo for synchronization
static void prepareServo(void) {
        htimHandle->Instance->CCR4 = 0;
        HAL_TIM_PWM_Start(htimHandle, TIM_CHANNEL_4);
        vTaskDelay(pdMS_TO_TICKS(200));
        htimHandle->Instance->CCR4 = SERVO_RELEASE_PWM;
        vTaskDelay(pdMS_TO_TICKS(SERVO_PARK_TIME));
}


// Commands to the servo to advance one hour
static void clockAdvHour(void) {

#ifndef CIFRA5_DEBUG
	htimHandle->Instance->CCR4 = SERVO_ENGAGE_PWM;
	vTaskDelay(pdMS_TO_TICKS(SERVO_ENGAGE_TIME));
	htimHandle->Instance->CCR4 = SERVO_RELEASE_PWM;
	vTaskDelay(pdMS_TO_TICKS(SERVO_ENGAGE_TIME));
#endif

	// Update mechanical hours in backup registers
	incrementMechHour();
}

// Commands to the servo to go in parking position and shutdown
static void shutdownServo(void) {
	htimHandle->Instance->CCR4 = SERVO_PARKING_PWM;
	vTaskDelay(pdMS_TO_TICKS(SERVO_PARK_TIME));
	htimHandle->Instance->CCR4 = 0;
	HAL_TIM_PWM_Stop(htimHandle, TIM_CHANNEL_4); // Stop to generate PWM signal
    vTaskDelay(pdMS_TO_TICKS(500));
}

// Advance the clock by 1 minute. Slow = 1 add extra delay for the coil
static void clockAdvMinute(const uint8_t slow) {
	uint32_t coilExcite = COIL_EXCITE_TIME + (COIL_EXTRA_TIME * slow);
	uint32_t coilRest = COIL_REST_TIME + (COIL_EXTRA_TIME * slow);
	uint8_t tickType = getLastTick();

	if (tickType == 0) {
#ifndef CIFRA5_DEBUG
			HAL_GPIO_WritePin(CLK_TICK_GPIO_Port, CLK_TICK_Pin, GPIO_PIN_RESET);
			vTaskDelay(pdMS_TO_TICKS(coilExcite));
			HAL_GPIO_WritePin(CLK_TICK_GPIO_Port, CLK_TICK_Pin, GPIO_PIN_SET);
			vTaskDelay(pdMS_TO_TICKS(coilRest));
#endif
		tickType = 1;
	} else {
#ifndef CIFRA5_DEBUG
			HAL_GPIO_WritePin(CLK_TOCK_GPIO_Port, CLK_TOCK_Pin, GPIO_PIN_RESET);
			vTaskDelay(pdMS_TO_TICKS(coilExcite));
			HAL_GPIO_WritePin(CLK_TOCK_GPIO_Port, CLK_TOCK_Pin, GPIO_PIN_SET);
			vTaskDelay(pdMS_TO_TICKS(coilRest));
#endif
		tickType = 0;
	}

	// Update mechanical position and tick state in backup registers
	incrementMechMinute();
	setLastTick(tickType);
}



// Search for 00:00 position using sensors (first time sync)
static void searchForZeroPosition(void) {
	uint8_t i;
	uint8_t prevSens;

	// Search for 00 minutes using hour sensor
	xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_SYN_SRC_HOUR, eSetValueWithOverwrite);
	i = 0;
	do {
		if (i == 62) {  // Error: sensor not found
			xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_ERR_SNS_HOUR, eSetValueWithOverwrite);
			vTaskSuspend(NULL);
		}
		prevSens = HAL_GPIO_ReadPin(SNS_HOUR_GPIO_Port, SNS_HOUR_Pin);
		clockAdvMinute(0);
		i++;
	} while (!((prevSens == 0) && (HAL_GPIO_ReadPin(SNS_HOUR_GPIO_Port, SNS_HOUR_Pin) == 1)));

	// Now at XX:00, search for 00 hours using day sensor
	xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_SYN_SRC_DAY, eSetValueWithOverwrite);

	// Start PWM and park servo
    prepareServo();
	i = 0;
	do {
		if (i == 25) {  // Error: sensor not found
			shutdownServo();
			xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_ERR_SNS_DAY, eSetValueWithOverwrite);
			vTaskSuspend(NULL);
		}
		prevSens = HAL_GPIO_ReadPin(SNS_DAY_GPIO_Port, SNS_DAY_Pin);
		clockAdvHour();
		i++;
	} while (!((prevSens == 1) && (HAL_GPIO_ReadPin(SNS_DAY_GPIO_Port, SNS_DAY_Pin) == 0)));

    // Reset mechanical position to 00:00
    resetMechPosition();
}


// Synchronize hours using servo
static void syncHours(uint8_t targetHours) {
	xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_SYN_SET_HOUR, eSetValueWithOverwrite);
   
    if (htimHandle->Instance->CCR4 != SERVO_RELEASE_PWM) {    // prevent double activation
        prepareServo();
    }
    
	// Advance hours to target (wrap around if needed)
	while (getMechHours() != targetHours) {
		clockAdvHour();
	}
	shutdownServo();
}


// Synchronize minutes using coils
static void syncMinutes(uint8_t targetMinutes) {
	xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_SYN_SET_MIN, eSetValueWithOverwrite);

	for (uint8_t i = 0; i < targetMinutes; i++) {
		clockAdvMinute(0);
	}
}


// Advance to hour boundary (minutes to 00)
static void advanceToHourBoundary(void) {
	uint8_t minutes = getMechMinutes();
	if (minutes != 0) {
		xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_SYN_SRC_HOUR, eSetValueWithOverwrite);
		for (uint8_t i = minutes; i < 60; i++) {
			clockAdvMinute(0);
		}
	}
}


// #### Main Clock Task ####
static void clockTask(void *parameters) {
	uint8_t rtcInitOk = (uint8_t)(uintptr_t) parameters; // When = 0 the clock has NOT been initialized
	uint32_t message;
	uint8_t prevSens;
	uint8_t syncCount = 0;
	uint8_t inSilentMode = 0;

	while (1) {

		// ===== PHASE 1: PRE-SYNC =====

		// First boot: RTC not initialized, let user set time first
		if (!rtcInitOk) {
			rtcInitOk = 1;
			xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_FORCE_SET_TIME, eSetValueWithOverwrite);
			xTaskNotifyWait(0xffffffff, 0xffffffff, &message, portMAX_DELAY);
			// User finished setting time, proceed to sync
		}

		// Wait out silent period (power outage recovery)
		while (isInSilentPeriod()) {
			vTaskDelay(pdMS_TO_TICKS(60000));
		}

		// ===== PHASE 2: SYNC =====

		vTaskSuspend(buttonTaskHandle); // Disable buttons during sync (no-op if already suspended)
		vTaskDelay(pdMS_TO_TICKS(200)); // Wait to complete any display action

        if (syncCount == 3) {  // Too many resynchronizations
			xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_ERR_MANY_SYNC, eSetValueWithOverwrite);
		}

		xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_SYN_START, eSetValueWithOverwrite);
		vTaskDelay(pdMS_TO_TICKS(1000)); // Wait to show the message on display

		// Check if mechanical position is at 0:0 (first time sync needs sensor search)
		if (getMechHours() == 0 && getMechMinutes() == 0) {
			// FIRST TIME SYNC: Use sensor-based search to find 00:00
			searchForZeroPosition();
		} else {
			// FAST SYNC: Position known from backup registers
			advanceToHourBoundary();
		}

		HAL_RTC_GetTime(hrtcHandle, &RTC_Time, RTC_FORMAT_BIN); // Get updated time
		HAL_RTC_GetDate(hrtcHandle, &RTC_Date, RTC_FORMAT_BIN);
        
		syncHours(RTC_Time.Hours);
		syncMinutes(RTC_Time.Minutes);

		xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_SYN_END, eSetValueWithOverwrite);

		// ===== PHASE 3: NORMAL OPERATION =====

		inSilentMode = 0;

		while (1) {
			// Check for new time set by user
			if (xTaskNotifyWait(0, 0xffffffff, &message, pdMS_TO_TICKS(CLOCK_UPDATE_INTERVAL)) == pdTRUE) {
				if (message == CLOCK_EV_NEW_TIME) {
					break; // User set new time -> resync
				}
				continue; // Ignore unexpected notifications
			}

			// Silent period entry
			if (isInSilentPeriod()) {
				inSilentMode = 1;
				continue;
			}

			// Silent period exit -> resync
			if (inSilentMode) {
				inSilentMode = 0;
				break;
			}

			// Check if minute advance needed
			if (RTC_Time.Minutes == getMechMinutes()) {
				continue;
			}

			prevSens = HAL_GPIO_ReadPin(SNS_HOUR_GPIO_Port, SNS_HOUR_Pin);
			clockAdvMinute(1);

			// Unexpected hour transition: mechanical drift detected, force full resync
			if ((prevSens == 0)
					&& (HAL_GPIO_ReadPin(SNS_HOUR_GPIO_Port, SNS_HOUR_Pin) == 1)
					&& (getMechMinutes() != 0)) {
				resetMechPosition();
				vTaskSuspend(buttonTaskHandle);
				syncCount++;
				break;
			}
		}

	} // Outer while loop end
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
	uint8_t clockTaskInitState = (uint8_t) (hrtcHandle->Instance->ICSR & RTC_ICSR_INITS);

    resetMechPosition();    // Force in startup to search for 0 in order to sincronize the Tick Tock

	// FreeRTOS - Tasks Creation/
	configASSERT(xTaskCreate(displayTask, "Display Task", 120, NULL, 2, &displayTaskHandle) == pdPASS);
	configASSERT(xTaskCreate(buttonTask, "Button Task", 80, NULL, 2, &buttonTaskHandle) == pdPASS);
	configASSERT(xTaskCreate(clockTask, "Clock Task", 120, (void *)(uintptr_t) clockTaskInitState, 2, &clockTaskHandle) == pdPASS);

	vTaskSuspend(buttonTaskHandle); // During StartUp do not check the buttons
}


// #### Stack Overflow Hook Implementation ####
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
	Error_Handler();
}

