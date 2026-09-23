#ifndef ARX_TELEMETRY_DB_H
#define ARX_TELEMETRY_DB_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    ARX_SIGNAL_NATIVE=0,
    ARX_SIGNAL_UDS
} ArxSignalSource;

typedef struct {
    const char *key;
    ArxSignalSource source;

    uint32_t request_id;
    uint32_t response_id;
    uint8_t request[8];
    uint8_t request_len;

    uint8_t reply_len;
    uint8_t reply_offset;
    bool signed_value;
    float scale;
    float offset;          /* engineering-domain offset, applied after scale */
    const char *unit;

    int32_t raw_offset;    /* applied before scale, for compatibility definitions */
    uint8_t decimal_digits;
} ArxTelemetryDefinition;

#define ARX_DIESEL_DASHBOARD_PAGE_COUNT 55u

typedef struct {
    const char *title;
    const char *primary_key;
    const char *secondary_key;
} ArxTelemetryPage;

const ArxTelemetryDefinition *arx_telemetry_diesel(size_t *count);
const ArxTelemetryPage *arx_telemetry_diesel_pages(size_t *count);
const ArxTelemetryDefinition *arx_telemetry_find(
    const ArxTelemetryDefinition *db,
    size_t count,
    const char *key
);

bool arx_telemetry_build_request(
    const ArxTelemetryDefinition *def,
    ArxBus bus,
    uint32_t now_ms,
    ArxCanFrame *out
);

bool arx_telemetry_decode_response(
    const ArxTelemetryDefinition *def,
    const ArxCanFrame *frame,
    float *value
);

#endif
