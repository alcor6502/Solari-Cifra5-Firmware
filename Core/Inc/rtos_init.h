/**
 * @file   rtos_init.h
 * @brief  Shared definitions, enums, typedefs, and extern globals for all
 *         RTOS modules. Included by every task .c file and by main.c.
 *
 * @version 2.0
 * @date    12/02/2026
 * @author  Alfredo Cortellini
 *
 * @copyright Copyright (c) 2026 Alfredo Cortellini.
 *            Licensed under CC BY-NC-SA 4.0.
 *            See https://creativecommons.org/licenses/by-nc-sa/4.0/
 */

#ifndef _RTOS_INIT_H_
#define _RTOS_INIT_H_

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"


/*
 *  Shared global Message notification
 */
#define CLOCK_EV_NEW_TIME		10

/*
 *  Shared global enumerator (defined in rtos_init.c and rtc_helpers.c)
 */
enum dispEventIdEnum{
	DISP_EV_BTN_SET = 101,
	DISP_EV_BTN_INC = 102,
	DISP_EV_BTN_DEC = 103,
	DISP_EV_BTN_SET_LONG = 104,
	DISP_EV_BTN_INC_LONG = 105,
	DISP_EV_BTN_DEC_LONG = 106,
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
	DISP_EV_FORCE_SETUP = 999
};

/*
 *  Shared global variables (defined in rtos_init.c and rtc_helpers.c)
 */
extern TIM_HandleTypeDef *htimHandle;
extern RTC_HandleTypeDef *hrtcHandle;
extern TaskHandle_t displayTaskHandle;
extern TaskHandle_t buttonTaskHandle;
extern TaskHandle_t clockTaskHandle;
extern RTC_TimeTypeDef RTC_Time;
extern RTC_DateTypeDef RTC_Date;


/*
 *  Public API (called from main.c)
 */
void initRTOS_Periferals(TIM_HandleTypeDef *htim, RTC_HandleTypeDef *hrtc);
void createRTOS_Tasks(void);


/*  Determine amount of ticks lapsed between a and b.
 *  Times a and b are unsigned, but performing the comparison
 *  using signed arithmetic automatically handles wrapping.
 *  The disambiguation window is half the maximum value. */
#if (configUSE_16_BIT_TICKS == 1)
#define timeLapsed(a,b)    ((int16_t)(a) - (int16_t)(b))
#else
#define timeLapsed(a,b)    ((int32_t)(a) - (int32_t)(b))
#endif


#endif /* _RTOS_INIT_H_ */
