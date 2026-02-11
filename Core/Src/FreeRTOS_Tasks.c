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

// Titles array (indexed by dispStateEnum)
const char dispTitle[6][11] = {
		{DISP_TITLE01},
		{DISP_TITLE02},
		{DISP_TITLE03},
		{DISP_TITLE04},
		{DISP_TITLE05},
		{DISP_TITLE06},
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

// Get silent start hour from backup register (defaults to SILENT_DEFAULT_START)
static uint8_t getSilentStartHour(void) {
	uint32_t raw = HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_SILENT);
	uint8_t hour = (uint8_t)(raw & 0xFF);
	return (hour > 23) ? SILENT_DEFAULT_START : hour;
}

// Get silent end hour from backup register (defaults to SILENT_DEFAULT_END)
static uint8_t getSilentEndHour(void) {
	uint32_t raw = HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_SILENT);
	uint8_t hour = (uint8_t)((raw >> 8) & 0xFF);
	return (hour > 23) ? SILENT_DEFAULT_END : hour;
}

// Set silent period hours in backup register (start in bits 7:0, end in bits 15:8)
static void setSilentHours(uint8_t startHour, uint8_t endHour) {
	uint32_t raw = (uint32_t)startHour | ((uint32_t)endHour << 8);
	HAL_RTCEx_BKUPWrite(hrtcHandle, RTC_BKP_SILENT, raw);
}

// Get calibration from backup register (bit 9=plus, bits 0-8=value)
static void getCalibration(uint8_t *plusPulses, uint16_t *calibValue) {
	uint32_t raw = HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_CALIB);
	if (raw > 0x3FF) {
		*plusPulses = 0;
		*calibValue = 0;
		return;
	}
	*plusPulses = (raw & 0x200) ? 1 : 0;
	*calibValue = raw & 0x1FF;
}

// Set calibration in backup register
static void setCalibration(uint8_t plusPulses, uint16_t calibValue) {
	uint32_t raw = (calibValue & 0x1FF) | (plusPulses ? 0x200 : 0);
	HAL_RTCEx_BKUPWrite(hrtcHandle, RTC_BKP_CALIB, raw);
}

// Apply calibration from backup register to RTC hardware
static void applyCalibration(void) {
	uint8_t plusPulses;
	uint16_t calibValue;
	getCalibration(&plusPulses, &calibValue);
	HAL_RTCEx_SetSmoothCalib(hrtcHandle,
		RTC_SMOOTHCALIB_PERIOD_32SEC,
		plusPulses ? RTC_SMOOTHCALIB_PLUSPULSES_SET : RTC_SMOOTHCALIB_PLUSPULSES_RESET,
		calibValue);
}


/*
 * ################################
 * #    FLASH SETTINGS STORAGE    #
 * ################################
 */

// Write silent hours and calibration to Flash (last page)
// Data packed into one 64-bit double-word:
//   word0: silentStart[7:0] | silentEnd[15:8] | calibRaw[31:16]
//   word1: magic number (0xC1F5A001)
static void flashWriteSettings(void) {
	uint8_t silentStart = getSilentStartHour();
	uint8_t silentEnd = getSilentEndHour();
	uint8_t calibPlus;
	uint16_t calibValue;
	getCalibration(&calibPlus, &calibValue);

	uint32_t calibRaw = (calibValue & 0x1FF) | (calibPlus ? 0x200 : 0);
	uint32_t word0 = (uint32_t)silentStart
					| ((uint32_t)silentEnd << 8)
					| (calibRaw << 16);
	uint64_t data = (uint64_t)word0 | ((uint64_t)FLASH_SETTINGS_MAGIC << 32);

	FLASH_EraseInitTypeDef eraseInit;
	uint32_t pageError;

	HAL_FLASH_Unlock();

	eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
	eraseInit.Page = FLASH_SETTINGS_PAGE;
	eraseInit.NbPages = 1;
	HAL_FLASHEx_Erase(&eraseInit, &pageError);

	HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_SETTINGS_ADDR, data);

	HAL_FLASH_Lock();
}

