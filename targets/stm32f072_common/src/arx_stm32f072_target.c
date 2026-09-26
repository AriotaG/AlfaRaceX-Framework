#include "arx_stm32f072_target.h"

static const ArxStm32TargetProfile profiles[] = {
    {
        .name = "ARX-C1",
        .role = ARX_TARGET_ROLE_C1,
        .bus = ARX_BUS_C1,
        .can_bitrate = 500000u,
        .can_prescaler = ARX_STM32_CAN_PRESCALER_500K,
        .pedal_uart_present = true,
        .controls_slave_reset = true,
        .controls_slave_can_sleep = true
    },
    {
        .name = "ARX-C2",
        .role = ARX_TARGET_ROLE_C2,
        .bus = ARX_BUS_C2,
        .can_bitrate = 500000u,
        .can_prescaler = ARX_STM32_CAN_PRESCALER_500K,
        .pedal_uart_present = false,
        .controls_slave_reset = false,
        .controls_slave_can_sleep = false
    },
    {
        .name = "ARX-BH",
        .role = ARX_TARGET_ROLE_BH,
        .bus = ARX_BUS_BH,
        .can_bitrate = 125000u,
        .can_prescaler = ARX_STM32_CAN_PRESCALER_125K,
        .pedal_uart_present = false,
        .controls_slave_reset = false,
        .controls_slave_can_sleep = false
    }
};

const ArxStm32TargetProfile *arx_stm32_profile(ArxTargetRole role) {
    if ((unsigned)role >= (sizeof(profiles) / sizeof(profiles[0]))) return 0;
    return &profiles[(unsigned)role];
}

bool arx_stm32_storage_address_valid(uint32_t address,bool erase_page) {
    /* Software map boundary only: this does not establish physical Flash capacity. */
    if(address<ARX_STM32_LOG_PAGE_ADDRESS||address>=0x08020000u)return false;
    return erase_page ? (address%ARX_STM32_FLASH_PAGE_BYTES)==0u : (address%2u)==0u;
}
