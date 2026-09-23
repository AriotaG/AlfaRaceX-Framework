#include "arx/features/arx_acc.h"
#include "arx/arx_crc.h"

static void bump_counter_crc(ArxCanFrame *frame) {
    frame->data[1] = (uint8_t)((frame->data[1] & 0xF0u) |
                              (((frame->data[1] & 0x0Fu) + 1u) & 0x0Fu));
    frame->data[2] = arx_crc8_sae_j1850(frame->data, 2u);
}

void arx_acc_init(ArxAccControl *f) {
    if (!f) return;
    f->virtual_pad_enabled = false;
    f->has_virtual_pad_enabled = false;
    f->autostart_mode = ARX_ACC_AUTOSTART_OFF;
    f->has_press_frames_remaining = 0u;
    f->autostart_burst_count = 0u;
    f->last_autostart_burst_ms = 0u;
}

void arx_acc_request_has_press(ArxAccControl *f) {
    if (f && f->has_virtual_pad_enabled) f->has_press_frames_remaining = 5u;
}

bool arx_acc_transform_2fa(
    ArxAccControl *f,
    const ArxCanFrame *source,
    uint8_t acc_status,
    bool vehicle_stationary,
    bool acc_braking,
    uint32_t now_ms,
    ArxCanFrame *out
) {
    if (!f || !source || !out || source->extended_id ||
        source->id != 0x2FAu || source->dlc != 3u) return false;

    *out = *source;

    if (f->has_press_frames_remaining && source->data[0] == 0x10u) {
        f->has_press_frames_remaining--;
        out->data[1] |= 0x10u;
        out->data[2] = arx_crc8_sae_j1850(out->data, 2u);
        return true;
    }

    if (f->virtual_pad_enabled && source->data[0] == 0x12u) {
        out->data[0] = 0x11u;
        bump_counter_crc(out);
        return true;
    }

    if (f->virtual_pad_enabled && source->data[0] == 0x90u &&
        acc_status > 1u && acc_status != 5u) {
        out->data[0] = 0x50u;
        bump_counter_crc(out);
        return true;
    }

    if (f->autostart_mode != ARX_ACC_AUTOSTART_OFF &&
        acc_status > 1u && acc_status != 5u &&
        vehicle_stationary && acc_braking &&
        source->data[0] == 0x10u &&
        now_ms - f->last_autostart_burst_ms >= 500u) {

        out->data[0] = (f->autostart_mode == ARX_ACC_AUTOSTART_PLUS) ? 0x08u : 0x90u;
        bump_counter_crc(out);

        f->autostart_burst_count++;
        if (f->autostart_burst_count >= 5u) {
            f->autostart_burst_count = 0u;
            f->last_autostart_burst_ms = now_ms;
        }
        return true;
    }

    return false;
}
