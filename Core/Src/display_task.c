/**
 * @file   display_task.c
 * @brief  OLED display controller and UI state machine task.
 *
 * @version 2.0
 * @date    12/02/2026
 * @author  Alfredo Cortellini
 *
 * @copyright Copyright (c) 2026 Alfredo Cortellini.
 *            Licensed under CC BY-NC-SA 4.0.
 *            See https://creativecommons.org/licenses/by-nc-sa/4.0/
 */

#include <string.h>

#include "rtos_init.h"
#include "rtc_helpers.h"
#include "display_task.h"
#include "ssd1306.h"


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


/**
 * @brief  Render time as large digits on OLED: HH:MM with colon separator.
 *
 *         Writes each digit at its fixed X position (DIGIT_*_X defines).
 *         Digit values are raw 0-9, converted to ASCII by adding 48.
 *
 * @param  time  Array of 4 digit values [tensHrs, unitsHrs, tensMins, unitsMins]
 */
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


/**
 * @brief  Render silent hours on OLED: HH-HH with dash separator.
 *
 *         Same layout as displayShowClock but uses '-' (ASCII 45) instead
 *         of ':' in the center position.
 *
 * @param  time  Array of 4 digit values [tensStart, unitsStart, tensEnd, unitsEnd]
 */
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


/**
 * @brief  Render calibration value on OLED: ±NNN (sign + 3 digits).
 *
 *         Position 0 shows '+' (ASCII 43) or '-' (ASCII 45) based on
 *         time[0] (1=plus, 0=minus). The colon position shows a blank
 *         space. Digits 1-3 show the numeric value (0-511).
 *
 * @param  time  Array [plusFlag, hundreds, tens, units]
 */
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


/**
 * @brief  Split current RTC time into individual digit values.
 *
 *         Reads from the global RTC_Time struct (which must have been
 *         populated by a prior HAL_RTC_GetTime call) and writes the four
 *         digit values into the provided array.
 *
 * @param[out] time  Array of 4 digits [tensHrs, unitsHrs, tensMins, unitsMins]
 */
static void displayUpdateTimeVar(uint8_t *time) {
	time[TEEN_HRS] = RTC_Time.Hours / 10;
	time[UNIT_HRS] = RTC_Time.Hours % 10;
	time[TEEN_MINS] = RTC_Time.Minutes / 10;
	time[UNIT_MINS] = RTC_Time.Minutes % 10;
}


/**
 * @brief  Draw or clear the cursor marker above a digit position.
 *
 *         Writes three small characters (symbol) above the digit to form
 *         a visual cursor. Use ASCII 96 (grave accent) to show the cursor, or
 *         ASCII 32 (' ') to erase it when moving to the next digit.
 *
 * @param  digit   Digit index (0-3), maps to X position via setTimeDigitPos[]
 * @param  symbol  ASCII character to draw (96 = show cursor, 32 = clear)
 */
static void displayCursorSetTime(uint8_t digit, const uint8_t symbol) {

	ssd1306_SetCursor(setTimeDigitPos[digit], DIGIT_TIME_Y - 2);
	ssd1306_WriteChar(symbol, DISP_FONT_S);
	ssd1306_SetCursor(setTimeDigitPos[digit] + 6, DIGIT_TIME_Y - 2);
	ssd1306_WriteChar(symbol, DISP_FONT_S);
	ssd1306_SetCursor(setTimeDigitPos[digit] + 10, DIGIT_TIME_Y - 2);
	ssd1306_WriteChar(symbol, DISP_FONT_S);
}


/**
 * @brief  Display a two-line informative message centered on the OLED.
 *
 *         Messages are stored in the dispMsg[9][2][11] array, indexed by msgId.
 *         Each message has two rows of up to 10 characters. The function clears
 *         margin characters on both sides of each row to handle variable-width
 *         centering from previous messages.
 *
 *         SSD1306_WIDTH and SSD1306_CHAR_WIDTH are display driver constants
 *         used to compute centered X positions.
 *
 * @param  msgId   Index into dispMsg[][] (0-8, derived from event subtraction)
 * @param  buffer  Caller-provided 11-byte char buffer used as scratch for strcpy
 */
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


/**
 * @brief  Display a title string centered on the top row (page 0) of the OLED.
 *
 *         Titles are stored in the dispTitle[6][11] array, indexed by the
 *         current display state (dispStateEnum). All titles are 10 characters
 *         wide, so no margin clearing is needed (unlike displayMessage).
 *
 * @param  msgId   Index into dispTitle[] (typically ctx.state cast to uint8_t)
 * @param  buffer  Caller-provided 11-byte char buffer used as scratch for strcpy
 */
