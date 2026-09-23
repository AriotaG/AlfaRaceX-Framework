#ifndef ARX_STM32F072_LOG_H
#define ARX_STM32F072_LOG_H

#include "arx/arx_runtime.h"
#include <stdbool.h>
#include <stdint.h>

#if defined(ARX_TARGET_STM32F072)
bool arx_stm32f072_log_save(const ArxRuntime *rt,uint32_t now_ms);
#endif

#endif
