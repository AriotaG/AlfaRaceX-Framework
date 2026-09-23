#ifndef ARX_START_STOP_H
#define ARX_START_STOP_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool operation_done_this_engine_cycle;
    bool vehicle_start_stop_enabled;

    uint32_t boot_grace_ms;
    uint32_t engine_grace_ms;
    uint32_t engine_running_since_ms;
} ArxStartStop;

void arx_start_stop_init(ArxStartStop *f);
void arx_start_stop_on_engine_rpm(ArxStartStop *f, uint16_t rpm, uint32_t now_ms);
bool arx_start_stop_observe_status_226(ArxStartStop *f, const ArxCanFrame *frame);
bool arx_start_stop_should_toggle(const ArxStartStop *f, uint32_t now_ms);
bool arx_start_stop_build_toggle(const ArxCanFrame *source_4b1, ArxCanFrame *out);
void arx_start_stop_mark_toggled(ArxStartStop *f);

#endif
