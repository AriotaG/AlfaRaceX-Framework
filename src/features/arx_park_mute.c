#include "arx/features/arx_park_mute.h"
#include <string.h>

void arx_park_mute_init(ArxParkMute *f) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->brake_threshold_percent = 14.5f;
}

void arx_park_mute_observe(ArxParkMute *f, const ArxCanFrame *frame) {
    if (!f || !frame || frame->extended_id || frame->bus != ARX_BUS_C2) return;

    if (frame->id == 0x107u && frame->dlc >= 1u) {
        f->brake_travel_percent = (float)frame->data[0] * 0.4f;
    } else if (frame->id == 0x3E7u && frame->dlc >= 6u) {
        f->pdc_beeping = frame->data[0] > 0u;
    } else if (frame->id == 0x54Au && frame->dlc >= 4u) {
        f->pdc_function_status = (uint8_t)(frame->data[1] & 0x03u);
        f->pdc_led_status = (uint8_t)((frame->data[3] >> 6u) & 0x03u);
    } else if (frame->id == 0x0FCu && frame->dlc >= 4u) {
        f->reverse_active = (((frame->data[3] >> 2u) & 0x03u) == 1u);
    }
}

void arx_park_mute_evaluate(ArxParkMute *f) {
    if (!f || !f->enabled || f->request_toggle || f->button_down) return;

    if (f->reverse_active) {
        if (f->auto_disabled) {
            if (f->pdc_led_status == 1u) f->request_toggle = true;
            f->auto_disabled = false;
        }
        return;
    }

    if (f->brake_travel_percent > f->brake_threshold_percent) {
        if (f->pdc_beeping && !f->auto_disabled && f->pdc_led_status != 1u) {
            f->request_toggle = true;
            f->auto_disabled = true;
        }
    } else if (f->auto_disabled) {
        if (f->pdc_led_status == 1u) {
            f->request_toggle = true;
        }
        f->auto_disabled = false;
    }
}

bool arx_park_mute_next_button_frame(ArxParkMute *f, uint32_t now_ms, ArxCanFrame *out) {
    if (!f || !out || !f->enabled || !f->request_toggle) return false;

    *out = (ArxCanFrame){
        .bus = ARX_BUS_C2,
        .id = 0x5B0u,
        .extended_id = false,
        .dlc = 8u,
        .data = {0,0,0,0,0,0,0,0},
        .timestamp_ms = now_ms
    };

    if (!f->button_down) {
        out->data[1] = 0x20u;
        f->button_down = true;
        f->button_down_since_ms = now_ms;
        return true;
    }

    if (now_ms - f->button_down_since_ms > 50u) {
        f->button_down = false;
        f->request_toggle = false;
        out->data[1] = 0x00u;
        return true;
    }

    return false;
}
