/*
 * task_handlers.h
 *
 *  Created on: May 12, 2021
 *      Author: Alfredo
 */

#ifndef _TASK_HANDLERS_H_
#define _TASK_HANDLERS_H_

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"


/*
 *  Function declaration
 */
void initRTOS_Periferals(TIM_HandleTypeDef *htim, RTC_HandleTypeDef *hrtc);
void createRTOS_Tasks(void);


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

// Clock Titles
#define DISP_TITLE01					"CLOCK TIME\0"
#define DISP_TITLE02					" SET TIME \0"
#define DISP_TITLE03					"SYNC CLOCK\0"
#define DISP_TITLE04					"SYNC ERROR\0"


// Events enumerator
enum dispEventIdEnum{
	DISP_EV_BTN_SET = 101,
	DISP_EV_BTN_INC = 102,
	DISP_EV_BTN_DEC = 103,
	DISP_EV_SYN_START = 201,
	DISP_EV_SYN_SRC_HOUR = 202,
	DISP_EV_SYN_SRC_DAY = 203,
	DISP_EV_SYN_SET_HOUR = 204,
	DISP_EV_SYN_SET_MIN = 205,
	DISP_EV_SYN_END = 206,
	DISP_EV_ERR_START = 301,
	DISP_EV_ERR_SNS_HOUR = 307,
	DISP_EV_ERR_SNS_DAY = 308,
	DISP_EV_ERR_MANY_SYNC = 309,
	DISP_EV_FORCE_SET_TIME = 999
};

// Display state enumerator
enum dispStateEnum{
	DISP_CLOCK,
	DISP_SET_RTC,
	DISP_SYNC,
	DISP_ERROR,
	DISP_WAIT_SET_RTC,
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


/*
 *   Button Task Structure and definitions
 */

// Timeouts and delays
#define BTN_MAX				3 	// Remember to update this value with the actual number of Button installed on the board
#define BTN_DEBOUNCE		20 	// De-bounce time in multiple of 5ms
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

/*  Determine amount of ticks lapsed between a and b.
 *  Times a and b are unsigned, but performing the comparison
 *  using signed arithmetic automatically handles wrapping.
 *  The disambiguation window is half the maximum value. */
#if (configUSE_16_BIT_TICKS == 1)
#define timeLapsed(a,b)    ((int16_t)(a) - (int16_t)(b))
#else
#define timeLapsed(a,b)    ((int32_t)(a) - (int32_t)(b))
#endif


/*  Sync definitions
 *
 */

// Servo parameters
#define SERVO_PARKING_PWM		61
#define SERVO_RELEASE_PWM		74
#define SERVO_ENGAGE_PWM		81
#define SERVO_ENGAGE_TIME		300
#define SERVO_PARK_TIME			500

// Clock coil parameters
#define COIL_REST_TIME			200
#define COIL_EXCITE_TIME		200
#define COIL_EXTRA_TIME			0


/*  Clock Task Structure and definitions
 *
 */

// Timeouts and delays
#define CLOCK_UPDATE_INTERVAL	100

// Silent mode configuration
#define SILENT_START_HOUR		22		// Hour when silent period starts (no mechanical movement)
#define SILENT_END_HOUR			9		// Hour when silent period ends

// RTC Backup Register allocation
#define RTC_BKP_MECH_HOURS		RTC_BKP_DR0  // Mechanical clock hours (0-23)
#define RTC_BKP_MECH_MINUTES	RTC_BKP_DR1  // Mechanical clock minutes (0-59)
#define RTC_BKP_FLAGS			RTC_BKP_DR2  // Last tick state + other flags

// Backup register flag bits
#define RTC_BKP_FLAG_LAST_TICK	0x00000001  // Bit 0: last tick state (0=tick, 1=tock)

// Message notification to clock Task
#define CLOCK_EV_NEW_TIME		10

#endif /* _TASK_HANDLERS_H_ */
