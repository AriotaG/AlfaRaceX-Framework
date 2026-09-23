#include "arx_stm32f072_storage.h"

#if defined(ARX_TARGET_STM32F072)
#include "stm32f0xx_hal.h"

static uint16_t stm_read16(uint32_t address,void *user) {
    (void)user;
    return *(volatile const uint16_t *)address;
}

static bool stm_erase(uint32_t address,void *user) {
    (void)user;
    FLASH_EraseInitTypeDef erase={0};
    uint32_t page_error=0u;
    erase.TypeErase=FLASH_TYPEERASE_PAGES;
    erase.PageAddress=address;
    erase.NbPages=1u;

    HAL_FLASH_Unlock();
    HAL_StatusTypeDef s=HAL_FLASHEx_Erase(&erase,&page_error);
    HAL_FLASH_Lock();
    return s==HAL_OK;
}

static bool stm_program16(uint32_t address,uint16_t value,void *user) {
    (void)user;
    HAL_FLASH_Unlock();
    HAL_StatusTypeDef s=HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,address,value);
    HAL_FLASH_Lock();
    return s==HAL_OK;
}

ArxStorageBackend arx_stm32f072_storage_backend(void) {
    ArxStorageBackend b={
        .read_halfword=stm_read16,
        .erase_page=stm_erase,
        .program_halfword=stm_program16,
        .user=0
    };
    return b;
}
#endif
