#ifndef ARX_PARK_MUTE_H
#define ARX_PARK_MUTE_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool pdc_beeping;
    bool reverse_active;
    bool auto_disabled;
    bool request_toggle;
    bool button_down;

    uint8_t pdc_function_status;
    uint8_t pdc_led_status;
    float brake_travel_percent;
    float brake_threshold_percent;

    uint32_t button_down_since_ms;
} ArxParkMute;

void arx_park_mute_init(ArxParkMute *f);
void arx_park_mute_observe(ArxParkMute *f, const ArxCanFrame *frame);
void arx_park_mute_evaluate(ArxParkMute *f);
bool arx_park_mute_next_button_frame(ArxParkMute *f, uint32_t now_ms, ArxCanFrame *out);

#endif
