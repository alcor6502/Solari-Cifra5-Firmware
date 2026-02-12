/**
 * @file   rtc_helpers.c
 * @brief  RTC backup register utilities, Flash settings storage,
 *         calibration, and silent period helpers.
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


// Struct to set and read the real time clock
RTC_TimeTypeDef RTC_Time;
RTC_DateTypeDef RTC_Date;


/*
 * ################################
 * #   RTC BACKUP REGISTER UTILS  #
 * ################################
 */

/**
 * @brief  Read mechanical clock hours from RTC backup register DR0.
 *
 *         The mechanical clock position is tracked in backup registers so it
 *         survives power cycles. Returns 0 if the stored value is out of
 *         range (> 23), which happens on first battery insertion.
 *
 * @return Mechanical hours (0-23)
 */
uint8_t getMechHours(void) {
	uint8_t hours = (uint8_t)(HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_MECH_HOURS) & 0xFF);
	return (hours > 23) ? 0 : hours;
}

/**
 * @brief  Read mechanical clock minutes from RTC backup register DR1.
 *
 *         Returns 0 if the stored value is out of range (> 59).
 *
 * @return Mechanical minutes (0-59)
 */
uint8_t getMechMinutes(void) {
	uint8_t minutes = (uint8_t)(HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_MECH_MINUTES) & 0xFF);
	return (minutes > 59) ? 0 : minutes;
}

/**
 * @brief  Write mechanical clock position to backup registers DR0 and DR1.
 *
 * @param  hours    Hour value to store (0-23)
 * @param  minutes  Minute value to store (0-59)
 */
static void setMechPosition(uint8_t hours, uint8_t minutes) {
	HAL_RTCEx_BKUPWrite(hrtcHandle, RTC_BKP_MECH_HOURS, (uint32_t)hours);
	HAL_RTCEx_BKUPWrite(hrtcHandle, RTC_BKP_MECH_MINUTES, (uint32_t)minutes);
}

/**
 * @brief  Increment mechanical minute by 1, with hour rollover.
 *
 *         Called after each coil pulse advances the minute flap.
 *         Handles 59→00 minute rollover and 23→00 hour rollover.
 *
 */
void incrementMechMinute(void) {
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

/**
 * @brief  Increment mechanical hour by 1, resetting minutes to 0.
 *
 *         Called after each servo actuation advances the hour flap.
 *         Handles 23→00 rollover. Minutes are set to 0 because the
 *         servo always advances from an hour boundary (XX:00).
 *
 */
void incrementMechHour(void) {
	uint8_t hours = getMechHours();
	hours++;
	if (hours >= 24) {
		hours = 0;
	}
	setMechPosition(hours, 0);
}

/**
 * @brief  Reset mechanical position to 00:00 in backup registers.
 *
 *         Called at startup to force a sensor-based search, and when
 *         mechanical drift is detected during normal operation.
 *
 */
void resetMechPosition(void) {
	setMechPosition(0, 0);
}

/**
 * @brief  Read last tick/tock state from backup register DR2 (bit 0).
 *
 *         The mechanical clock coil alternates between two pins (tick/tock)
 *         on each minute advance. This state must persist across power cycles
 *         to keep the alternation in sync with the mechanical mechanism.
 *
 * @return 0 = last was tick (next should be tock), 1 = last was tock
 */
uint8_t getLastTick(void) {
	uint32_t flags = HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_FLAGS);
	return (flags & RTC_BKP_FLAG_LAST_TICK) ? 1 : 0;
}

/**
 * @brief  Write last tick/tock state to backup register DR2 (bit 0).
 *
 *         Uses read-modify-write to preserve other flag bits in DR2.
 *
 * @param  tickState  0 = tick, 1 = tock
 */
void setLastTick(uint8_t tickState) {
	uint32_t flags = HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_FLAGS);

	if (tickState) {
		flags |= RTC_BKP_FLAG_LAST_TICK;
	} else {
		flags &= ~RTC_BKP_FLAG_LAST_TICK;
	}

	HAL_RTCEx_BKUPWrite(hrtcHandle, RTC_BKP_FLAGS, flags);
}

