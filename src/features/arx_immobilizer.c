#include "arx/features/arx_immobilizer.h"
#include <string.h>

void arx_immobilizer_init(ArxImmobilizer *f) {
    if (!f) return;
    memset(f,0,sizeof(*f));
}

static bool guarded_sid(const ArxCanFrame *r) {
    if (!r || !r->extended_id || r->dlc<2u) return false;
    const uint8_t off=(uint8_t)(r->data[0]>>4u);
    if (off>=2u || (uint8_t)(off+1u)>=r->dlc) return false;
    const uint8_t sid=r->data[off+1u];

    if ((r->id & 0xFFFFFFF0u)==0x18DAC7F0u) {
        return sid==0x10u || sid==0x27u || sid==0x29u ||
               sid==0x3Eu || sid==0x2Eu || sid==0x3Du;
    }
    if ((r->id & 0xFFFFF0FFu)==0x18DAF0C7u) {
        return sid==0x50u || sid==0x67u || sid==0x69u ||
               sid==0x7Eu || sid==0x6Eu || sid==0x7Du;
    }
    return false;
}

bool arx_immobilizer_observe(
    ArxImmobilizer *f,
    const ArxCanFrame *frame,
    bool engine_running_long_enough,
    uint32_t now_ms
) {
    if (!f || !frame || !f->enabled || engine_running_long_enough ||
        f->state!=ARX_GUARD_IDLE || !guarded_sid(frame)) return false;

    f->state=ARX_GUARD_ACTIVE;
    f->triggered_ms=now_ms;
    f->last_reset_tx_ms=0u;
    f->burst_index=0u;
    f->detections++;
    return true;
}

static ArxCanFrame rfhub_reset(uint32_t now) {
    return (ArxCanFrame){.bus=ARX_BUS_C1,.id=0x18DAC7F1u,.extended_id=true,.dlc=3,
        .data={0x02,0x11,0x01},.timestamp_ms=now};
}
static ArxCanFrame alarm_pulse(uint32_t now) {
    return (ArxCanFrame){.bus=ARX_BUS_C1,.id=0x1E340041u,.extended_id=true,.dlc=4,
        .data={0x88,0x20,0x15,0x00},.timestamp_ms=now};
}
static ArxCanFrame alarm_start(uint32_t now) {
    return (ArxCanFrame){.bus=ARX_BUS_C1,.id=0x1EFu,.extended_id=false,.dlc=8,
        .data={0x42,0x02,0xE2,0x00,0x00,0x00,0x01,0x56},.timestamp_ms=now};
}
static ArxCanFrame alarm_stop(uint32_t now) {
    return (ArxCanFrame){.bus=ARX_BUS_C1,.id=0x1EFu,.extended_id=false,.dlc=8,
        .data={0x00,0x00,0xE2,0x00,0x00,0x00,0x00,0x00},.timestamp_ms=now};
}

bool arx_immobilizer_next_frame(ArxImmobilizer *f, uint32_t now_ms, ArxCanFrame *out) {
    if (!f || !out || !f->enabled || f->state==ARX_GUARD_IDLE) return false;

    if (f->state==ARX_GUARD_ACTIVE) {
        if (!f->alarm_active && now_ms-f->triggered_ms>1000u) {
            f->state=ARX_GUARD_START_ALARM_BURST;
            f->burst_index=0u;
        } else if (now_ms-f->triggered_ms>10000u) {
            f->state=ARX_GUARD_STOP_ALARM_BURST;
            f->burst_index=0u;
        } else if (!f->last_reset_tx_ms || now_ms-f->last_reset_tx_ms>10u) {
            *out=rfhub_reset(now_ms);
            f->last_reset_tx_ms=now_ms;
            return true;
        }
    }

    if (f->state==ARX_GUARD_START_ALARM_BURST) {
        if (f->burst_index<15u) {
            *out=alarm_pulse(now_ms); f->burst_index++; return true;
        }
        *out=alarm_start(now_ms);
        f->alarm_active=true;
        f->state=ARX_GUARD_ACTIVE;
        f->burst_index=0u;
        return true;
    }

    if (f->state==ARX_GUARD_STOP_ALARM_BURST) {
        if (f->burst_index<15u) {
            *out=alarm_pulse(now_ms); f->burst_index++; return true;
        }
        *out=alarm_stop(now_ms);
        f->alarm_active=false;
        f->state=ARX_GUARD_IDLE;
        f->burst_index=0u;
        return true;
    }

    return false;
}
