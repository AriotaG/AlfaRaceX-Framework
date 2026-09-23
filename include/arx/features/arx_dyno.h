#ifndef ARX_DYNO_H
#define ARX_DYNO_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_DYNO_IDLE = 0,
    ARX_DYNO_WAIT_SESSION,
    ARX_DYNO_WAIT_STATUS,
    ARX_DYNO_WAIT_DISABLE,
    ARX_DYNO_WAIT_ENABLE,
    ARX_DYNO_ERROR
} ArxDynoState;

typedef struct {
    bool enabled;
    ArxDynoState state;
    uint32_t state_since_ms;
    uint32_t last_tester_present_ms;
    uint32_t timeout_ms;
    uint32_t tx_count;
    uint32_t negative_responses;
} ArxDyno;

void arx_dyno_init(ArxDyno *f);
bool arx_dyno_toggle(ArxDyno *f, uint32_t now_ms, ArxCanFrame *out);
bool arx_dyno_on_response(ArxDyno *f, const ArxCanFrame *response, uint32_t now_ms, ArxCanFrame *next);
bool arx_dyno_tick(ArxDyno *f, uint32_t now_ms, ArxCanFrame *out);

#endif
