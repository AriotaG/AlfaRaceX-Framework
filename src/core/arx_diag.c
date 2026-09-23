#include "arx/arx_diag.h"
#include <string.h>

void arx_diag_init(
    ArxDiagSequence *seq,
    ArxBus bus,
    uint32_t request_id,
    bool extended_id,
    const ArxDiagStep *steps,
    size_t step_count
) {
    if (!seq) return;
    memset(seq, 0, sizeof(*seq));
    seq->bus = bus;
    seq->request_id = request_id;
    seq->extended_id = extended_id;
    seq->steps = steps;
    seq->step_count = step_count;
    seq->timeout_ms = 3000u;
}

void arx_diag_start(ArxDiagSequence *seq, uint32_t now_ms) {
    if (!seq || !seq->steps || seq->step_count == 0u) return;
    seq->step_index = 0u;
    seq->state = ARX_DIAG_RUNNING;
    seq->last_tx_ms = 0u;
    seq->started_ms = now_ms;
}

void arx_diag_stop(ArxDiagSequence *seq) {
    if (!seq) return;
    seq->state = ARX_DIAG_IDLE;
    seq->step_index = 0u;
}

bool arx_diag_next_frame(ArxDiagSequence *seq, uint32_t now_ms, ArxCanFrame *out) {
    if (!seq || !out || seq->state != ARX_DIAG_RUNNING) return false;
    if (seq->step_index >= seq->step_count) {
        seq->state = ARX_DIAG_COMPLETE;
        return false;
    }
    if (seq->timeout_ms && now_ms - seq->started_ms > seq->timeout_ms) {
        seq->state = ARX_DIAG_ERROR;
        return false;
    }

    const ArxDiagStep *step = &seq->steps[seq->step_index];
    if (seq->last_tx_ms && (now_ms - seq->last_tx_ms) < step->min_delay_ms) return false;

    memset(out, 0, sizeof(*out));
    out->bus = seq->bus;
    out->id = seq->request_id;
    out->extended_id = seq->extended_id;
    out->dlc = step->dlc;
    memcpy(out->data, step->payload, step->dlc);
    out->timestamp_ms = now_ms;
    seq->last_tx_ms = now_ms;
    return true;
}

bool arx_diag_on_positive_response(ArxDiagSequence *seq, uint8_t positive_sid, uint32_t now_ms) {
    if (!seq || seq->state != ARX_DIAG_RUNNING || seq->step_index >= seq->step_count) {
        return false;
    }

    const ArxDiagStep *step = &seq->steps[seq->step_index];
    if (step->dlc < 2u) return false;

    const uint8_t requested_sid = step->payload[1];
    if (positive_sid != (uint8_t)(requested_sid + 0x40u)) {
        return false;
    }

    seq->step_index++;
    seq->last_tx_ms = now_ms;
    if (seq->step_index >= seq->step_count) {
        seq->state = ARX_DIAG_COMPLETE;
    }
    return true;
}
