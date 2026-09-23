#include "arx/features/arx_awd.h"
#include <string.h>

void arx_awd_init(ArxAwdControl *f) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
}

bool arx_awd_request_disable(ArxAwdControl *f, bool vehicle_stationary) {
    if (!f || !f->enabled || !vehicle_stationary || f->state != ARX_AWD_NORMAL) return false;
    f->state = ARX_AWD_SEND_SESSION;
    f->last_tx_ms = 0u;
    return true;
}

void arx_awd_request_normal(ArxAwdControl *f) {
    if (!f) return;
    f->state = ARX_AWD_NORMAL;
}

void arx_awd_on_engine_rpm(ArxAwdControl *f, uint16_t rpm) {
    if (f && rpm < 400u) f->state = ARX_AWD_NORMAL;
}

static void make(uint8_t which, uint32_t now_ms, ArxCanFrame *out) {
    memset(out, 0, sizeof(*out));
    out->bus = ARX_BUS_C1;
    out->id = 0x18DA1AF1u;
    out->extended_id = true;
    out->timestamp_ms = now_ms;

    switch (which) {
        case 0: out->dlc=3; out->data[0]=0x02; out->data[1]=0x10; out->data[2]=0x03; break;
        case 1: out->dlc=3; out->data[0]=0x02; out->data[1]=0x3E; out->data[2]=0x80; break;
        case 2:
            out->dlc=7;
            { const uint8_t x[7]={0x06,0x2F,0x2A,0xAA,0x03,0x00,0x00};
              memcpy(out->data,x,7); }
            break;
        default: out->dlc=3; out->data[0]=0x02; out->data[1]=0x11; out->data[2]=0x01; break;
    }
}

bool arx_awd_tick(ArxAwdControl *f, uint32_t now_ms, ArxCanFrame *out) {
    if (!f || !out || !f->enabled || f->state == ARX_AWD_NORMAL) return false;

    const bool hold = (f->state == ARX_AWD_HOLD_DISABLED);
    const uint32_t period = hold ? 30u : 100u;
    if (f->last_tx_ms != 0u && now_ms - f->last_tx_ms <= period) return false;

    switch (f->state) {
        case ARX_AWD_SEND_SESSION:
            make(0u, now_ms, out);
            f->state = ARX_AWD_SEND_TESTER_PRESENT;
            break;
        case ARX_AWD_SEND_TESTER_PRESENT:
            make(1u, now_ms, out);
            f->state = ARX_AWD_SEND_ZERO_FRONT_TORQUE;
            break;
        case ARX_AWD_SEND_ZERO_FRONT_TORQUE:
            make(2u, now_ms, out);
            f->state = ARX_AWD_HOLD_DISABLED;
            break;
        case ARX_AWD_HOLD_DISABLED:
            make(3u, now_ms, out);
            break;
        default:
            return false;
    }

    f->last_tx_ms = now_ms;
    f->tx_count++;
    return true;
}
