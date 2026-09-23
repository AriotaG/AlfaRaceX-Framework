#include "arx/arx_runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rng_state=0xA17FA5E1u;
static uint32_t rnd(void){
    uint32_t x=rng_state;
    x^=x<<13; x^=x>>17; x^=x<<5;
    rng_state=x;
    return x;
}

static ArxCanFrame random_frame(ArxBus bus,uint32_t tick){
    ArxCanFrame f;
    memset(&f,0,sizeof(f));
    f.bus=bus;
    f.extended_id=(rnd()&3u)==0u;
    f.id=f.extended_id?(rnd()&0x1FFFFFFFu):(rnd()&0x7FFu);
    f.dlc=(uint8_t)(rnd()%9u);
    f.timestamp_ms=tick;
    for(uint8_t i=0;i<8u;i++)f.data[i]=(uint8_t)rnd();
    return f;
}

static void exercise_role(ArxRuntimeRole role,ArxBus bus){
    ArxRuntime rt;
    arx_runtime_init(&rt,role,0);

    for(uint32_t i=1u;i<=30000u;i++){
        ArxCanFrame f=random_frame(bus,i);
        arx_runtime_on_can(&rt,&f,i);

        if((i%7u)==0u){
            uint8_t wire[ARX_INTERCHIP_FRAME_SIZE];
            for(size_t j=0;j<sizeof(wire);j++)wire[j]=(uint8_t)rnd();
            arx_runtime_on_interchip(&rt,wire,i);
        }
        if((i%3u)==0u)arx_runtime_tick(&rt,i);
    }

    assert(rt.rx_frames==30000u);
    assert(rt.generated_frames<200000u);
    for(unsigned q=0u;q<ARX_PRIORITY_COUNT;q++)
        assert(rt.can_tx.queues[q].count<=ARX_CAN_QUEUE_CAPACITY);
    assert(rt.interchip.tx.count<=ARX_INTERCHIP_QUEUE_SIZE);
}

int main(void){
    exercise_role(ARX_RUNTIME_C1,ARX_BUS_C1);
    exercise_role(ARX_RUNTIME_C2,ARX_BUS_C2);
    exercise_role(ARX_RUNTIME_BH,ARX_BUS_BH);
    puts("runtime stress: OK");
    return 0;
}
