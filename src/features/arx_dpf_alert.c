#include "arx/features/arx_dpf_alert.h"
#include <string.h>

void arx_dpf_alert_init(ArxDpfAlert *f) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->visual_alert_enabled = true;
    f->sound_alert_enabled = true;
}

bool arx_dpf_alert_on_5ae(ArxDpfAlert *f, const ArxCanFrame *frame, bool *started, bool *ended) {
    if (!f || !frame || frame->extended_id || frame->id != 0x5AEu || frame->dlc < 6u) {
        return false;
    }

    if (started) *started = false;
    if (ended) *ended = false;

    const uint8_t mode = (uint8_t)((frame->data[5] >> 2u) & 0x07u);
    f->last_regen_mode = mode;

    if (!f->enabled) return true;

    if (mode == 2u && !f->regeneration_active) {
        f->regeneration_active = true;
        f->zero_mode_debounce_count = 0u;
        f->transitions++;
        if (started) *started = true;
        return true;
    }

    if (f->regeneration_active && mode == 0u) {
        if (f->zero_mode_debounce_count < 255u) f->zero_mode_debounce_count++;
        if (f->zero_mode_debounce_count > 10u) {
            f->regeneration_active = false;
            f->zero_mode_debounce_count = 0u;
            f->transitions++;
            if (ended) *ended = true;
        }
    } else if (mode != 0u) {
        f->zero_mode_debounce_count = 0u;
    }

    return true;
}

bool arx_dpf_alert_build_visual(const ArxDpfAlert *f, const ArxCanFrame *source_5ae, ArxCanFrame *out) {
    if (!f || !source_5ae || !out || !f->enabled ||
        !f->visual_alert_enabled || !f->regeneration_active ||
        source_5ae->extended_id || source_5ae->id != 0x5AEu ||
        source_5ae->dlc < 5u) return false;

    if (source_5ae->data[4] & 0x04u) return false;

    *out = *source_5ae;
    out->data[4] |= 0x04u;
    return true;
}
