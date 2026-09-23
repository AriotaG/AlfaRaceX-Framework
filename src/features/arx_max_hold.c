#include "arx/features/arx_max_hold.h"
#include <string.h>

void arx_max_hold_init(ArxMaxHold *m) {
    if (!m) return;
    memset(m,0,sizeof(*m));
}

void arx_max_hold_set_enabled(ArxMaxHold *m,bool enabled) {
    if (!m) return;
    if (enabled && !m->enabled) arx_max_hold_reset(m);
    m->enabled=enabled;
}

void arx_max_hold_reset(ArxMaxHold *m) {
    if (!m) return;
    memset(m->valid,0,sizeof(m->valid));
    memset(m->value,0,sizeof(m->value));
}

void arx_max_hold_update(ArxMaxHold *m,size_t slot,float value) {
    if (!m || !m->enabled || slot>=ARX_MAX_HOLD_SLOTS) return;
    if (!m->valid[slot] || value>m->value[slot]) {
        m->value[slot]=value;
        m->valid[slot]=true;
    }
}

bool arx_max_hold_get(const ArxMaxHold *m,size_t slot,float *value) {
    if (!m || !value || slot>=ARX_MAX_HOLD_SLOTS || !m->valid[slot]) return false;
    *value=m->value[slot];
    return true;
}
