#ifndef ARX_STM32F072_STORAGE_H
#define ARX_STM32F072_STORAGE_H

#include "arx/arx_storage.h"

#if defined(ARX_TARGET_STM32F072)
ArxStorageBackend arx_stm32f072_storage_backend(void);
#endif

#endif
