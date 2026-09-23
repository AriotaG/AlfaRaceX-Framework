#include "arx/features/arx_dynamic_shift.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    ArxDynamicShift s;
    arx_dynamic_shift_init(&s);
    s.enabled=true;

    assert(s.base_threshold_rpm==4500u);
    assert(s.level2_offset_rpm==500u);
    assert(s.level3_offset_rpm==1000u);

    assert(arx_dynamic_shift_calculate(&s,4499u)==0u);
    assert(arx_dynamic_shift_calculate(&s,4500u)==1u);
    assert(arx_dynamic_shift_calculate(&s,5000u)==2u);
    assert(arx_dynamic_shift_calculate(&s,5500u)==3u);

    ArxCanFrame src={
        .bus=ARX_BUS_C1,.id=0x2ED,.dlc=8,
        .data={0,0,0,0,0,0,0xA0,0}
    };
    ArxCanFrame out;

    s.ipc_my23=true;
    assert(arx_dynamic_shift_build_frame(&s,&src,5000u,&out));
    assert((out.data[1]&0x60u)==0x40u);
    assert((out.data[6]&0x03u)==2u);
    assert((out.data[6]&0xFCu)==(src.data[6]&0xFCu));

    puts("shift exact tests: OK");
    return 0;
}
