#include "arx/features/arx_qv_exhaust.h"
#include <string.h>

void arx_qv_exhaust_init(ArxQvExhaust *f) {
    if (!f) return;
    memset(f,0,sizeof(*f));
}

void arx_qv_exhaust_toggle(ArxQvExhaust *f) {
    if (!f || !f->enabled) return;
    f->state = (f->state == ARX_EXHAUST_ECU_CONTROL)
        ? ARX_EXHAUST_SEND_SESSION
        : ARX_EXHAUST_RETURN_CONTROL;
    f->last_tx_ms=0u;
}

void arx_qv_exhaust_on_engine_rpm(ArxQvExhaust *f, uint16_t rpm) {
    if (f && rpm == 0u && f->state != ARX_EXHAUST_ECU_CONTROL) {
        f->state=ARX_EXHAUST_RETURN_CONTROL;
        f->last_tx_ms=0u;
    }
}

static void make(ArxExhaustState state, uint32_t now_ms, ArxCanFrame *out) {
    memset(out,0,sizeof(*out));
    out->bus=ARX_BUS_C1; out->id=0x18DA17F1u; out->extended_id=true; out->timestamp_ms=now_ms;

    switch(state){
        case ARX_EXHAUST_SEND_SESSION:
            out->dlc=3; out->data[0]=0x02; out->data[1]=0x10; out->data[2]=0x03; break;
        case ARX_EXHAUST_SEND_TESTER:
            out->dlc=3; out->data[0]=0x02; out->data[1]=0x3E; out->data[2]=0x00; break;
        case ARX_EXHAUST_SEND_OPEN_IO: {
            const uint8_t x[7]={0x06,0x2F,0x51,0x90,0x03,0x00,0x00};
            out->dlc=7; memcpy(out->data,x,7); break;
        }
        default: {
            const uint8_t x[5]={0x04,0x2F,0x51,0x90,0x00};
            out->dlc=5; memcpy(out->data,x,5); break;
        }
    }
}

bool arx_qv_exhaust_tick(ArxQvExhaust *f, uint32_t now_ms, ArxCanFrame *out) {
    if (!f || !out || !f->enabled || f->state==ARX_EXHAUST_ECU_CONTROL) return false;
    if (f->last_tx_ms && now_ms-f->last_tx_ms <= 500u) return false;

    const ArxExhaustState sent=f->state;
    make(sent,now_ms,out);

    switch(sent){
        case ARX_EXHAUST_SEND_SESSION: f->state=ARX_EXHAUST_SEND_TESTER; break;
        case ARX_EXHAUST_SEND_TESTER: f->state=ARX_EXHAUST_SEND_OPEN_IO; break;
        case ARX_EXHAUST_SEND_OPEN_IO: f->state=ARX_EXHAUST_SEND_TESTER; break;
        case ARX_EXHAUST_RETURN_CONTROL: f->state=ARX_EXHAUST_ECU_CONTROL; break;
        default: break;
    }

    f->last_tx_ms=now_ms;
    f->tx_count++;
    return true;
}