/**
 * @brief  Read silent period start hour from RTC backup register DR3.
 *
 *         DR3 packs two hours: start in bits [7:0], end in bits [15:8].
 *         Returns SILENT_DEFAULT_START (22) if the stored value is out of
 *         range (> 23).
 *
 * @return Start hour (0-23)
 */
uint8_t getSilentStartHour(void) {
	uint32_t raw = HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_SILENT);
	uint8_t hour = (uint8_t)(raw & 0xFF);
	return (hour > 23) ? SILENT_DEFAULT_START : hour;
}

/**
 * @brief  Read silent period end hour from RTC backup register DR3.
 *
 *         Extracts bits [15:8] from DR3. Returns SILENT_DEFAULT_END (9)
 *         if the stored value is out of range (> 23).
 *
 * @return End hour (0-23)
 */
uint8_t getSilentEndHour(void) {
	uint32_t raw = HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_SILENT);
	uint8_t hour = (uint8_t)((raw >> 8) & 0xFF);
	return (hour > 23) ? SILENT_DEFAULT_END : hour;
}

/**
 * @brief  Write silent period hours to backup register DR3.
 *
 *         Packs start in bits [7:0] and end in bits [15:8] of a single
 *         32-bit register.
 *
 * @param  startHour  Hour when silent period begins (0-23)
 * @param  endHour    Hour when silent period ends (0-23)
 */
void setSilentHours(uint8_t startHour, uint8_t endHour) {
	uint32_t raw = (uint32_t)startHour | ((uint32_t)endHour << 8);
	HAL_RTCEx_BKUPWrite(hrtcHandle, RTC_BKP_SILENT, raw);
}

/**
 * @brief  Read RTC smooth calibration from backup register DR4.
 *
 *         DR4 encoding: bit 9 = plus pulses flag, bits [8:0] = calibration
 *         value (0-511). If the raw register exceeds 0x3FF (invalid), both
 *         outputs default to 0 (no correction applied).
 *
 *         The STM32 RTC smooth calibration adds or removes clock pulses
 *         every 32-second window to fine-tune the RTC oscillator.
 *
 * @param[out] plusPulses  1 = add pulses (speed up), 0 = remove pulses (slow down)
 * @param[out] calibValue Number of pulses to add/remove per 32s (0-511)
 */
void getCalibration(uint8_t *plusPulses, uint16_t *calibValue) {
	uint32_t raw = HAL_RTCEx_BKUPRead(hrtcHandle, RTC_BKP_CALIB);
	if (raw > 0x3FF) {
		*plusPulses = 0;
		*calibValue = 0;
		return;
	}
	*plusPulses = (raw & 0x200) ? 1 : 0;
	*calibValue = raw & 0x1FF;
}

/**
 * @brief  Write RTC smooth calibration to backup register DR4.
 *
 *         Encodes plusPulses flag in bit 9 and value in bits [8:0].
 *         Call applyCalibration() afterwards to activate the new value.
 *
 * @param  plusPulses   1 = add pulses (speed up), 0 = remove pulses (slow down)
 * @param  calibValue  Number of pulses per 32s window (0-511)
 */
void setCalibration(uint8_t plusPulses, uint16_t calibValue) {
	uint32_t raw = (calibValue & 0x1FF) | (plusPulses ? 0x200 : 0);
	HAL_RTCEx_BKUPWrite(hrtcHandle, RTC_BKP_CALIB, raw);
}

/**
 * @brief  Apply calibration from backup register to RTC hardware.
 *
 *         Reads calibration from DR4 and calls HAL_RTCEx_SetSmoothCalib()
 *         to program the RTC's smooth calibration registers. Called at
 *         startup and after the user changes calibration from the sub-menu.
 *
 *         HAL_RTCEx_SetSmoothCalib(hrtc, period, plusPulses, value):
 *         - period: RTC_SMOOTHCALIB_PERIOD_32SEC (correction window)
 *         - plusPulses: PLUSPULSES_SET adds (512-value) pulses per window,
 *           PLUSPULSES_RESET subtracts value pulses per window
 *         - value: 0-511 pulses to add/remove
 *
 */
