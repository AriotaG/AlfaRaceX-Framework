#ifndef ARX_STM32F072_TARGET_H
#define ARX_STM32F072_TARGET_H

#include "arx/arx_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_STM32_SYSCLK_HZ              48000000u
#define ARX_STM32_INTERCHIP_BAUD         38400u
#define ARX_STM32_PEDAL_BAUD             9600u
#define ARX_STM32_CONFIG_SYNC_DELAY_MS    3502u

#define ARX_STM32_CAN_BS1_TQ             4u
#define ARX_STM32_CAN_BS2_TQ             3u
#define ARX_STM32_CAN_SJW_TQ             1u
#define ARX_STM32_CAN_PRESCALER_500K     12u
#define ARX_STM32_CAN_PRESCALER_125K     48u

#define ARX_STM32_LED_COUNT              46u
#define ARX_STM32_WS2812_TIMER_HZ        48000000u
#define ARX_STM32_WS2812_PERIOD_TICKS    60u
#define ARX_STM32_WS2812_ZERO_TICKS      17u
#define ARX_STM32_WS2812_ONE_TICKS       34u
#define ARX_STM32_WS2812_RESET_SLOTS     50u

#define ARX_STM32_FLASH_PAGE_BYTES       2048u
#define ARX_STM32_SETTINGS_PAGE_ADDRESS  0x0801F800u
#define ARX_STM32_STATS_PAGE_ADDRESS     0x0801F000u
#define ARX_STM32_PARAM_PAGE_ADDRESS     0x0801E800u
#define ARX_STM32_LOG_PAGE_ADDRESS       0x0801E000u

typedef enum {
    ARX_TARGET_ROLE_C1 = 0,
    ARX_TARGET_ROLE_C2,
    ARX_TARGET_ROLE_BH
} ArxTargetRole;

typedef struct {
    const char *name;
    ArxTargetRole role;
    ArxBus bus;
    uint32_t can_bitrate;
    uint16_t can_prescaler;
    bool pedal_uart_present;
    bool controls_slave_reset;
    bool controls_slave_can_sleep;
} ArxStm32TargetProfile;

const ArxStm32TargetProfile *arx_stm32_profile(ArxTargetRole role);
bool arx_stm32_storage_address_valid(uint32_t address, bool erase_page);

#if defined(ARX_TARGET_STM32F072)
void Error_Handler(void);
void arx_stm32f072_platform_init(ArxTargetRole role);
void arx_stm32f072_led_init(void);
void arx_stm32f072_led_suspend(void);
void arx_stm32f072_led_dma_irq(void);
void arx_stm32_slave_reset(bool asserted);
void arx_stm32_slave_can_sleep(bool sleep);
bool arx_target_led_dma_start(const uint16_t *pwm, size_t count);
#endif

#endif
