#ifndef ARX_DPF_ALERT_H
#define ARX_DPF_ALERT_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool visual_alert_enabled;
    bool sound_alert_enabled;
    bool regeneration_active;

    uint8_t last_regen_mode;
    uint8_t zero_mode_debounce_count;
    uint32_t transitions;
} ArxDpfAlert;

void arx_dpf_alert_init(ArxDpfAlert *f);
bool arx_dpf_alert_on_5ae(ArxDpfAlert *f, const ArxCanFrame *frame, bool *started, bool *ended);
bool arx_dpf_alert_build_visual(const ArxDpfAlert *f, const ArxCanFrame *source_5ae, ArxCanFrame *out);

#endif
