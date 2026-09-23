#include "arx/arx_sgw.h"
#include <string.h>

void arx_sgw_init(ArxSgwManager *manager) {
    if (!manager) {
        return;
    }
    memset(manager, 0, sizeof(*manager));
    manager->state = ARX_SGW_UNKNOWN;
}

void arx_sgw_set_state(ArxSgwManager *manager, ArxSgwState state) {
    if (!manager) {
        return;
    }
    manager->state = state;
}