// Restore settings from Flash to backup registers (called on battery-loss boot)
static void flashRestoreSettings(void) {
	uint32_t *addr = (uint32_t *)FLASH_SETTINGS_ADDR;
	uint32_t word0 = addr[0];
	uint32_t word1 = addr[1];

	if (word1 != FLASH_SETTINGS_MAGIC) return;

	uint8_t silentStart = (uint8_t)(word0 & 0xFF);
	uint8_t silentEnd = (uint8_t)((word0 >> 8) & 0xFF);
	uint16_t calibRaw = (uint16_t)((word0 >> 16) & 0x3FF);

	if (silentStart <= 23 && silentEnd <= 23) {
		setSilentHours(silentStart, silentEnd);
	}

	uint8_t calibPlus = (calibRaw & 0x200) ? 1 : 0;
	uint16_t calibValue = calibRaw & 0x1FF;
	setCalibration(calibPlus, calibValue);
}


/*
 * ################################
 * #     SILENT MODE HELPERS      #
 * ################################
 */

// Check if current time is within silent period (starts at HH:01, not HH:00)
static uint8_t isInSilentPeriod(void) {
    HAL_RTC_GetTime(hrtcHandle, &RTC_Time, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(hrtcHandle, &RTC_Date, RTC_FORMAT_BIN);
	uint8_t start = getSilentStartHour();
	uint8_t end = getSilentEndHour();
	uint8_t pastStart = (RTC_Time.Hours > start)
			|| (RTC_Time.Hours == start && RTC_Time.Minutes > 0);
	if (start > end) {
		// Wraps around midnight (e.g., 22:01 to 09:00)
		return (pastStart || RTC_Time.Hours < end);
	} else {
		// Same day (e.g., 02:01 to 05:00)
		return (pastStart && RTC_Time.Hours < end);
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


// Display silent hours with dash separator (HH-HH)
static void displayShowSilentHours(uint8_t *time) {
	ssd1306_SetCursor(DIGIT_TEEN_HRS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[0] + 48, DISP_FONT_L);
	ssd1306_SetCursor(DIGIT_UNIT_HRS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[1] + 48, DISP_FONT_L);

	ssd1306_SetCursor(DIGIT_COLON_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(45, DISP_FONT_L);  // '-' dash

	ssd1306_SetCursor(DIGIT_TEEN_MINS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[2] + 48, DISP_FONT_L);
	ssd1306_SetCursor(DIGIT_UNIT_MINS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[3] + 48, DISP_FONT_L);
}


// Display calibration value with sign (±NNN)
static void displayShowCalibration(uint8_t *time) {
	ssd1306_SetCursor(DIGIT_TEEN_HRS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[0] ? 43 : 45, DISP_FONT_L);  // '+' or '-'

	ssd1306_SetCursor(DIGIT_UNIT_HRS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[1] + 48, DISP_FONT_L);

	ssd1306_SetCursor(DIGIT_COLON_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(32, DISP_FONT_L);  // blank space

	ssd1306_SetCursor(DIGIT_TEEN_MINS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[2] + 48, DISP_FONT_L);
	ssd1306_SetCursor(DIGIT_UNIT_MINS_X, DIGIT_TIME_Y);
	ssd1306_WriteChar(time[3] + 48, DISP_FONT_L);
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
static void displayOnOff(uint8_t newState, displayCtx_t *ctx) {
	if (newState != ctx->isOn) {
		ssd1306_SetDisplayOnOff(newState);
		ctx->isOn = newState;
	}
	if (newState == ON) {
		ctx->lastOnTime = xTaskGetTickCount();
	}
}


// Enter time setting mode
static void enterSetRtc(displayCtx_t *ctx, char *buf) {
	ctx->state = DISP_SET_RTC;
	ctx->setupMode = 0;
	ctx->digitCursor = 0;
	ssd1306_ClearScreen();
	displayUpdateTimeVar(ctx->showTime);
	displayTitle(ctx->state, buf);
	displayCursorSetTime(ctx->digitCursor, 96);
	displayShowClock(ctx->showTime);
}


// Enter silent hours setting mode
static void enterSetSilent(displayCtx_t *ctx, char *buf) {
	ctx->state = DISP_SET_SILENT;
	ctx->digitCursor = 0;
	ctx->showTime[0] = getSilentStartHour() / 10;
	ctx->showTime[1] = getSilentStartHour() % 10;
	ctx->showTime[2] = getSilentEndHour() / 10;
	ctx->showTime[3] = getSilentEndHour() % 10;
	ssd1306_ClearScreen();
	displayTitle(ctx->state, buf);
	displayShowSilentHours(ctx->showTime);
	displayCursorSetTime(ctx->digitCursor, 96);
}


// Enter calibration setting mode
static void enterSetCorrection(displayCtx_t *ctx, char *buf) {
	uint8_t plus;
	uint16_t val;
	getCalibration(&plus, &val);
	ctx->state = DISP_SET_CORRECTION;
	ctx->digitCursor = 0;
	ctx->showTime[0] = plus;
	ctx->showTime[1] = val / 100;
	ctx->showTime[2] = (val / 10) % 10;
	ctx->showTime[3] = val % 10;
	ssd1306_ClearScreen();
	displayTitle(ctx->state, buf);
	displayShowCalibration(ctx->showTime);
	displayCursorSetTime(ctx->digitCursor, 96);
}


// Handle button events in DISP_CLOCK state
static void handleClockBtns(uint32_t eventId, displayCtx_t *ctx, char *buf) {
	switch (eventId) {
	case DISP_EV_BTN_SET_LONG:
		if (!isInSilentPeriod()) {
			enterSetRtc(ctx, buf);
		}
		break;
	case DISP_EV_BTN_INC_LONG:
		enterSetSilent(ctx, buf);
		break;
	case DISP_EV_BTN_DEC_LONG:
		enterSetCorrection(ctx, buf);
		break;
	default:
		break;
	}
}


// Handle button events in DISP_SET_RTC state (digit editing)
static void handleSetRtcBtns(uint32_t eventId, displayCtx_t *ctx) {
	switch (eventId) {
	case DISP_EV_BTN_SET:
		displayCursorSetTime(ctx->digitCursor, 32);
		ctx->digitCursor++;
		break;

	case DISP_EV_BTN_INC:
		if ((ctx->digitCursor == TEEN_HRS) && (ctx->showTime[ctx->digitCursor] < 2)) {
			ctx->showTime[ctx->digitCursor]++;
			if ((ctx->showTime[ctx->digitCursor] == 2) && (ctx->showTime[ctx->digitCursor + 1] > 3)) {
				ctx->showTime[ctx->digitCursor + 1] = 3;
			}
		}
		if (((ctx->digitCursor == UNIT_HRS) && (ctx->showTime[ctx->digitCursor] < 9) && (ctx->showTime[TEEN_HRS] < 2))
				|| ((ctx->digitCursor == UNIT_HRS) && (ctx->showTime[ctx->digitCursor] < 3) && (ctx->showTime[TEEN_HRS] == 2))) {
			ctx->showTime[ctx->digitCursor]++;
		}
		if ((ctx->digitCursor == TEEN_MINS) && (ctx->showTime[ctx->digitCursor] < 5)) {
			ctx->showTime[ctx->digitCursor]++;
		}
		if ((ctx->digitCursor == UNIT_MINS) && (ctx->showTime[ctx->digitCursor] < 9)) {
			ctx->showTime[ctx->digitCursor]++;
		}
		break;

	case DISP_EV_BTN_DEC:
		if (ctx->showTime[ctx->digitCursor] > 0) {
			ctx->showTime[ctx->digitCursor]--;
		}
		break;

	default:
		break;
	}

	if (ctx->digitCursor == 4) {
		// All digits set — update RTC and start sync
		RTC_Time.Hours = (ctx->showTime[TEEN_HRS] * 10) + ctx->showTime[UNIT_HRS];
		RTC_Time.Minutes = (ctx->showTime[TEEN_MINS] * 10) + ctx->showTime[UNIT_MINS];
		RTC_Time.Seconds = 0;
		RTC_Date.Date = 1;
		RTC_Date.Month = RTC_MONTH_JANUARY;
		RTC_Date.Year = 21;
		RTC_Date.WeekDay = RTC_WEEKDAY_FRIDAY;

		taskENTER_CRITICAL();
		HAL_RTC_SetTime(hrtcHandle, &RTC_Time, RTC_FORMAT_BIN);
		HAL_RTC_SetDate(hrtcHandle, &RTC_Date, RTC_FORMAT_BIN);
		taskEXIT_CRITICAL();

		ctx->state = DISP_SYNC;
		xTaskNotify(clockTaskHandle, (uint32_t) CLOCK_EV_NEW_TIME, eSetValueWithOverwrite);
	} else {
		displayShowClock(ctx->showTime);
		displayCursorSetTime(ctx->digitCursor, 96);
	}
}


// Handle button events in DISP_SET_SILENT state (HH-HH editing)
static void handleSetSilentBtns(uint32_t eventId, displayCtx_t *ctx, char *buf) {
	switch (eventId) {
	case DISP_EV_BTN_SET:
		displayCursorSetTime(ctx->digitCursor, 32);
		ctx->digitCursor++;
		break;

	case DISP_EV_BTN_INC: {
		uint8_t tensIdx = (ctx->digitCursor < 2) ? 0 : 2;
		if (ctx->digitCursor == tensIdx) {
			// Tens digit: 0-2
			if (ctx->showTime[ctx->digitCursor] < 2) {
				ctx->showTime[ctx->digitCursor]++;
				if (ctx->showTime[ctx->digitCursor] == 2 && ctx->showTime[ctx->digitCursor + 1] > 3) {
					ctx->showTime[ctx->digitCursor + 1] = 3;
				}
			}
		} else {
			// Units digit: 0-9 (tens<2) or 0-3 (tens==2)
			uint8_t max = (ctx->showTime[tensIdx] == 2) ? 3 : 9;
			if (ctx->showTime[ctx->digitCursor] < max) {
				ctx->showTime[ctx->digitCursor]++;
			}
		}
		break;
	}

	case DISP_EV_BTN_DEC:
		if (ctx->showTime[ctx->digitCursor] > 0) {
			ctx->showTime[ctx->digitCursor]--;
		}
		break;

	default:
		break;
	}

	if (ctx->digitCursor == 4) {
		// Save to backup registers and persist to Flash
		setSilentHours(ctx->showTime[0] * 10 + ctx->showTime[1],
				ctx->showTime[2] * 10 + ctx->showTime[3]);
		flashWriteSettings();
		if (ctx->setupMode) {
			enterSetCorrection(ctx, buf);
		} else {
			ssd1306_ClearScreen();
			ctx->state = DISP_CLOCK;
		}
	} else {
		displayShowSilentHours(ctx->showTime);
		displayCursorSetTime(ctx->digitCursor, 96);
	}
}


// Handle button events in DISP_SET_CORRECTION state (±NNN editing, 0-511)
static void handleSetCorrBtns(uint32_t eventId, displayCtx_t *ctx, char *buf) {
	switch (eventId) {
	case DISP_EV_BTN_SET:
		displayCursorSetTime(ctx->digitCursor, 32);
		ctx->digitCursor++;
		break;

	case DISP_EV_BTN_INC:
		if (ctx->digitCursor == 0) {
			ctx->showTime[0] = !ctx->showTime[0];
		} else if (ctx->digitCursor == 1) {
			// Hundreds: 0-5, clamp lower digits to keep <= 511
			if (ctx->showTime[1] < 5) {
				ctx->showTime[1]++;
				if (ctx->showTime[1] == 5) {
					if (ctx->showTime[2] > 1) ctx->showTime[2] = 1;
					if (ctx->showTime[2] == 1 && ctx->showTime[3] > 1) ctx->showTime[3] = 1;
				}
			}
		} else if (ctx->digitCursor == 2) {
			uint8_t max = (ctx->showTime[1] == 5) ? 1 : 9;
			if (ctx->showTime[2] < max) {
				ctx->showTime[2]++;
				if (ctx->showTime[1] == 5 && ctx->showTime[2] == 1 && ctx->showTime[3] > 1) {
					ctx->showTime[3] = 1;
				}
			}
		} else {
			uint8_t max = (ctx->showTime[1] == 5 && ctx->showTime[2] == 1) ? 1 : 9;
			if (ctx->showTime[3] < max) {
				ctx->showTime[3]++;
			}
		}
		break;

	case DISP_EV_BTN_DEC:
		if (ctx->digitCursor == 0) {
			ctx->showTime[0] = !ctx->showTime[0];
		} else {
			if (ctx->showTime[ctx->digitCursor] > 0) {
				ctx->showTime[ctx->digitCursor]--;
			}
		}
		break;

	default:
		break;
	}

	if (ctx->digitCursor == 4) {
		// Save to backup register, persist to Flash, and apply immediately
		uint16_t value = ctx->showTime[1] * 100 + ctx->showTime[2] * 10 + ctx->showTime[3];
		setCalibration(ctx->showTime[0], value);
		applyCalibration();
		flashWriteSettings();
		if (ctx->setupMode) {
			enterSetRtc(ctx, buf);
		} else {
			ssd1306_ClearScreen();
			ctx->state = DISP_CLOCK;
		}
	} else {
		displayShowCalibration(ctx->showTime);
		displayCursorSetTime(ctx->digitCursor, 96);
	}
}


// #### Main Display Task ####
static void displayTask(void *parameters) {
	char buf[11];
	uint32_t eventId;
	displayCtx_t ctx = {
		.state = DISP_SYNC,
		.digitCursor = 0,
		.isOn = OFF,
		.lastOnTime = xTaskGetTickCount(),
		.lastClockUpdate = xTaskGetTickCount(),
	};

	while (1) {
		if (xTaskNotifyWait(0, 0xffffffff, &eventId, pdMS_TO_TICKS(DISPLAY_TASK_DELAY)) == pdTRUE) {

			// First boot → full setup: silent hours → calibration → time
			if (eventId == DISP_EV_FORCE_SETUP) {
				ctx.setupMode = 1;
				enterSetSilent(&ctx, buf);
				displayOnOff(ON, &ctx);
				vTaskResume(buttonTaskHandle);
				continue;
			}

			// *** BUTTON EVENTS (101-106) ***
			if ((eventId > 100) && (eventId < 200)) {
				uint8_t wasOff = !ctx.isOn;
				displayOnOff(ON, &ctx);

				if (wasOff) {
					vTaskResume(buttonTaskHandle);
					continue;  // Wake only, don't process
				}

				switch (ctx.state) {
				case DISP_CLOCK:          handleClockBtns(eventId, &ctx, buf);  break;
				case DISP_SET_RTC:        handleSetRtcBtns(eventId, &ctx);      break;
				case DISP_SET_SILENT:     handleSetSilentBtns(eventId, &ctx, buf);  break;
				case DISP_SET_CORRECTION: handleSetCorrBtns(eventId, &ctx, buf);  break;
				case DISP_ERROR:          break;
				case DISP_SYNC:           break;
				}

				if (ctx.state != DISP_SYNC) {
					vTaskResume(buttonTaskHandle);
				}
			}

			// *** SYNC EVENTS (201-206) ***
			if ((eventId > 200) && (eventId < 300)) {
				ctx.state = DISP_SYNC;
				displayOnOff(ON, &ctx);

				if (eventId == DISP_EV_SYN_START) {
					ssd1306_ClearScreen();
					displayTitle(ctx.state, buf);
				}

				displayMessage(eventId - DISP_EV_SYN_START, buf);

				if (eventId == DISP_EV_SYN_END) {
					ctx.state = DISP_CLOCK;
					vTaskDelay(pdMS_TO_TICKS(1000));
					ssd1306_ClearScreen();
					displayOnOff(ON, &ctx);
					ctx.lastClockUpdate = xTaskGetTickCount() - pdMS_TO_TICKS(DISPLAY_CLOCK_INTERVAL) - 10;
					vTaskResume(buttonTaskHandle);
				}
			}

			// *** ERROR EVENTS (301-309) ***
			if ((eventId > 300) && (eventId < 400)) {
				ctx.state = DISP_ERROR;
				displayOnOff(ON, &ctx);
				displayTitle(ctx.state, buf);
				displayMessage(eventId - DISP_EV_ERR_START, buf);
				vTaskResume(buttonTaskHandle);
			}

		} else {  // Event wait timed out

			// Time clock display
			if (ctx.state == DISP_CLOCK) {
				if (timeLapsed(xTaskGetTickCount(), ctx.lastClockUpdate) > pdMS_TO_TICKS(DISPLAY_CLOCK_INTERVAL)) {
					displayTitle(DISP_CLOCK, buf);
					displayUpdateTimeVar(ctx.showTime);
					displayShowClock(ctx.showTime);
					ctx.lastClockUpdate = xTaskGetTickCount();
				}
			}

			// Display auto shut off
			if (ctx.state != DISP_SYNC) {
				if (timeLapsed(xTaskGetTickCount(), ctx.lastOnTime) > pdMS_TO_TICKS(DISPLAY_OFF_TIMEOUT)) {
					displayOnOff(OFF, &ctx);
					if (ctx.state != DISP_ERROR) {
						ctx.state = DISP_CLOCK;
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
			xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_FORCE_SETUP, eSetValueWithOverwrite);
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

	// Battery was lost — restore silent hours and calibration from Flash
	if (!clockTaskInitState) {
		flashRestoreSettings();
	}

    resetMechPosition();    // Force in startup to search for 0 in order to sincronize the Tick Tock
	applyCalibration();		// Apply RTC smooth calibration from backup register

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

