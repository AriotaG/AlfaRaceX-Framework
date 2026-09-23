#include "arx_stm32f072_log.h"

#if defined(ARX_TARGET_STM32F072)
#include "arx/arx_config.h"
#include "arx_stm32f072_storage.h"
#include "arx_stm32f072_target.h"
#include <stddef.h>
#include <string.h>

#define ARX_TARGET_LOG_MAGIC   0x4152584Cu /* ARXL */
#define ARX_TARGET_LOG_VERSION 1u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t role;
    uint8_t reserved;
    uint32_t timestamp_ms;
    uint32_t captured;
    uint32_t dropped_total;
    uint16_t head;
    uint16_t tail;
    uint16_t count;
    uint16_t dropped_pending;
    uint8_t ring[ARX_SNIFFER_BUFFER_SIZE];
    uint32_t crc32;
} ArxTargetLogRecord;

_Static_assert(sizeof(ArxTargetLogRecord)<ARX_STM32_FLASH_PAGE_BYTES,"log record must fit one flash page");

bool arx_stm32f072_log_save(const ArxRuntime *rt,uint32_t now_ms){
    if(!rt)return false;
    static ArxTargetLogRecord record;
    memset(&record,0,sizeof(record));
    record.magic=ARX_TARGET_LOG_MAGIC;
    record.version=ARX_TARGET_LOG_VERSION;
    record.role=(uint8_t)rt->role;
    record.timestamp_ms=now_ms;
    record.captured=rt->sniffer.captured;
    record.dropped_total=rt->sniffer.dropped_total;
    record.head=rt->sniffer.head;
    record.tail=rt->sniffer.tail;
    record.count=rt->sniffer.count;
    record.dropped_pending=rt->sniffer.dropped_pending;
    memcpy(record.ring,rt->sniffer.ring,sizeof(record.ring));
    record.crc32=arx_crc32(&record,(uint32_t)offsetof(ArxTargetLogRecord,crc32));

    ArxStorageBackend b=arx_stm32f072_storage_backend();
    if(!b.erase_page||!b.program_halfword)return false;
    if(!b.erase_page(ARX_STM32_LOG_PAGE_ADDRESS,b.user))return false;

    const uint8_t *raw=(const uint8_t*)&record;
    for(size_t i=0u;i<sizeof(record);i+=2u){
        uint16_t hw=raw[i];
        if(i+1u<sizeof(record))hw|=(uint16_t)((uint16_t)raw[i+1u]<<8u);
        if(!b.program_halfword(ARX_STM32_LOG_PAGE_ADDRESS+(uint32_t)i,hw,b.user))return false;
    }
    return true;
}
#endif
