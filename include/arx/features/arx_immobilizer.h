#ifndef ARX_IMMOBILIZER_H
#define ARX_IMMOBILIZER_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_GUARD_IDLE=0,
    ARX_GUARD_ACTIVE,
    ARX_GUARD_START_ALARM_BURST,
    ARX_GUARD_STOP_ALARM_BURST
} ArxGuardState;

typedef struct {
    bool enabled;
    ArxGuardState state;
    uint32_t triggered_ms;
    uint32_t last_reset_tx_ms;
    uint8_t burst_index;
    bool alarm_active;
    uint32_t detections;
} ArxImmobilizer;

void arx_immobilizer_init(ArxImmobilizer *f);
bool arx_immobilizer_observe(ArxImmobilizer *f, const ArxCanFrame *frame, bool engine_running_long_enough, uint32_t now_ms);
bool arx_immobilizer_next_frame(ArxImmobilizer *f, uint32_t now_ms, ArxCanFrame *out);

#endif
