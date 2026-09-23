#include "arx/features/arx_seatbelt.h"
#include <string.h>

static void session(uint32_t now_ms, ArxCanFrame *out) {
    memset(out,0,sizeof(*out));
    out->bus=ARX_BUS_C1; out->id=0x18DA60F1u; out->extended_id=true; out->dlc=3;
    out->data[0]=0x02; out->data[1]=0x10; out->data[2]=0x03; out->timestamp_ms=now_ms;
}

static void write_alarm(bool enabled, uint32_t now_ms, ArxCanFrame *out) {
    const uint8_t x[6]={0x05,0x2F,0x55,0xA0,0x03, enabled ? 0x01u : 0x00u};
    memset(out,0,sizeof(*out));
    out->bus=ARX_BUS_C1; out->id=0x18DA60F1u; out->extended_id=true; out->dlc=6;
    memcpy(out->data,x,6); out->timestamp_ms=now_ms;
}

void arx_seatbelt_init(ArxSeatbelt *f) {
    if (!f) return;
    memset(f,0,sizeof(*f));
    f->state=ARX_SEATBELT_UNKNOWN;
    f->timeout_ms=10000u;
}

bool arx_seatbelt_request(ArxSeatbelt *f, bool alarm_enabled, uint32_t now_ms, ArxCanFrame *out) {
    if (!f || !out) return false;
    session(now_ms,out);
    f->state = alarm_enabled ? ARX_SEATBELT_WAIT_SESSION_ENABLE
                             : ARX_SEATBELT_WAIT_SESSION_DISABLE;
    f->state_since_ms=now_ms;
    return true;
}

bool arx_seatbelt_on_response(ArxSeatbelt *f, const ArxCanFrame *r, uint32_t now_ms, ArxCanFrame *next) {
    if (!f || !r || !next || !r->extended_id || r->id!=0x18DAF160u) return false;

    if (r->dlc>=3u && r->data[1]==0x7Fu) {
        f->state=ARX_SEATBELT_ERROR;
        return false;
    }

    if ((f->state==ARX_SEATBELT_WAIT_SESSION_ENABLE ||
         f->state==ARX_SEATBELT_WAIT_SESSION_DISABLE) &&
        r->dlc>=2u && r->data[1]==0x50u) {

        const bool enable=(f->state==ARX_SEATBELT_WAIT_SESSION_ENABLE);
        write_alarm(enable,now_ms,next);
        f->state = enable ? ARX_SEATBELT_WAIT_WRITE_ENABLE
                          : ARX_SEATBELT_WAIT_WRITE_DISABLE;
        f->state_since_ms=now_ms;
        return true;
    }

    if ((f->state==ARX_SEATBELT_WAIT_WRITE_ENABLE ||
         f->state==ARX_SEATBELT_WAIT_WRITE_DISABLE) &&
        r->dlc>=4u && r->data[1]==0x6Fu &&
        r->data[2]==0x55u && r->data[3]==0xA0u) {

        f->state = (f->state==ARX_SEATBELT_WAIT_WRITE_ENABLE)
            ? ARX_SEATBELT_ENABLED : ARX_SEATBELT_DISABLED;
        return false;
    }

    return false;
}

void arx_seatbelt_tick(ArxSeatbelt *f, uint32_t now_ms) {
    if (!f) return;
    switch(f->state){
        case ARX_SEATBELT_WAIT_SESSION_ENABLE:
        case ARX_SEATBELT_WAIT_WRITE_ENABLE:
        case ARX_SEATBELT_WAIT_SESSION_DISABLE:
        case ARX_SEATBELT_WAIT_WRITE_DISABLE:
            if(now_ms-f->state_since_ms > f->timeout_ms) f->state=ARX_SEATBELT_UNKNOWN;
            break;
        default: break;
    }
}
