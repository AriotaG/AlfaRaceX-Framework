#ifndef ARX_AWD_H
#define ARX_AWD_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_AWD_NORMAL = 0,
    ARX_AWD_SEND_SESSION,
    ARX_AWD_SEND_TESTER_PRESENT,
    ARX_AWD_SEND_ZERO_FRONT_TORQUE,
    ARX_AWD_HOLD_DISABLED
} ArxAwdState;

typedef struct {
    bool enabled;
    ArxAwdState state;
    uint32_t last_tx_ms;
    uint32_t tx_count;
} ArxAwdControl;

void arx_awd_init(ArxAwdControl *f);
bool arx_awd_request_disable(ArxAwdControl *f, bool vehicle_stationary);
void arx_awd_request_normal(ArxAwdControl *f);
void arx_awd_on_engine_rpm(ArxAwdControl *f, uint16_t rpm);
bool arx_awd_tick(ArxAwdControl *f, uint32_t now_ms, ArxCanFrame *out);

#endif
