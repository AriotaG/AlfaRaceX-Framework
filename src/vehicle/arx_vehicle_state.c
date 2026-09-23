#include "arx/arx_vehicle_state.h"
#include <string.h>

void arx_vehicle_state_init(ArxVehicleState *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->dna_mode = ARX_DNA_UNKNOWN;
    state->acc_state = ARX_ACC_UNKNOWN;
}

bool arx_vehicle_engine_running(const ArxVehicleState *state) {
    return state && (state->valid_mask & ARX_VS_ENGINE_RPM) &&
           state->engine_rpm > 400u;
}
