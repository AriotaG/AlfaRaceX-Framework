#include "arx/arx_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define BASE 0x0801E800u
#define SIZE 0x1800u

typedef struct {
    uint8_t mem[SIZE];
} Mock;

static size_t off(uint32_t addr) {
    assert(addr>=BASE && addr+1u<BASE+SIZE);
    return (size_t)(addr-BASE);
}
static uint16_t rd(uint32_t addr,void *u) {
    Mock *m=(Mock*)u; size_t o=off(addr);
    return (uint16_t)(m->mem[o]|((uint16_t)m->mem[o+1u]<<8u));
}
static bool erase(uint32_t addr,void *u) {
    Mock *m=(Mock*)u; size_t o=off(addr);
    assert((o%ARX_STORAGE_PAGE_BYTES)==0u);
    memset(&m->mem[o],0xFF,ARX_STORAGE_PAGE_BYTES);
    return true;
}
static bool wr(uint32_t addr,uint16_t v,void *u) {
    Mock *m=(Mock*)u; size_t o=off(addr);
    m->mem[o]=(uint8_t)v; m->mem[o+1u]=(uint8_t)(v>>8u); return true;
}

int main(void) {
    Mock m; memset(&m,0xFF,sizeof(m));
    ArxStorageBackend b={rd,erase,wr,&m};

    ArxRuntimeConfig erased_cfg;
    assert(arx_storage_read_settings(&b,&erased_cfg));
    assert(erased_cfg.immobilizer_enabled);
    assert(erased_cfg.smart_start_stop_enabled);
    assert(erased_cfg.clear_faults_enabled);
    assert(erased_cfg.diesel_profile);
    assert(!erased_cfg.ipc_my23);
    assert(!erased_cfg.dyno_enabled);

    ArxRuntimeConfig c;
    arx_config_defaults(&c);
    c.shift_indicator_enabled=true;
    c.shift_threshold_rpm=4200u;
    c.pedal_mode=ARX_CFG_PEDAL_DYNAMIC;
    c.pedal_power=-3;
    c.front_park_mute_enabled=true;
    c.elm327_enabled=true;

    assert(arx_storage_write_settings(&b,&c));
    assert(rd(ARX_STORAGE_SETTINGS_ADDR+3u*4u,&m)==1u);
    assert(rd(ARX_STORAGE_SETTINGS_ADDR+4u*4u,&m)==4200u);
    assert(rd(ARX_STORAGE_SETTINGS_ADDR+19u*4u,&m)==ARX_CFG_PEDAL_DYNAMIC);
    assert((uint8_t)rd(ARX_STORAGE_SETTINGS_ADDR+28u*4u,&m)==(uint8_t)-3);

    ArxRuntimeConfig loaded;
    assert(arx_storage_read_settings(&b,&loaded));
    assert(loaded.shift_indicator_enabled);
    assert(loaded.shift_threshold_rpm==4200u);
    assert(loaded.pedal_mode==ARX_CFG_PEDAL_DYNAMIC);
    assert(loaded.pedal_power==-3);
    assert(loaded.elm327_enabled);

    uint8_t vis[40]={0};
    vis[0]=1;vis[15]=1;vis[16]=1;vis[39]=1;
    assert(arx_storage_write_visible(&b,vis,40));
    uint8_t got[40]={0};
    assert(arx_storage_read_visible(&b,got,40));
    assert(got[0]&&got[15]&&got[16]&&got[39]);

    assert(arx_storage_write_best_seconds(&b,6.321f,17.456f));
    float a=arx_storage_read_best_seconds(&b,1);
    float z=arx_storage_read_best_seconds(&b,2);
    assert(a>6.320f&&a<6.322f);
    assert(z>17.455f&&z<17.457f);

    puts("storage tests: OK");
    return 0;
}
