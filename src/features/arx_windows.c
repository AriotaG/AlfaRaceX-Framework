#include "arx/features/arx_windows.h"
#include "arx/arx_crc.h"

void arx_windows_init(ArxWindows *f) {
    if (!f) return;
    *f = (ArxWindows){0};
}

void arx_windows_observe_rf(ArxWindows *f, const ArxCanFrame *frame, uint32_t now_ms) {
    if (!f || !frame || frame->extended_id || frame->id != 0x1EFu || frame->dlc != 8u) return;

    const uint8_t action = (uint8_t)(frame->data[2] >> 4u);
    const uint8_t requestor = (uint8_t)(frame->data[2] & 0x0Eu);

    if (action == 0x01u) {
        if (now_ms - f->last_lock_ms < 3000u) f->lock_clicks++;
        else f->lock_clicks = (f->close_mode == ARX_WINDOWS_ONE_CLICK) ? 1u : 0u;

        f->last_lock_ms = now_ms;
        f->fob = (uint8_t)(frame->data[1] & 0x1Eu);
        f->requestor = requestor;

        if (f->lock_clicks >= 1u) f->close_phase = 1u;
        f->open_active = false;
        f->unlock_clicks = 0u;
    }

    if ((action == 0x03u || action == 0x04u) && requestor != 0x04u) {
        if (now_ms - f->last_unlock_ms < 3000u) f->unlock_clicks++;
        else f->unlock_clicks = (f->open_mode == ARX_WINDOWS_ONE_CLICK) ? 1u : 0u;

        f->last_unlock_ms = now_ms;
        f->fob = (uint8_t)(frame->data[1] & 0x1Eu);
        f->requestor = requestor;
        f->close_phase = 0u;

        /* In multi-click mode the counter contains additional clicks after the first one.
         * Therefore value 1 means the second physical click. */
        if (f->open_mode != ARX_WINDOWS_DISABLED && f->unlock_clicks >= 1u) {
            f->open_active = true;
        }
    }
}

bool arx_windows_build_action(ArxWindows *f, const ArxCanFrame *template_1ef, uint32_t now_ms, ArxCanFrame *out) {
    if (!f || !template_1ef || !out || template_1ef->extended_id ||
        template_1ef->id != 0x1EFu || template_1ef->dlc != 8u) return false;

    *out = *template_1ef;

    if (f->close_phase == 1u && now_ms - f->last_lock_ms > 4000u) {
        out->data[1] = (uint8_t)(f->fob | 0x01u);
        out->data[2] = f->requestor;
        out->data[7] = arx_crc8_sae_j1850(out->data, 7u);

        if (now_ms - f->last_lock_ms > 9000u) {
            if (f->lock_clicks >= 2u) f->close_phase = 2u;
            else {
                f->close_phase = 0u;
                f->lock_clicks = 0u;
            }
        }
        return true;
    }

    if (f->close_phase == 2u && now_ms - f->last_lock_ms > 11000u) {
        out->data[1] = f->fob;
        out->data[2] = (uint8_t)(0xB0u | f->requestor);
        out->data[7] = arx_crc8_sae_j1850(out->data, 7u);
        if (now_ms - f->last_lock_ms > 11550u) {
            f->close_phase = 0u;
            f->lock_clicks = 0u;
        }
        return true;
    }

    if (f->open_active && now_ms - f->last_unlock_ms > 3500u) {
        out->data[1] = f->fob;
        out->data[2] = (uint8_t)(0xB0u | f->requestor);
        out->data[7] = arx_crc8_sae_j1850(out->data, 7u);
        if (now_ms - f->last_unlock_ms > 8000u) {
            f->open_active = false;
            f->unlock_clicks = 0u;
        }
        return true;
    }

    return false;
}
