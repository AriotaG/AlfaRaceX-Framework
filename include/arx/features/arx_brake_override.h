#ifndef ARX_BRAKE_OVERRIDE_H
#define ARX_BRAKE_OVERRIDE_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_BRAKE_NORMAL = 0,
    ARX_BRAKE_SEND_SESSION,
    ARX_BRAKE_SEND_TESTER,
    ARX_BRAKE_SEND_IO,
    ARX_BRAKE_RELEASE_ONCE
} ArxBrakeState;

typedef struct {
    bool enabled;
    bool launch_assist_enabled;
    uint16_t launch_torque_nm;
    ArxBrakeState state;
    uint32_t last_tx_ms;
    uint32_t tx_count;
} ArxBrakeOverride;

void arx_brake_override_init(ArxBrakeOverride *f);
bool arx_brake_request_force(ArxBrakeOverride *f, bool vehicle_stationary, bool dyno_enabled);
void arx_brake_request_release(ArxBrakeOverride *f);
void arx_brake_on_engine_rpm(ArxBrakeOverride *f, uint16_t rpm);
bool arx_brake_launch_release_due(const ArxBrakeOverride *f, int16_t torque_nm);
bool arx_brake_tick(ArxBrakeOverride *f, uint32_t now_ms, ArxCanFrame *out);

#endif
