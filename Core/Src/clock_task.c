/**
 * @file   clock_task.c
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

#include "rtos_init.h"
#include "rtc_helpers.h"
#include "clock_task.h"


/**
 * @brief  Initialize servo PWM and move to release (neutral) position.
 *
 *         Starts with CCR4=0 (servo off), enables PWM on TIM_CHANNEL_4,
 *         waits 200ms for the servo to power up, then sets the release
 *         position and waits for it to settle.
 *
 *         The servo controls the hour flap advancement mechanism:
 *         - PARKING: arm retracted (off position)
 *         - RELEASE: arm at neutral (ready to engage)
 *         - ENGAGE:  arm pushes the flap forward by one step
 *
 *         HAL_TIM_PWM_Start(htim, channel): starts PWM output on the
 *         specified timer channel. The duty cycle is set via CCR4 register.
 *
 *         vTaskDelay(ticks): suspends the calling task for the specified
 *         number of ticks, yielding the CPU to other tasks. Unlike a busy
 *         wait, the task consumes no CPU time while delayed.
 *
 */
static void prepareServo(void) {
        htimHandle->Instance->CCR4 = 0;
        HAL_TIM_PWM_Start(htimHandle, TIM_CHANNEL_4);
        vTaskDelay(pdMS_TO_TICKS(200));
        htimHandle->Instance->CCR4 = SERVO_RELEASE_PWM;
        vTaskDelay(pdMS_TO_TICKS(SERVO_PARK_TIME));
}


/**
 * @brief  Advance the mechanical hour flap by one position using the servo.
 *
 *         Performs one engage→release cycle: pushes the servo arm to the
 *         engage position (SERVO_ENGAGE_PWM) to flip one hour flap, then
 *         returns to the release position. Updates the mechanical hour
 *         counter in backup registers via incrementMechHour().
 *
 *         The servo must be initialized with prepareServo() before calling.
 *         After all hour advances are done, call shutdownServo() to park.
 *
 *         Guarded by CIFRA5_DEBUG: when defined, skips the physical servo
 *         movement but still updates the backup register (for testing).
 *
 */
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

/**
 * @brief  Move servo to parking position and stop PWM output.
 *
 *         Moves the servo arm to SERVO_PARKING_PWM (fully retracted),
 *         waits for it to settle, then sets CCR4=0 and stops the PWM
 *         timer to eliminate idle current draw from the servo.
 *
 *         HAL_TIM_PWM_Stop(htim, channel): stops PWM generation on the
 *         timer channel. The output pin goes to its idle level.
 *
 */
static void shutdownServo(void) {
	htimHandle->Instance->CCR4 = SERVO_PARKING_PWM;
	vTaskDelay(pdMS_TO_TICKS(SERVO_PARK_TIME));
	htimHandle->Instance->CCR4 = 0;
	HAL_TIM_PWM_Stop(htimHandle, TIM_CHANNEL_4); // Stop to generate PWM signal
    vTaskDelay(pdMS_TO_TICKS(500));
}

/**
 * @brief  Advance the mechanical minute flap by one position using the coil.
 *
 *         The minute mechanism uses an electromagnetic coil that alternates
 *         between two pins (tick/tock) on each advance. The alternation state
 *         is tracked in backup register DR2 bit 0 via getLastTick/setLastTick.
 *
 *         Each pulse: drive the coil pin LOW (excite), wait coilExcite ms,
 *         then drive HIGH (release), wait coilRest ms. The slow parameter
 *         adds COIL_EXTRA_TIME to both durations for gentler movement during
 *         normal operation (vs. fast sync).
 *
 *         After the pulse, updates the mechanical minute counter via
 *         incrementMechMinute() and saves the new tick state.
 *
 *         HAL_GPIO_WritePin(port, pin, state): directly sets/clears a GPIO
 *         pin. GPIO_PIN_RESET=0 (coil energized), GPIO_PIN_SET=1 (coil off).
 *
 *         Guarded by CIFRA5_DEBUG: when defined, skips physical GPIO
 *         toggling but still updates backup registers.
 *
 * @param  slow  0 = fast (sync mode), 1 = slow (normal tick with extra delay)
 */
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



/**
 * @brief  Find the mechanical 00:00 position using physical sensors.
 *
 *         Called on first sync when the mechanical position is unknown (both
 *         mech hours and minutes are 0 in backup registers = not yet calibrated).
 *
 *         Phase 1 — Find 00 minutes:
 *         Advances the minute flap until the hour sensor detects a 0→1
 *         transition (falling edge = magnet leaving sensor). This indicates
 *         the minutes have crossed an hour boundary (XX:00).
 *         Error after 62 attempts (> 60 minutes = sensor missing).
 *
 *         Phase 2 — Find 00 hours:
 *         Activates the servo, then advances the hour flap until the day
 *         sensor detects a 1→0 transition (rising edge = 24→00 rollover).
 *         Error after 25 attempts (> 24 hours = sensor missing).
 *
 *         On error, notifies displayTask with an error event and suspends.
 *         The task remains suspended until the user power-cycles the device.
 *
 *         vTaskSuspend(NULL): suspends the calling task indefinitely.
 *         The task will not run again until another task calls
 *         vTaskResume(handle) on it. Passing NULL means "suspend myself".
 *
 */
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


