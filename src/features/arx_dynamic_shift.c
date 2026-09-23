#include "arx/features/arx_dynamic_shift.h"
#include <string.h>

void arx_dynamic_shift_init(ArxDynamicShift *feature) {
    if (!feature) return;
    memset(feature,0,sizeof(*feature));
    feature->base_threshold_rpm=4500u;
    feature->level2_offset_rpm=500u;
    feature->level3_offset_rpm=1000u;
}

uint8_t arx_dynamic_shift_calculate(const ArxDynamicShift *feature, uint16_t rpm) {
    if (!feature || !feature->enabled) return 0u;

    const uint32_t t1=feature->base_threshold_rpm;
    const uint32_t t2=t1+feature->level2_offset_rpm;
    const uint32_t t3=t1+feature->level3_offset_rpm;

    if (rpm<t1) return 0u;
    if (rpm<t2) return 1u;
    if (rpm<t3) return 2u;
    return 3u;
}

bool arx_dynamic_shift_build_frame(
    ArxDynamicShift *feature,
    const ArxCanFrame *source_2ed,
    uint16_t rpm,
    ArxCanFrame *out
) {
    if (!feature || !source_2ed || !out ||
        source_2ed->extended_id || source_2ed->bus!=ARX_BUS_C1 ||
        source_2ed->id!=0x2EDu || source_2ed->dlc!=8u) return false;

    const uint8_t urgency=arx_dynamic_shift_calculate(feature,rpm);
    if (!urgency) {
        feature->current_urgency=0u;
        return false;
    }

    *out=*source_2ed;

    if (feature->ipc_my23) {
        out->data[1]=(uint8_t)((out->data[1] & (uint8_t)~0x60u) | 0x40u);
    }

    out->data[6]=(uint8_t)((out->data[6]&0xFCu)|urgency);
    feature->current_urgency=urgency;
    feature->generated_frames++;
    return true;
}
