#include "arx/features/arx_brake_override.h"
#include <string.h>

void arx_brake_override_init(ArxBrakeOverride *f) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->launch_torque_nm = 100u;
}

bool arx_brake_request_force(ArxBrakeOverride *f, bool vehicle_stationary, bool dyno_enabled) {
    if (!f || !f->enabled || !vehicle_stationary || !dyno_enabled ||
        f->state != ARX_BRAKE_NORMAL) return false;

    f->state = ARX_BRAKE_SEND_SESSION;
    f->launch_assist_enabled = true;
    f->last_tx_ms = 0u;
    return true;
}

void arx_brake_request_release(ArxBrakeOverride *f) {
    if (!f || f->state == ARX_BRAKE_NORMAL) return;
    f->launch_assist_enabled = false;
    f->state = ARX_BRAKE_RELEASE_ONCE;
    f->last_tx_ms = 0u;
}

void arx_brake_on_engine_rpm(ArxBrakeOverride *f, uint16_t rpm) {
    if (f && rpm < 400u && f->state != ARX_BRAKE_NORMAL) {
        f->state = ARX_BRAKE_RELEASE_ONCE;
        f->launch_assist_enabled = false;
        f->last_tx_ms = 0u;
    }
}

bool arx_brake_launch_release_due(const ArxBrakeOverride *f, int16_t torque_nm) {
    return f && f->launch_assist_enabled &&
           f->state != ARX_BRAKE_NORMAL &&
           torque_nm >= (int16_t)f->launch_torque_nm;
}

static void make(uint8_t which, uint32_t now_ms, ArxCanFrame *out) {
    memset(out, 0, sizeof(*out));
    out->bus=ARX_BUS_C2;
    out->id=0x18DA28F1u;
    out->extended_id=true;
    out->timestamp_ms=now_ms;

    if (which==0u) {
        const uint8_t x[5]={0x04,0x2F,0x5A,0xBD,0x00};
        out->dlc=5; memcpy(out->data,x,5);
    } else if (which==1u) {
        const uint8_t x[8]={0x07,0x2F,0x5A,0xBD,0x03,0x27,0x10,0x03};
        out->dlc=8; memcpy(out->data,x,8);
    } else if (which==2u) {
        out->dlc=3; out->data[0]=0x02; out->data[1]=0x3E; out->data[2]=0x80;
    } else {
        out->dlc=3; out->data[0]=0x02; out->data[1]=0x10; out->data[2]=0x40;
    }
}

bool arx_brake_tick(ArxBrakeOverride *f, uint32_t now_ms, ArxCanFrame *out) {
    if (!f || !out || !f->enabled || f->state == ARX_BRAKE_NORMAL) return false;
    if (f->last_tx_ms && now_ms - f->last_tx_ms <= 500u) return false;

    switch (f->state) {
        case ARX_BRAKE_RELEASE_ONCE:
            make(0u,now_ms,out);
            f->state=ARX_BRAKE_NORMAL;
            break;
        case ARX_BRAKE_SEND_SESSION:
            make(3u,now_ms,out);
            f->state=ARX_BRAKE_SEND_TESTER;
            break;
        case ARX_BRAKE_SEND_TESTER:
            make(2u,now_ms,out);
            f->state=ARX_BRAKE_SEND_IO;
            break;
        case ARX_BRAKE_SEND_IO:
            make(1u,now_ms,out);
            f->state=ARX_BRAKE_SEND_TESTER;
            break;
        default:
            return false;
    }

    f->last_tx_ms=now_ms;
    f->tx_count++;
    return true;
}
