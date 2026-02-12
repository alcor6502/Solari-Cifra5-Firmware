/**
 * @file   clock_task.h
 * @brief  Mechanical clock synchronization and minute ticking task.
 *
 * @version 2.0
 * @date    12/02/2026
 * @author  Alfredo Cortellini
 *
 * @copyright Copyright (c) 2026 Alfredo Cortellini.
 *            Licensed under CC BY-NC-SA 4.0.
 *            See https://creativecommons.org/licenses/by-nc-sa/4.0/
 */

#ifndef _CLOCK_TASK_H_
#define _CLOCK_TASK_H_

#include "rtos_init.h"

void clockTask(void *parameters);

/*  
 *   Clock Task Structure and definitions
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

// Timeouts and delays
#define CLOCK_UPDATE_INTERVAL	100


#endif /* _CLOCK_TASK_H_ */
