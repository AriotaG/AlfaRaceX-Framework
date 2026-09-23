#ifndef ARX_DECODER_H
#define ARX_DECODER_H

#include "arx/arx_vehicle_state.h"

void arx_decode_frame(const ArxCanFrame *frame, ArxVehicleState *state);

#endif
