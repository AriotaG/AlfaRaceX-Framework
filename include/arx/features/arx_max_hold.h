#ifndef ARX_MAX_HOLD_H
#define ARX_MAX_HOLD_H

#include <stdbool.h>
#include <stddef.h>

#define ARX_MAX_HOLD_SLOTS 2u

typedef struct {
    bool enabled;
    bool valid[ARX_MAX_HOLD_SLOTS];
    float value[ARX_MAX_HOLD_SLOTS];
} ArxMaxHold;

void arx_max_hold_init(ArxMaxHold *m);
void arx_max_hold_set_enabled(ArxMaxHold *m, bool enabled);
void arx_max_hold_reset(ArxMaxHold *m);
void arx_max_hold_update(ArxMaxHold *m, size_t slot, float value);
bool arx_max_hold_get(const ArxMaxHold *m, size_t slot, float *value);

#endif
