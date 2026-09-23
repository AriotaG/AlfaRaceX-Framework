#include "arx/features/arx_experimental.h"

void arx_experimental_init(ArxExperimentalCapabilities *f) {
    if (!f) return;
    f->remote_start_available = false;
}