/**
 * @brief  Advance the hour flap to match the target RTC hour.
 *
 *         If the servo is not already at release position, calls
 *         prepareServo() first (prevents double-initialization). Then
 *         loops clockAdvHour() until the mechanical hours (from backup
 *         registers) match targetHours. Finally parks the servo.
 *
 *         Since hours wrap around 0-23, this handles any direction
 *         (e.g. mechanical=22, target=3 → advances 5 times through 0).
 *
 * @param  targetHours  Desired hour position (0-23, from RTC)
 */
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


/**
 * @brief  Advance the minute flap to match the target RTC minutes.
 *
 *         Assumes the mechanical clock is at XX:00 (either from sensor
 *         search or advanceToHourBoundary). Advances targetMinutes times
 *         using fast coil pulses (slow=0).
 *
 * @param  targetMinutes  Number of minute steps to advance (0-59, from RTC)
 */
static void syncMinutes(uint8_t targetMinutes) {
	xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_SYN_SET_MIN, eSetValueWithOverwrite);

	for (uint8_t i = 0; i < targetMinutes; i++) {
		clockAdvMinute(0);
	}
}


/**
 * @brief  Advance minutes from current position to the next hour boundary (XX:00).
 *
 *         Used in "fast sync" when the mechanical position is known from
 *         backup registers (not 00:00). If minutes are already at 0, does
 *         nothing. Otherwise advances (60 - currentMinutes) times to reach
 *         the next hour, which also increments the mechanical hour.
 *
 *         After this function, the mechanical clock is at (H+1):00 and
 *         syncHours/syncMinutes can set the exact target.
 *
 */
static void advanceToHourBoundary(void) {
	uint8_t minutes = getMechMinutes();
	if (minutes != 0) {
		xTaskNotify(displayTaskHandle, (uint32_t) DISP_EV_SYN_SRC_HOUR, eSetValueWithOverwrite);
		for (uint8_t i = minutes; i < 60; i++) {
			clockAdvMinute(0);
		}
	}
}


/**
 * @brief  FreeRTOS task: mechanical clock synchronization and minute ticking.
 *
 *         Manages the physical Solari Cifra 5 flip clock mechanism. Runs in
 *         three phases inside an outer while(1) loop:
 *
 *         PHASE 1 — PRE-SYNC:
 *         On first boot (rtcInitOk=0, RTC never initialized), sends
 *         DISP_EV_FORCE_SETUP to trigger the setup wizard and blocks on
 *         xTaskNotifyWait until the user finishes setting the time.
 *         Then waits out any active silent period (checks every 60s).
 *
 *         PHASE 2 — SYNC:
 *         Suspends buttonTask to prevent user interaction during sync.
 *         If mechanical position is 00:00 (unknown), runs sensor-based
 *         searchForZeroPosition(). Otherwise does a fast sync via
 *         advanceToHourBoundary(). Then reads RTC time and calls
 *         syncHours() + syncMinutes() to match the mechanical clock.
 *         Notifies displayTask with DISP_EV_SYN_END when complete.
 *
 *         PHASE 3 — NORMAL OPERATION:
 *         Polls every CLOCK_UPDATE_INTERVAL (100ms). Compares mechanical
 *         minutes (from backup registers) to RTC minutes. When they differ,
 *         advances one minute flap using clockAdvMinute(slow=1).
 *         Monitors the hour sensor during advances — an unexpected hour
 *         transition indicates mechanical drift and triggers a full resync
 *         (breaks back to Phase 2).
 *         Also handles silent period entry/exit (exit triggers resync).
 *
 *         xTaskNotifyWait(clearEntry, clearExit, &value, timeout): blocks
 *         until a notification arrives or timeout. In Phase 1, uses
 *         portMAX_DELAY (infinite wait). In Phase 3, uses 100ms timeout
 *         for periodic polling.
 *
 *         vTaskSuspend(handle): suspends another task. Used to disable
 *         buttonTask during sync. vTaskSuspend on an already-suspended
 *         task is a safe no-op.
 *
 * @param  parameters  Cast to uint8_t: 0 = RTC not initialized (first boot),
 *                     non-zero = RTC valid (normal boot / power cycle)
 */
void clockTask(void *parameters) {
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
