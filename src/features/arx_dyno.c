#include "arx/features/arx_dyno.h"
#include <string.h>

static void make(uint8_t which, uint32_t now_ms, ArxCanFrame *out) {
    static const uint8_t payloads[][6] = {
        {0x02,0x10,0x03,0,0,0},       /* session */
        {0x03,0x22,0x30,0x02,0,0},    /* read status */
        {0x05,0x2E,0x30,0x02,0x00,0x01}, /* disable */
        {0x05,0x2E,0x30,0x02,0xFF,0x01}, /* enable */
        {0x02,0x3E,0x80,0,0,0}        /* tester present */
    };
    static const uint8_t dlc[] = {3,4,6,6,3};

    memset(out, 0, sizeof(*out));
    out->bus = ARX_BUS_C2;
    out->id = 0x18DA28F1u;
    out->extended_id = true;
    out->dlc = dlc[which];
    memcpy(out->data, payloads[which], dlc[which]);
    out->timestamp_ms = now_ms;
}

void arx_dyno_init(ArxDyno *f) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->state = ARX_DYNO_IDLE;
    f->timeout_ms = 4000u;
}

bool arx_dyno_toggle(ArxDyno *f, uint32_t now_ms, ArxCanFrame *out) {
    if (!f || !out || f->state != ARX_DYNO_IDLE) return false;
    make(0u, now_ms, out);
    f->state = ARX_DYNO_WAIT_SESSION;
    f->state_since_ms = now_ms;
    f->tx_count++;
    return true;
}

bool arx_dyno_on_response(ArxDyno *f, const ArxCanFrame *r, uint32_t now_ms, ArxCanFrame *next) {
    if (!f || !r || !next || !r->extended_id || r->id != 0x18DAF128u) return false;
    if (r->dlc >= 3u && r->data[1] == 0x7Fu) {
        f->negative_responses++;
        f->state = ARX_DYNO_ERROR;
        return false;
    }

    switch (f->state) {
        case ARX_DYNO_WAIT_SESSION:
            if (r->dlc >= 3u && r->data[1] == 0x50u && r->data[2] == 0x03u) {
                make(1u, now_ms, next);
                f->state = ARX_DYNO_WAIT_STATUS;
            } else return false;
            break;

        case ARX_DYNO_WAIT_STATUS:
            if (r->dlc >= 5u && r->data[1] == 0x62u &&
                r->data[2] == 0x30u && r->data[3] == 0x02u) {
                if (r->data[4] == 0x00u) {
                    f->enabled = false;
                    make(3u, now_ms, next);
                    f->state = ARX_DYNO_WAIT_ENABLE;
                } else {
                    f->enabled = true;
                    make(2u, now_ms, next);
                    f->state = ARX_DYNO_WAIT_DISABLE;
                }
            } else return false;
            break;

        case ARX_DYNO_WAIT_DISABLE:
            if (r->dlc >= 4u && r->data[1] == 0x6Eu &&
                r->data[2] == 0x30u && r->data[3] == 0x02u) {
                f->enabled = false;
                f->state = ARX_DYNO_IDLE;
                return false;
            }
            return false;

        case ARX_DYNO_WAIT_ENABLE:
            if (r->dlc >= 4u && r->data[1] == 0x6Eu &&
                r->data[2] == 0x30u && r->data[3] == 0x02u) {
                f->enabled = true;
                f->state = ARX_DYNO_IDLE;
                f->last_tester_present_ms = now_ms;
                return false;
            }
            return false;

        default:
            return false;
    }

    f->state_since_ms = now_ms;
    f->tx_count++;
    return true;
}

bool arx_dyno_tick(ArxDyno *f, uint32_t now_ms, ArxCanFrame *out) {
    if (!f || !out) return false;

    if (f->state != ARX_DYNO_IDLE && f->state != ARX_DYNO_ERROR &&
        now_ms - f->state_since_ms > f->timeout_ms) {
        f->state = ARX_DYNO_ERROR;
        return false;
    }

    if (f->enabled && f->state == ARX_DYNO_IDLE &&
        now_ms - f->last_tester_present_ms > 500u) {
        make(4u, now_ms, out);
        f->last_tester_present_ms = now_ms;
        f->tx_count++;
        return true;
    }
    return false;
}
