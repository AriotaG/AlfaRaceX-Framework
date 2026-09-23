#ifndef STM32F0XX_HAL_CONF_H
#define STM32F0XX_HAL_CONF_H

#define HAL_MODULE_ENABLED
#define HAL_CAN_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

#ifndef HSE_VALUE
#define HSE_VALUE 8000000U
#endif
#ifndef HSE_STARTUP_TIMEOUT
#define HSE_STARTUP_TIMEOUT 100U
#endif
#ifndef HSI_VALUE
#define HSI_VALUE 8000000U
#endif
#ifndef HSI_STARTUP_TIMEOUT
#define HSI_STARTUP_TIMEOUT 5000U
#endif
#ifndef HSI14_VALUE
#define HSI14_VALUE 14000000U
#endif
#ifndef HSI48_VALUE
#define HSI48_VALUE 48000000U
#endif
#ifndef LSI_VALUE
#define LSI_VALUE 32000U
#endif
#ifndef LSE_VALUE
#define LSE_VALUE 32768U
#endif
#ifndef LSE_STARTUP_TIMEOUT
#define LSE_STARTUP_TIMEOUT 5000U
#endif

#define VDD_VALUE 3300U
#define TICK_INT_PRIORITY ((uint32_t)(1U<<__NVIC_PRIO_BITS)-1U)
#define USE_RTOS 0U
#define PREFETCH_ENABLE 1U
#define INSTRUCTION_CACHE_ENABLE 0U
#define DATA_CACHE_ENABLE 0U

#define USE_HAL_CAN_REGISTER_CALLBACKS 0U
#define USE_HAL_UART_REGISTER_CALLBACKS 0U
#define USE_HAL_TIM_REGISTER_CALLBACKS 0U
#define USE_HAL_PCD_REGISTER_CALLBACKS 0U

#include "stm32f0xx_hal_rcc.h"
#include "stm32f0xx_hal_gpio.h"
#include "stm32f0xx_hal_dma.h"
#include "stm32f0xx_hal_cortex.h"
#include "stm32f0xx_hal_can.h"
#include "stm32f0xx_hal_flash.h"
#include "stm32f0xx_hal_pcd.h"
#include "stm32f0xx_hal_pwr.h"
#include "stm32f0xx_hal_tim.h"
#include "stm32f0xx_hal_uart.h"

#ifndef assert_param
#define assert_param(expr) ((void)0U)
#endif

#endif
