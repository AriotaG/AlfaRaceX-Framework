#ifndef ARX_ROUTER_H
#define ARX_ROUTER_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool armed;
    bool target_extended;
    uint8_t offset;
    uint8_t request_descriptor;
    uint32_t target_id;
} ArxRouteService;

void arx_route_init(ArxRouteService *f);
bool arx_route_on_request(ArxRouteService *f, const ArxCanFrame *request);
void arx_route_cancel(ArxRouteService *f);
bool arx_route_capture(ArxRouteService *f, const ArxCanFrame *candidate, bool dashboard_busy, ArxCanFrame *response);

#endif
