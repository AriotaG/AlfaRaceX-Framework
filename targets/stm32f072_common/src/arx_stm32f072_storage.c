#include "arx_stm32f072_storage.h"

#if defined(ARX_TARGET_STM32F072)
#include "stm32f0xx_hal.h"
#include "arx_stm32f072_target.h"

static uint16_t stm_read16(uint32_t address,void *user) {
    (void)user;
    if(!arx_stm32_storage_address_valid(address,false))return 0xFFFFu;
    return *(volatile const uint16_t *)address;
}

static bool stm_erase(uint32_t address,void *user) {
    (void)user;
    if(!arx_stm32_storage_address_valid(address,true))return false;
    FLASH_EraseInitTypeDef erase={0};
    uint32_t page_error=0u;
    erase.TypeErase=FLASH_TYPEERASE_PAGES;
    erase.PageAddress=address;
    erase.NbPages=1u;

    if(HAL_FLASH_Unlock()!=HAL_OK)return false;
    HAL_StatusTypeDef s=HAL_FLASHEx_Erase(&erase,&page_error);
    const HAL_StatusTypeDef locked=HAL_FLASH_Lock();
    if(s!=HAL_OK||locked!=HAL_OK)return false;
    for(uint32_t offset=0;offset<ARX_STM32_FLASH_PAGE_BYTES;offset+=2u)
        if(stm_read16(address+offset,0)!=0xFFFFu)return false;
    return true;
}

static bool stm_program16(uint32_t address,uint16_t value,void *user) {
    (void)user;
    if(!arx_stm32_storage_address_valid(address,false))return false;
    if(HAL_FLASH_Unlock()!=HAL_OK)return false;
    HAL_StatusTypeDef s=HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,address,value);
    const HAL_StatusTypeDef locked=HAL_FLASH_Lock();
    return s==HAL_OK&&locked==HAL_OK&&stm_read16(address,0)==value;
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
