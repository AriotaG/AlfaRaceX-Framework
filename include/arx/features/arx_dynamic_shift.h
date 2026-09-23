#ifndef ARX_DYNAMIC_SHIFT_H
#define ARX_DYNAMIC_SHIFT_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool ipc_my23;

    uint16_t base_threshold_rpm;
    uint16_t level2_offset_rpm;
    uint16_t level3_offset_rpm;

    uint8_t current_urgency;
    uint32_t generated_frames;
} ArxDynamicShift;

void arx_dynamic_shift_init(ArxDynamicShift *feature);
uint8_t arx_dynamic_shift_calculate(const ArxDynamicShift *feature, uint16_t rpm);

bool arx_dynamic_shift_build_frame(
    ArxDynamicShift *feature,
    const ArxCanFrame *source_2ed,
    uint16_t rpm,
    ArxCanFrame *out
);

#endif
