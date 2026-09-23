#ifndef ARX_QV_EXHAUST_H
#define ARX_QV_EXHAUST_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_EXHAUST_ECU_CONTROL = 0,
    ARX_EXHAUST_SEND_SESSION,
    ARX_EXHAUST_SEND_TESTER,
    ARX_EXHAUST_SEND_OPEN_IO,
    ARX_EXHAUST_RETURN_CONTROL
} ArxExhaustState;

typedef struct {
    bool enabled;
    ArxExhaustState state;
    uint32_t last_tx_ms;
    uint32_t tx_count;
} ArxQvExhaust;

void arx_qv_exhaust_init(ArxQvExhaust *f);
void arx_qv_exhaust_toggle(ArxQvExhaust *f);
void arx_qv_exhaust_on_engine_rpm(ArxQvExhaust *f, uint16_t rpm);
bool arx_qv_exhaust_tick(ArxQvExhaust *f, uint32_t now_ms, ArxCanFrame *out);

#endif
