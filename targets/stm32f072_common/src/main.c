#include "arx_stm32f072_app.h"

#if defined(ARX_TARGET_STM32F072)

#ifndef ARX_IMAGE_ROLE
#error "Define ARX_IMAGE_ROLE as ARX_TARGET_ROLE_C1, ARX_TARGET_ROLE_C2 or ARX_TARGET_ROLE_BH"
#endif

int main(void) {
    arx_stm32f072_app_init((ArxTargetRole)ARX_IMAGE_ROLE);
    for(;;) arx_stm32f072_app_loop();
}

#endif
