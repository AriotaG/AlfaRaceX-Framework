#ifndef ARX_SEATBELT_H
#define ARX_SEATBELT_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_SEATBELT_UNKNOWN = 0,
    ARX_SEATBELT_ENABLED,
    ARX_SEATBELT_DISABLED,
    ARX_SEATBELT_WAIT_SESSION_ENABLE,
    ARX_SEATBELT_WAIT_WRITE_ENABLE,
    ARX_SEATBELT_WAIT_SESSION_DISABLE,
    ARX_SEATBELT_WAIT_WRITE_DISABLE,
    ARX_SEATBELT_ERROR
} ArxSeatbeltState;

typedef struct {
    bool feature_enabled;
    ArxSeatbeltState state;
    uint32_t state_since_ms;
    uint32_t timeout_ms;
} ArxSeatbelt;

void arx_seatbelt_init(ArxSeatbelt *f);
bool arx_seatbelt_request(ArxSeatbelt *f, bool alarm_enabled, uint32_t now_ms, ArxCanFrame *out);
bool arx_seatbelt_on_response(ArxSeatbelt *f, const ArxCanFrame *response, uint32_t now_ms, ArxCanFrame *next);
void arx_seatbelt_tick(ArxSeatbelt *f, uint32_t now_ms);

#endif