static void displayTitle(uint8_t msgId, char *buffer) {
	// Clear the margins
	ssd1306_SetCursor((SSD1306_WIDTH - (SSD1306_CHAR_WIDTH * 2 * strlen(dispTitle[msgId]))) / 2, 0);
	strcpy(buffer, dispTitle[msgId]);
	ssd1306_WriteString(buffer, DISP_FONT_S);
}


/**
 * @brief  Turn the OLED display on or off, tracking state in the context.
 *
 *         Only sends the hardware command (ssd1306_SetDisplayOnOff) when the
 *         requested state differs from the current state, avoiding redundant
 *         I2C traffic. When turning ON, resets the auto-off timer by updating
 *         ctx->lastOnTime with the current tick count.
 *
 *         xTaskGetTickCount() returns the current RTOS tick count (a
 *         monotonically increasing counter incremented by the SysTick ISR,
 *         typically every 1 ms). Used here to timestamp the last display
 *         wake-up for auto-off timeout comparison.
 *
 * @param  newState  ON (1) or OFF (0), from onOffEnum
 * @param  ctx       Display context — reads/writes ctx->isOn, ctx->lastOnTime
 */
static void displayOnOff(uint8_t newState, displayCtx_t *ctx) {
	if (newState != ctx->isOn) {
		ssd1306_SetDisplayOnOff(newState);
		ctx->isOn = newState;
	}
	if (newState == ON) {
		ctx->lastOnTime = xTaskGetTickCount();
	}
}


/**
 * @brief  Enter the RTC time-setting sub-menu (DISP_SET_RTC).
 *
 *         Shared entry point for both first-boot setup (end of wizard chain)
 *         and manual long-press-SET entry from DISP_CLOCK. Clears the screen,
 *         loads current RTC time into the digit array, displays the title and
 *         clock digits, and positions the cursor on the first digit (tens hours).
 *
 *         Always clears ctx->setupMode to 0 because this is the LAST screen
 *         in the setup wizard chain (silent → calibration → time).
 *
 * @param  ctx  Display context — sets state, digitCursor, setupMode, showTime[]
 * @param  buf  Caller-provided 11-byte scratch buffer for displayTitle
 */
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


/**
 * @brief  Enter the silent hours sub-menu (DISP_SET_SILENT).
 *
 *         Loads current silent start/end hours from backup register DR3 into
 *         the digit array as [tensStart, unitsStart, tensEnd, unitsEnd].
 *         Clears the screen and renders the title, HH-HH display, and cursor.
 *
 *         Entry paths:
 *         - First-boot wizard (DISP_EV_FORCE_SETUP → setupMode=1)
 *         - Manual: long-press INC from DISP_CLOCK
 *
 *         Does NOT clear setupMode — the caller sets it before calling.
 *
 * @param  ctx  Display context — sets state, digitCursor, showTime[]
 * @param  buf  Caller-provided 11-byte scratch buffer for displayTitle
 */
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


/**
 * @brief  Enter the RTC calibration sub-menu (DISP_SET_CORRECTION).
 *
 *         Reads the current calibration from backup register DR4 and splits
 *         it into [plusFlag, hundreds, tens, units] in the digit array.
 *         Clears the screen and renders the title, ±NNN display, and cursor.
 *
 *         Entry paths:
 *         - Setup wizard chain (from handleSetSilentBtns when setupMode=1)
 *         - Manual: long-press DEC from DISP_CLOCK
 *
 * @param  ctx  Display context — sets state, digitCursor, showTime[]
 * @param  buf  Caller-provided 11-byte scratch buffer for displayTitle
 */
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


/**
 * @brief  Handle button events while in DISP_CLOCK (idle) state.
 *
 *         Only long presses trigger sub-menu transitions:
 *         - Long SET → DISP_SET_RTC (blocked if inside silent period)
 *         - Long INC → DISP_SET_SILENT (edit silent hours)
 *         - Long DEC → DISP_SET_CORRECTION (edit RTC calibration)
 *         Short presses are no-ops (display was already woken by caller).
 *
 * @param  eventId  Button event code (101-106)
 * @param  ctx      Display context — may transition state
 * @param  buf      11-byte scratch buffer for display rendering
 */
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


