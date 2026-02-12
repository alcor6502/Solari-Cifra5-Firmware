/**
 * @file   rtos_init.c
 * @brief  RTOS initialization: peripheral handle storage, task creation,
 *         and stack overflow hook.
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
#include "display_task.h"
#include "button_task.h"
#include "clock_task.h"


/*
 *  Global variable definitions (declared extern in rtos_init.h)
 */
TIM_HandleTypeDef *htimHandle;
RTC_HandleTypeDef *hrtcHandle;
TaskHandle_t displayTaskHandle;
TaskHandle_t buttonTaskHandle;
TaskHandle_t clockTaskHandle;


/**
 * @brief  Store peripheral handles for use by RTOS tasks.
 *
 *         Called from main() before the scheduler starts. Saves pointers
 *         to the timer (servo PWM) and RTC (timekeeping + backup registers)
 *         HAL handles into globals so all tasks can access them.
 *
 * @param  htim  Pointer to TIM handle (TIM_CHANNEL_4 used for servo PWM)
 * @param  hrtc  Pointer to RTC handle (time read/write + backup registers)
 */
void initRTOS_Periferals(TIM_HandleTypeDef *htim, RTC_HandleTypeDef *hrtc) {
	htimHandle = htim;
	hrtcHandle = hrtc;
}


/**
 * @brief  Create all FreeRTOS tasks and perform pre-scheduler initialization.
 *
 *         Called from main() after initRTOS_Periferals(). Runs before the
 *         scheduler starts (no RTOS API except task creation is safe here).
 *
 *         Initialization steps:
 *         1. Check ICSR.INITS bit — if 0, the RTC lost power (battery removed).
 *            In that case, restore silent hours and calibration from Flash.
 *         2. Reset mechanical position to 00:00 (forces sensor search on sync).
 *         3. Apply RTC smooth calibration from backup register to hardware.
 *         4. Create 3 tasks (displayTask, buttonTask, clockTask) with
 *            configASSERT to halt on creation failure.
 *         5. Suspend buttonTask — buttons are disabled until clockTask
 *            completes its first sync and displayTask resumes them.
 *
 *         clockTask receives rtcInitOk as its parameter (cast to void*):
 *         - 0 = first boot, clock never set → triggers setup wizard
 *         - non-zero = RTC valid, proceed to sync immediately
 *
 *         xTaskCreate(function, name, stackDepth, param, priority, &handle):
 *         creates a new task. stackDepth is in WORDS (not bytes) — e.g.
 *         120 words = 480 bytes on 32-bit ARM. Priority 2 means all three
 *         tasks have equal priority and share CPU via round-robin.
 *
 *         configASSERT(expr): if expr is false, calls a fault handler
 *         (stops the system). Used here to catch out-of-memory errors.
 *
 */
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

}


/**
 * @brief  FreeRTOS stack overflow hook — called when a task exceeds its stack.
 *
 *         FreeRTOS checks for stack overflow on each context switch (when
 *         configCHECK_FOR_STACK_OVERFLOW is set in FreeRTOSConfig.h).
 *         If overflow is detected, the kernel calls this function with the
 *         offending task's handle and name.
 *
 *         This implementation calls Error_Handler() (defined in main.c),
 *         which enters an infinite loop — effectively halting the system
 *         for debugging. In a release build this could trigger a watchdog
 *         reset instead.
 *
 * @param  xTask      Handle of the task that overflowed (unused here)
 * @param  pcTaskName Human-readable name of the task (unused here)
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
	Error_Handler();
}
