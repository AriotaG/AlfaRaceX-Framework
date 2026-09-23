#ifndef ARX_FEATURE_CATALOG_H
#define ARX_FEATURE_CATALOG_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ARX_FEATURE_STABLE_CORE = 0,
    ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,
    ARX_FEATURE_EXPERIMENTAL_DISABLED
} ArxFeatureMaturity;

typedef struct {
    const char *id;
    const char *name;
    ArxFeatureMaturity maturity;
    bool requires_vehicle_actuation;
} ArxFeatureDescriptor;

const ArxFeatureDescriptor *arx_feature_catalog(size_t *count);

#endif
