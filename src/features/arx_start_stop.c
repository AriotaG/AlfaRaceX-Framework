#include "arx/features/arx_start_stop.h"
#include <string.h>

void arx_start_stop_init(ArxStartStop *f) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->vehicle_start_stop_enabled = true;
    f->boot_grace_ms = 10000u;
    f->engine_grace_ms = 5000u;
}

void arx_start_stop_on_engine_rpm(ArxStartStop *f, uint16_t rpm, uint32_t now_ms) {
    if (!f) return;

    if (rpm < 400u) {
        f->operation_done_this_engine_cycle = false;
        f->engine_running_since_ms = 0u;
        return;
    }

    if (f->engine_running_since_ms == 0u) {
        f->engine_running_since_ms = now_ms;
    }
}

bool arx_start_stop_observe_status_226(ArxStartStop *f, const ArxCanFrame *frame) {
    if (!f || !frame || frame->extended_id || frame->id != 0x226u || frame->dlc < 3u) {
        return false;
    }

    f->vehicle_start_stop_enabled = (((frame->data[2] >> 2u) & 0x03u) != 0x01u);
    return true;
}

bool arx_start_stop_should_toggle(const ArxStartStop *f, uint32_t now_ms) {
    if (!f || !f->enabled || f->operation_done_this_engine_cycle) return false;
    if (!f->vehicle_start_stop_enabled) return false;
    if (now_ms <= f->boot_grace_ms || f->engine_running_since_ms == 0u) return false;
    return (now_ms - f->engine_running_since_ms) >= f->engine_grace_ms;
}

bool arx_start_stop_build_toggle(const ArxCanFrame *source_4b1, ArxCanFrame *out) {
    if (!source_4b1 || !out || source_4b1->extended_id ||
        source_4b1->id != 0x4B1u || source_4b1->dlc < 6u) return false;

    *out = *source_4b1;
    out->data[5] = (uint8_t)((out->data[5] & 0xC7u) | 0x08u);
    return true;
}

void arx_start_stop_mark_toggled(ArxStartStop *f) {
    if (f) f->operation_done_this_engine_cycle = true;
}
