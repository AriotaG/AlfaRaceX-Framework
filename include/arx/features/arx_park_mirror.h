#ifndef ARX_PARK_MIRROR_H
#define ARX_PARK_MIRROR_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t left_h;
    uint8_t left_v;
    uint8_t right_h;
    uint16_t right_v;
} ArxMirrorPosition;

typedef struct {
    bool enabled;
    bool calibrated_park;
    bool calibrated_normal;
    bool source_steady;

    ArxMirrorPosition park;
    ArxMirrorPosition normal;

    bool request_left_park;
    bool request_right_park;
    bool request_restore;
    bool capture_normal;
    bool capture_park;

    uint32_t left_request_ms;
    uint32_t right_request_ms;
    uint32_t restore_request_ms;
    uint32_t exit_reverse_ms;
    uint32_t neutral_entry_ms;
    uint32_t last_command_ms;

    uint32_t restore_delay_ms;
    uint32_t command_period_ms;
    uint32_t inter_command_pause_ms;
    uint32_t neutral_transient_ms;
} ArxParkMirror;

void arx_park_mirror_init(ArxParkMirror *f);
bool arx_park_mirror_observe_position(ArxParkMirror *f, const ArxCanFrame *frame);
void arx_park_mirror_request_capture_park(ArxParkMirror *f);

void arx_park_mirror_update(
    ArxParkMirror *f,
    uint8_t gear,
    uint8_t turn_indicator,
    uint16_t rpm,
    uint32_t now_ms
);
bool arx_park_mirror_build_command(ArxParkMirror *f, uint32_t now_ms, ArxCanFrame *out);

#endif
