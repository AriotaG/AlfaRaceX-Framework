#include "arx/features/arx_dashboard.h"
#include <string.h>

void arx_dashboard_init(ArxDashboard *f) {
    if (!f) return;
    memset(f,0,sizeof(*f));
    f->info_code=0x09u;
    memset(f->text,' ',ARX_DASHBOARD_TEXT_LEN);
}

void arx_dashboard_next(ArxDashboard *f, size_t count, uint32_t now_ms) {
    if (!f || !count) return;
    f->page=(f->page+1u)%count;
    f->last_activity_ms=now_ms;
}

void arx_dashboard_prev(ArxDashboard *f, size_t count, uint32_t now_ms) {
    if (!f || !count) return;
    f->page=(f->page==0u)?count-1u:f->page-1u;
    f->last_activity_ms=now_ms;
}

void arx_dashboard_set_text(ArxDashboard *f, const char *text, uint8_t repeats) {
    if (!f) return;
    memset(f->text,' ',ARX_DASHBOARD_TEXT_LEN);
    if (text) {
        size_t n=strlen(text);
        if (n>ARX_DASHBOARD_TEXT_LEN) n=ARX_DASHBOARD_TEXT_LEN;
        memcpy(f->text,text,n);
    }
    f->frame_index=0u;
    f->repeats_remaining=repeats;
}

bool arx_dashboard_next_telematic_frame(
    ArxDashboard *f,
    uint32_t now_ms,
    ArxCanFrame *out
) {
    if (!f || !out || f->repeats_remaining==0u) return false;
    if (f->last_frame_ms && now_ms-f->last_frame_ms<=50u) return false;

    const uint8_t total=(uint8_t)(ARX_DASHBOARD_FRAME_COUNT-1u);
    const uint8_t frame=f->frame_index;
    const size_t i=(size_t)frame*3u;

    *out=(ArxCanFrame){
        .bus=ARX_BUS_BH,.id=0x090u,.extended_id=false,.dlc=8u,
        .data={0,0,0,0,0,0,0,0},.timestamp_ms=now_ms
    };

    out->data[0]=(uint8_t)(((total<<3u)&0xF8u)|((frame>>2u)&0x07u));
    out->data[1]=(uint8_t)((f->info_code&0x3Fu)|((frame<<6u)&0xC0u));
    out->data[3]=(uint8_t)f->text[i+0u];
    out->data[5]=(uint8_t)f->text[i+1u];
    out->data[7]=(uint8_t)f->text[i+2u];

    f->last_frame_ms=now_ms;
    f->frame_index++;
    if (f->frame_index>=ARX_DASHBOARD_FRAME_COUNT) {
        f->frame_index=0u;
        f->repeats_remaining--;
    }
    return true;
}
