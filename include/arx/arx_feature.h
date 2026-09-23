#ifndef ARX_FEATURE_H
#define ARX_FEATURE_H

#include "arx/arx_can.h"
#include "arx/arx_vehicle_state.h"

typedef struct {
    const char *name;
    void (*init)(void *ctx);
    void (*on_frame)(void *ctx, const ArxCanFrame *frame, const ArxVehicleState *state);
    void (*tick)(void *ctx, uint32_t now_ms, const ArxVehicleState *state, ArxCanTransport *tx);
} ArxFeature;

#endif