void applyCalibration(void) {
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

/**
 * @brief  Persist silent hours and calibration to Flash (page 31).
 *
 *         Reads current settings from backup registers and writes them as
 *         a single 64-bit double-word to the last Flash page (0x0800F800).
 *
 *         Data layout:
 *           word0 [31:0]:  silentStart[7:0] | silentEnd[15:8] | calibRaw[31:16]
 *           word1 [63:32]: magic number 0xC1F5A001 (validates data on read)
 *
 *         STM32G0 Flash notes:
 *         - Erase unit is one page (2 KB). The full page is erased each time.
 *         - Program unit is one double-word (64 bits, 8 bytes).
 *         - Flash endurance is ~10,000 erase cycles (fine for rare settings changes).
 *         - HAL_FLASH_Unlock/Lock: the Flash control register is write-protected
 *           by default; Unlock removes protection, Lock restores it.
 *
 *         RTOS: taskENTER_CRITICAL() disables interrupts during the Flash
 *         operation (~40ms erase). On single-bank STM32G0 the CPU stalls on
 *         Flash reads during erase anyway, so this is mainly defensive.
 *
 */
void flashWriteSettings(void) {
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

	taskENTER_CRITICAL();

	HAL_FLASH_Unlock();

	eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
	eraseInit.Page = FLASH_SETTINGS_PAGE;
	eraseInit.NbPages = 1;
	HAL_FLASHEx_Erase(&eraseInit, &pageError);

	HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_SETTINGS_ADDR, data);

	HAL_FLASH_Lock();

	taskEXIT_CRITICAL();
}

/**
 * @brief  Restore settings from Flash to backup registers on battery-loss boot.
 *
 *         Called from createRTOS_Tasks() when ICSR.INITS == 0 (RTC lost power).
 *         Reads the double-word at FLASH_SETTINGS_ADDR, validates the magic
 *         number, and writes silent hours and calibration to backup registers.
 *         If the magic doesn't match (Flash never written = fresh device),
 *         writes factory defaults to the backup registers.
 *
 *         Called before the scheduler starts, so no critical section needed.
 *
 */
void flashRestoreSettings(void) {
	uint32_t *addr = (uint32_t *)FLASH_SETTINGS_ADDR;
	uint32_t word0 = addr[0];
	uint32_t word1 = addr[1];

	if (word1 != FLASH_SETTINGS_MAGIC) {
		// Fresh device: write defaults to backup registers
		setSilentHours(SILENT_DEFAULT_START, SILENT_DEFAULT_END);
		setCalibration(0, 0);
		return;
	}

	uint8_t silentStart = (uint8_t)(word0 & 0xFF);
	uint8_t silentEnd = (uint8_t)((word0 >> 8) & 0xFF);
	uint16_t calibRaw = (uint16_t)((word0 >> 16) & 0x3FF);

	if (silentStart <= 23 && silentEnd <= 23) {
		setSilentHours(silentStart, silentEnd);
	}

	uint8_t calibPlus = (calibRaw & 0x200) ? 1 : 0;
	uint16_t calibVal = calibRaw & 0x1FF;
	setCalibration(calibPlus, calibVal);
}


/*
 * ################################
 * #     SILENT MODE HELPERS      #
 * ################################
 */

/**
 * @brief  Check if current RTC time falls within the silent period.
 *
 *         Silent period is defined by start/end hours in backup register DR3.
 *         The check triggers at HH:01 (not HH:00), so the clock can tick to
 *         the exact start hour before going silent.
 *
 *         Handles wrap-around midnight (e.g. 22:01→09:00) and same-day
 *         ranges (e.g. 02:01→05:00).
 *
 *         STM32 RTC note: HAL_RTC_GetTime() MUST be followed by
 *         HAL_RTC_GetDate() — the date read unlocks the time shadow registers.
 *         Skipping GetDate causes GetTime to return stale values.
 *
 * @return 1 if currently in silent period, 0 otherwise
 */
uint8_t isInSilentPeriod(void) {
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
