#ifndef ARX_DIAG_H
#define ARX_DIAG_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t payload[8];
    uint8_t dlc;
    uint32_t min_delay_ms;
} ArxDiagStep;

typedef enum {
    ARX_DIAG_IDLE = 0,
    ARX_DIAG_RUNNING,
    ARX_DIAG_HOLD,
    ARX_DIAG_COMPLETE,
    ARX_DIAG_ERROR
} ArxDiagState;

typedef struct {
    ArxBus bus;
    uint32_t request_id;
    bool extended_id;

    const ArxDiagStep *steps;
    size_t step_count;
    size_t step_index;

    ArxDiagState state;
    uint32_t last_tx_ms;
    uint32_t timeout_ms;
    uint32_t started_ms;
} ArxDiagSequence;

void arx_diag_init(
    ArxDiagSequence *seq,
    ArxBus bus,
    uint32_t request_id,
    bool extended_id,
    const ArxDiagStep *steps,
    size_t step_count
);

void arx_diag_start(ArxDiagSequence *seq, uint32_t now_ms);
void arx_diag_stop(ArxDiagSequence *seq);
bool arx_diag_next_frame(ArxDiagSequence *seq, uint32_t now_ms, ArxCanFrame *out);
bool arx_diag_on_positive_response(ArxDiagSequence *seq, uint8_t positive_sid, uint32_t now_ms);

#endif
