#ifndef ARX_STM32F072_APP_H
#define ARX_STM32F072_APP_H

#include "arx/arx_runtime.h"
#include "arx_stm32f072_target.h"
#include <stdint.h>

#if defined(ARX_TARGET_STM32F072)

void arx_stm32f072_app_init(ArxTargetRole role);
void arx_stm32f072_app_loop(void);

ArxRuntime *arx_stm32f072_runtime(void);

/* Called by the USART2 19-byte receive completion path. */
void arx_stm32f072_interchip_rx(
    const uint8_t raw[ARX_INTERCHIP_FRAME_SIZE]
);

/* Called by the USART1 pedal receive path on C1. */
void arx_stm32f072_pedal_rx(uint8_t reply_byte);

#endif
#endif