/**
 * @brief  Handle button events in DISP_SET_RTC state (time digit editing).
 *
 *         SET button advances cursor to next digit (0→1→2→3→commit).
 *         INC/DEC buttons modify the digit at the current cursor position
 *         with validation: hours 00-23 (tens 0-2, units clamped to 0-3 when
 *         tens=2), minutes 00-59 (tens 0-5, units 0-9).
 *
 *         When the cursor reaches position 4 (past the last digit):
 *         - Writes the entered time to the RTC via HAL_RTC_SetTime/SetDate
 *           inside a critical section (interrupts disabled to prevent
 *           partial RTC register writes)
 *         - Transitions to DISP_SYNC and notifies clockTask with
 *           CLOCK_EV_NEW_TIME to start mechanical synchronization
 *
 *         xTaskNotify(handle, value, eSetValueWithOverwrite): sends a 32-bit
 *         notification value to the target task. eSetValueWithOverwrite means
 *         the value is written unconditionally (overwrites any pending
 *         unread notification).
 *
 * @param  eventId  Button event code (101-103 for short presses)
 * @param  ctx      Display context — reads/writes showTime[], digitCursor, state
 */
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


/**
 * @brief  Handle button events in DISP_SET_SILENT state (HH-HH editing).
 *
 *         Edits two hour values (start and end of silent period) displayed
 *         as HH-HH. Same digit editing pattern as handleSetRtcBtns but
 *         both pairs are validated as hours (0-23).
 *
 *         On commit (cursor reaches 4):
 *         - Saves start/end hours to backup register DR3 via setSilentHours()
 *         - Persists to Flash via flashWriteSettings()
 *         - If setupMode=1 (first-boot wizard): chains to enterSetCorrection()
 *         - If setupMode=0 (manual entry): returns to DISP_CLOCK
 *
 * @param  eventId  Button event code (101-103 for short presses)
 * @param  ctx      Display context — reads/writes showTime[], digitCursor, state
 * @param  buf      11-byte scratch buffer for display rendering
 */
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


/**
 * @brief  Handle button events in DISP_SET_CORRECTION state (±NNN editing).
 *
 *         Edits the RTC smooth calibration value displayed as ±NNN (0-511).
 *         Digit 0 toggles the sign (+/-). Digits 1-3 edit the numeric value
 *         with clamping to ensure the total never exceeds 511.
 *
 *         On commit (cursor reaches 4):
 *         - Saves to backup register DR4 via setCalibration()
 *         - Applies immediately to RTC hardware via applyCalibration()
 *         - Persists to Flash via flashWriteSettings()
 *         - If setupMode=1 (first-boot wizard): chains to enterSetRtc()
 *         - If setupMode=0 (manual entry): returns to DISP_CLOCK
 *
 * @param  eventId  Button event code (101-103 for short presses)
 * @param  ctx      Display context — reads/writes showTime[], digitCursor, state
 * @param  buf      11-byte scratch buffer for display rendering
 */
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


/**
 * @brief  FreeRTOS task: OLED display controller and UI state machine.
 *
 *         This task owns the SSD1306 OLED display. It receives notifications
 *         from buttonTask (user input) and clockTask (sync progress/errors),
 *         and updates the display accordingly.
 *
 *         State machine (ctx.state):
 *         - DISP_CLOCK:          Idle, shows current RTC time (HH:MM)
 *         - DISP_SET_RTC:        Digit-by-digit time editing
 *         - DISP_SET_SILENT:     Digit-by-digit silent hours editing (HH-HH)
 *         - DISP_SET_CORRECTION: Digit-by-digit calibration editing (±NNN)
 *         - DISP_SYNC:           Mechanical sync in progress (buttons disabled)
 *         - DISP_ERROR:          Error message displayed
 *
 *         Event handling:
 *         - DISP_EV_FORCE_SETUP (999): First-boot wizard chain
 *         - Button events (101-106): Dispatched to per-state handlers
 *         - Sync events (201-206): Show sync progress messages
 *         - Error events (301-309): Show error messages
 *         - Timeout (no event): Refresh clock display + auto-off check
 *
 *         Display wake logic: if display is OFF and any button is pressed,
 *         the display turns ON but the event is NOT forwarded to handlers
 *         (wake-only). This prevents accidental sub-menu entry.
 *
 *         xTaskNotifyWait(clearOnEntry, clearOnExit, &value, timeout):
 *         blocks the task until a notification arrives or timeout expires.
 *         - clearOnEntry/Exit: bit masks to clear from the notification value
 *         - Returns pdTRUE if a notification was received, pdFALSE on timeout
 *         - Here: clears all bits on exit (0xffffffff) to consume the event
 *
 *         vTaskResume(handle): resumes a suspended task. Used here to
 *         re-enable buttonTask after processing its event. Safe to call
 *         on an already-running task (no-op).
 *
 *         pdMS_TO_TICKS(ms): converts milliseconds to RTOS tick count
 *         (depends on configTICK_RATE_HZ, typically 1000 = 1ms/tick).
 *
 * @param  parameters  Unused (NULL)
 */
void displayTask(void *parameters) {
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
