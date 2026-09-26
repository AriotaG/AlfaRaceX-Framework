#include "arx_stm32f072_target.h"
#include "arx/arx_interchip.h"
#include "arx/arx_power.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    int uart_pause;
    int uart_resume;
    int reset_assert;
    int reset_release;
    int sleep_on;
    int sleep_off;
    int reconfigure;
} Calls;

static void pause_uart(void *u){ ((Calls*)u)->uart_pause++; }
static void resume_uart(void *u){ ((Calls*)u)->uart_resume++; }
static void reset_set(bool a, void *u){ if(a)((Calls*)u)->reset_assert++; else ((Calls*)u)->reset_release++; }
static void sleep_set(bool a, void *u){ if(a)((Calls*)u)->sleep_on++; else ((Calls*)u)->sleep_off++; }
static void reconfigure(void *u){ ((Calls*)u)->reconfigure++; }

int main(void) {
    const ArxStm32TargetProfile *c1=arx_stm32_profile(ARX_TARGET_ROLE_C1);
    const ArxStm32TargetProfile *c2=arx_stm32_profile(ARX_TARGET_ROLE_C2);
    const ArxStm32TargetProfile *bh=arx_stm32_profile(ARX_TARGET_ROLE_BH);

    assert(c1 && c2 && bh);
    assert(!arx_stm32_storage_address_valid(0x08000000u,true));
    assert(!arx_stm32_storage_address_valid(0x0801DFFFu,false));
    assert(!arx_stm32_storage_address_valid(0x08020000u,false));
    assert(!arx_stm32_storage_address_valid(UINT32_MAX,false));
    assert(arx_stm32_storage_address_valid(ARX_STM32_LOG_PAGE_ADDRESS,true));
    assert(arx_stm32_storage_address_valid(ARX_STM32_SETTINGS_PAGE_ADDRESS,true));
    assert(!arx_stm32_storage_address_valid(ARX_STM32_SETTINGS_PAGE_ADDRESS+2u,true));
    assert(!arx_stm32_storage_address_valid(ARX_STM32_SETTINGS_PAGE_ADDRESS+1u,false));
    assert(arx_stm32_storage_address_valid(0x0801FFFEu,false));
    assert(c1->can_bitrate==500000u && c1->can_prescaler==12u);
    assert(c2->can_bitrate==500000u && c2->can_prescaler==12u);
    assert(bh->can_bitrate==125000u && bh->can_prescaler==48u);
    assert(c1->pedal_uart_present && !c2->pedal_uart_present && !bh->pedal_uart_present);
    assert(ARX_STM32_SYSCLK_HZ==48000000u);
    assert(ARX_STM32_INTERCHIP_BAUD==38400u);
    assert(ARX_STM32_PEDAL_BAUD==9600u);
    assert(ARX_STM32_CONFIG_SYNC_DELAY_MS==3502u);

    assert(!arx_interchip_start_byte_valid(0x00u));
    assert(arx_interchip_start_byte_valid(ARX_IC_TO_C1));
    assert(arx_interchip_start_byte_valid(ARX_IC_TO_C1_C2));
    assert(arx_interchip_start_byte_valid(0x0Eu));
    assert(arx_interchip_start_byte_valid(0x0Fu));
    assert(arx_interchip_start_byte_valid(0x10u));
    assert(!arx_interchip_start_byte_valid(0x11u));

    ArxInterchipFrame f;
    const uint8_t p[]={ARX_IC_TO_C2,ARX_IC_C2_GET_STATUS};
    arx_interchip_frame_build(&f,p,sizeof(p));
    assert(f.bytes[0]==ARX_IC_TO_C2);
    assert(f.bytes[1]==ARX_IC_C2_GET_STATUS);
    for(unsigned i=2;i<ARX_INTERCHIP_FRAME_SIZE;i++) assert(f.bytes[i]==ARX_INTERCHIP_PAD);

    ArxInterchip ic;
    arx_interchip_init(&ic,ARX_IC_ROLE_C1);
    assert(!arx_interchip_tx_allowed(&ic,1000u));
    assert(arx_interchip_tx_allowed(&ic,2251u));

    ArxInterchip slave;
    arx_interchip_init(&slave,ARX_IC_ROLE_C2);
    arx_interchip_note_master_request(&slave,3000u);
    assert(arx_interchip_tx_allowed(&slave,3100u));
    assert(!arx_interchip_tx_allowed(&slave,3200u));

    ArxPowerManager pm;
    arx_power_init(&pm);
    pm.enabled=true;
    assert(pm.sleep_after_ms==3500u);
    assert(pm.wake_activity_window_ms==3400u);
    arx_power_note_can_rx(&pm,100u);

    Calls calls={0};
    ArxPowerOps ops={
        .interchip_uart_pause=pause_uart,
        .interchip_uart_resume=resume_uart,
        .slave_reset_set=reset_set,
        .slave_can_sleep_set=sleep_set,
        .on_wake_reconfigure=reconfigure,
        .user=&calls
    };
    ArxPowerBlocks b={0};
    assert(arx_power_enter_low_consume(&pm,b,3601u,&ops));
    assert(pm.state==ARX_POWER_LOW_CONSUME);
    assert(calls.uart_pause==1 && calls.reset_assert==1 && calls.sleep_on==1);

    arx_power_note_can_rx(&pm,3602u);
    assert(arx_power_wake_if_needed(&pm,3603u,&ops));
    assert(pm.state==ARX_POWER_AWAKE);
    assert(calls.uart_resume==1 && calls.reset_release==1 &&
           calls.sleep_off==1 && calls.reconfigure==1);

    puts("target contract tests: OK");
    return 0;
}
