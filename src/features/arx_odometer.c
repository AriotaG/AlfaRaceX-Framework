#include "arx/features/arx_odometer.h"

bool arx_odometer_build_no_blink(const ArxCanFrame *source_356, ArxCanFrame *out) {
    if (!source_356 || !out || source_356->extended_id ||
        source_356->bus != ARX_BUS_BH || source_356->id != 0x356u ||
        source_356->dlc < 5u) return false;

    if ((source_356->data[4] & 0x04u) == 0u) return false;

    *out = *source_356;
    out->data[4] &= (uint8_t)~0x04u;
    return true;
}
