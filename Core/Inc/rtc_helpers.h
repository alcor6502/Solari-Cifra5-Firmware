/**
 * @file   rtc_helpers.h
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

#ifndef _RTC_HELPERS_H_
#define _RTC_HELPERS_H_

#include "rtos_init.h"

/*  
 *   RTC Helpers Structure and definitions
 */

// Silent mode configuration (defaults, actual values stored in backup registers)
#define SILENT_DEFAULT_START	22		// Default hour when silent period starts
#define SILENT_DEFAULT_END		9		// Default hour when silent period ends

// RTC Backup Register allocation
#define RTC_BKP_MECH_HOURS		RTC_BKP_DR0  // Mechanical clock hours (0-23)
#define RTC_BKP_MECH_MINUTES	RTC_BKP_DR1  // Mechanical clock minutes (0-59)
#define RTC_BKP_FLAGS			RTC_BKP_DR2  // Last tick state + other flags

// Backup register flag bits
#define RTC_BKP_FLAG_LAST_TICK	0x00000001  // Bit 0: last tick state (0=tick, 1=tock)

// Silent hours backup register (start in bits 7:0, end in bits 15:8)
#define RTC_BKP_SILENT			RTC_BKP_DR3

// Calibration backup register (bit 9: plus flag, bits 0-8: value 0-511)
#define RTC_BKP_CALIB			RTC_BKP_DR4

// Flash settings storage (last page of 64K Flash)
#define FLASH_SETTINGS_PAGE		31				// Last Flash page (page 31)
#define FLASH_SETTINGS_ADDR		0x0800F800U		// Page 31 base address
#define FLASH_SETTINGS_MAGIC	0xC1F5A001U		// Validation magic number

/* Mechanical position (backup registers DR0/DR1) */
uint8_t getMechHours(void);
uint8_t getMechMinutes(void);
void incrementMechMinute(void);
void incrementMechHour(void);
void resetMechPosition(void);

/* Tick/tock alternation (backup register DR2 bit 0) */
uint8_t getLastTick(void);
void setLastTick(uint8_t tickState);

/* Silent period hours (backup register DR3, packed) */
uint8_t getSilentStartHour(void);
uint8_t getSilentEndHour(void);
void setSilentHours(uint8_t startHour, uint8_t endHour);

/* RTC smooth calibration (backup register DR4) */
void getCalibration(uint8_t *plusPulses, uint16_t *calibValue);
void setCalibration(uint8_t plusPulses, uint16_t calibValue);
void applyCalibration(void);

/* Flash settings persistence (page 31) */
void flashWriteSettings(void);
void flashRestoreSettings(void);

/* Silent period check */
uint8_t isInSilentPeriod(void);

#endif /* _RTC_HELPERS_H_ */
