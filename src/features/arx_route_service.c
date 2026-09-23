#include "arx/features/arx_route_service.h"
#include <string.h>

void arx_route_service_init(ArxRouteService *f) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
}

bool arx_route_service_on_request(ArxRouteService *f, const ArxCanFrame *request) {
    if (!f || !request || !f->enabled || !request->extended_id ||
        request->id != 0x18DABAF1u || request->dlc < 7u) return false;

    f->target_extended = ((request->data[2] >> 4u) != 0u);
    f->offset = (uint8_t)(request->data[2] & 0x0Fu);
    f->request_descriptor = request->data[2];
    f->target_id =
        ((uint32_t)request->data[3] << 24u) |
        ((uint32_t)request->data[4] << 16u) |
        ((uint32_t)request->data[5] << 8u) |
        (uint32_t)request->data[6];
    f->pending = true;
    return true;
}

bool arx_route_service_capture(
    ArxRouteService *f,
    const ArxCanFrame *candidate,
    ArxCanFrame *response
) {
    if (!f || !candidate || !response || !f->enabled || !f->pending) return false;
    if (candidate->extended_id != f->target_extended || candidate->id != f->target_id) return false;

    /* One-shot behavior: consume the request even if offset is invalid. */
    f->pending = false;
    if (f->offset >= candidate->dlc) return false;

    memset(response, 0, sizeof(*response));
    response->bus = ARX_BUS_C1;
    response->id = 0x18DAF1BAu;
    response->extended_id = true;
    response->dlc = 8u;
    response->timestamp_ms = candidate->timestamp_ms;

    response->data[0] = 0x07u;
    response->data[1] = 0x62u;
    response->data[2] = f->request_descriptor;

    uint8_t n = (uint8_t)(candidate->dlc - f->offset);
    if (n > 5u) n = 5u;
    memcpy(&response->data[3], &candidate->data[f->offset], n);
    return true;
}

void arx_route_service_cancel(ArxRouteService *f) {
    if (f) f->pending = false;
}
