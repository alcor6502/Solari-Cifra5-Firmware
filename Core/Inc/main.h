/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define PW_MON_Pin GPIO_PIN_1
#define PW_MON_GPIO_Port GPIOA
#define PW_MON_EXTI_IRQn EXTI0_1_IRQn
#define RTC_CAL_Pin GPIO_PIN_4
#define RTC_CAL_GPIO_Port GPIOA
#define CLK_TICK_Pin GPIO_PIN_6
#define CLK_TICK_GPIO_Port GPIOA
#define CLK_TOCK_Pin GPIO_PIN_7
#define CLK_TOCK_GPIO_Port GPIOA
#define SNS_HOUR_Pin GPIO_PIN_8
#define SNS_HOUR_GPIO_Port GPIOA
#define SNS_DAY_Pin GPIO_PIN_9
#define SNS_DAY_GPIO_Port GPIOA
#define LED_FAULT_Pin GPIO_PIN_6
#define LED_FAULT_GPIO_Port GPIOC
#define SERVO_PWM_Pin GPIO_PIN_11
#define SERVO_PWM_GPIO_Port GPIOA
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA
#define BTN_DEC_Pin GPIO_PIN_3
#define BTN_DEC_GPIO_Port GPIOB
#define BTN_INC_Pin GPIO_PIN_4
#define BTN_INC_GPIO_Port GPIOB
#define BTN_SET_Pin GPIO_PIN_5
#define BTN_SET_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

//#define CIFRA5_DEBUG
#define ERROR_HANDLER_FLASH_DELAY	200000UL

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
