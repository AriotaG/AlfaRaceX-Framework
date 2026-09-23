#include "arx/features/arx_router.h"
#include <string.h>

void arx_route_init(ArxRouteService *f) {
    if (!f) return;
    memset(f,0,sizeof(*f));
}

bool arx_route_on_request(ArxRouteService *f, const ArxCanFrame *r) {
    if (!f || !r || !f->enabled || !r->extended_id ||
        r->bus!=ARX_BUS_C1 || r->id!=0x18DABAF1u || r->dlc<7u) return false;

    f->target_extended = (r->data[2] >> 4u) != 0u;
    f->offset=(uint8_t)(r->data[2]&0x0Fu);
    f->request_descriptor=r->data[2];
    f->target_id=((uint32_t)r->data[3]<<24) | ((uint32_t)r->data[4]<<16) |
                 ((uint32_t)r->data[5]<<8) | (uint32_t)r->data[6];
    f->armed=true;
    return true;
}

void arx_route_cancel(ArxRouteService *f) {
    if (f) f->armed=false;
}

bool arx_route_capture(ArxRouteService *f, const ArxCanFrame *c, bool dashboard_busy, ArxCanFrame *resp) {
    if (!f || !c || !resp || !f->enabled || !f->armed) return false;
    if (dashboard_busy) {
        f->armed=false;
        return false;
    }
    if (c->extended_id != f->target_extended || c->id != f->target_id) return false;
    f->armed=false;
    if (f->offset>=c->dlc) return false;

    *resp=(ArxCanFrame){
        .bus=ARX_BUS_C1,.id=0x18DAF1BAu,.extended_id=true,.dlc=8,
        .data={0x07,0x62,0,0,0,0,0,0},.timestamp_ms=c->timestamp_ms
    };
    resp->data[2]=f->request_descriptor;
    uint8_t n=(uint8_t)(c->dlc-f->offset);
    if (n>5u) n=5u;
    memcpy(&resp->data[3],&c->data[f->offset],n);
    return true;
}
