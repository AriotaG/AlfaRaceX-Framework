#ifndef ARX_EXPERIMENTAL_H
#define ARX_EXPERIMENTAL_H

#include <stdbool.h>

typedef struct {
    bool remote_start_available;
} ArxExperimentalCapabilities;

void arx_experimental_init(ArxExperimentalCapabilities *f);

#endif
