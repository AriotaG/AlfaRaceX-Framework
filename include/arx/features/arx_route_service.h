#ifndef ARX_ROUTE_SERVICE_H
#define ARX_ROUTE_SERVICE_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool pending;
    bool target_extended;
    uint32_t target_id;
    uint8_t offset;
    uint8_t request_descriptor;
} ArxRouteService;

void arx_route_service_init(ArxRouteService *f);

/* Request frame: extended ID 0x18DABAF1, descriptor byte 2, target ID bytes 3..6. */
bool arx_route_service_on_request(ArxRouteService *f, const ArxCanFrame *request);

/* Captures exactly one matching frame and generates response 0x18DAF1BA. */
bool arx_route_service_capture(
    ArxRouteService *f,
    const ArxCanFrame *candidate,
    ArxCanFrame *response
);

void arx_route_service_cancel(ArxRouteService *f);

#endif
