#include "arx/features/arx_dynamic_shift.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    ArxDynamicShift feature;
    arx_dynamic_shift_init(&feature);
    feature.enabled=true;

    assert(feature.base_threshold_rpm==4500u);
    assert(arx_dynamic_shift_calculate(&feature,4499u)==0u);
    assert(arx_dynamic_shift_calculate(&feature,4500u)==1u);
    assert(arx_dynamic_shift_calculate(&feature,5000u)==2u);
    assert(arx_dynamic_shift_calculate(&feature,5500u)==3u);

    ArxCanFrame source={
        .bus=ARX_BUS_C1,.id=0x2EDu,.extended_id=false,.dlc=8u,
        .data={0,0,0,0,0,0,0xA0,0}
    };
    ArxCanFrame out;

    assert(arx_dynamic_shift_build_frame(&feature,&source,5100u,&out));
    assert((out.data[6]&0x03u)==2u);
    assert((out.data[6]&0xFCu)==(source.data[6]&0xFCu));

    feature.ipc_my23=true;
    assert(arx_dynamic_shift_build_frame(&feature,&source,5100u,&out));
    assert((out.data[1]&0x60u)==0x40u);

    puts("dynamic shift tests: OK");
    return 0;
}
