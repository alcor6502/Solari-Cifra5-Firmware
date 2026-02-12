/**
 * @file   display_task.h
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

#ifndef _DISPLAY_TASK_H_
#define _DISPLAY_TASK_H_

#include "rtos_init.h"

void displayTask(void *parameters);

/*
 *  Display Task enum and definitions
 */
// Timeouts and delays
#define DISPLAY_OFF_TIMEOUT		30000
#define DISPLAY_CLOCK_INTERVAL	500
#define DISPLAY_TASK_DELAY		20		// Frequency Display update Task


// Display Font Size
#define DISP_FONT_S 			1
#define DISP_FONT_L 			3

// Clock big digit positions
#define DIGIT_TIME_Y			4
#define DIGIT_TEEN_HRS_X		0
#define DIGIT_UNIT_HRS_X		26
#define DIGIT_COLON_X			54
#define DIGIT_TEEN_MINS_X		78
#define DIGIT_UNIT_MINS_X		104

// Clock Message
#define DISP_MSG01_ROW01				"  ADJUST  \0"
#define DISP_MSG01_ROW02				"  START  \0"
#define DISP_MSG02_ROW01				"SEARCH FOR\0"
#define DISP_MSG02_ROW02				"00 MINUTES\0"
#define DISP_MSG03_ROW01				"SEARCH FOR\0"
#define DISP_MSG03_ROW02				" 00 HOURS \0"
#define DISP_MSG04_ROW01				" SETTING \0"
#define DISP_MSG04_ROW02				"  HOURS  \0"
#define DISP_MSG05_ROW01				" SETTING \0"
#define DISP_MSG05_ROW02				" MINUTES \0"
#define DISP_MSG06_ROW01				"  ADJUST  \0"
#define DISP_MSG06_ROW02				" COMPLETE \0"
#define DISP_MSG07_ROW01				" MISSING \0"
#define DISP_MSG07_ROW02				"HOUR SENS.\0"
#define DISP_MSG08_ROW01				" MISSING \0"
#define DISP_MSG08_ROW02				"DAY SENS.\0"
#define DISP_MSG09_ROW01				" TOO MANY \0"
#define DISP_MSG09_ROW02				" SYNCROS \0"

// Clock Titles (indexed by dispStateEnum)
#define DISP_TITLE01					"CLOCK TIME\0"
#define DISP_TITLE02					" SET TIME \0"
#define DISP_TITLE03					"SILENT SET\0"
#define DISP_TITLE04					"RTC ADJUST\0"
#define DISP_TITLE05					"SYNC CLOCK\0"
#define DISP_TITLE06					"SYNC ERROR\0"




// Display state enumerator (order matches title array)
enum dispStateEnum{
	DISP_CLOCK,
	DISP_SET_RTC,
	DISP_SET_SILENT,
	DISP_SET_CORRECTION,
	DISP_SYNC,
	DISP_ERROR,
};

// Digit selection enumerator
enum setTimeEnum{
	TEEN_HRS,
	UNIT_HRS,
	TEEN_MINS,
	UNIT_MINS
};

enum onOffEnum{
	OFF,
	ON,
};

// Display task context (lives on displayTask stack)
typedef struct {
	uint8_t state;
	uint8_t showTime[4];
	uint8_t digitCursor;
	uint8_t isOn;
	uint8_t setupMode;
	TickType_t lastOnTime;
	TickType_t lastClockUpdate;
} displayCtx_t;

#endif /* _DISPLAY_TASK_H_ */
