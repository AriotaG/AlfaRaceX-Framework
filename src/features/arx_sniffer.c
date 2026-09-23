#include "arx/features/arx_sniffer.h"
#include <string.h>

void arx_sniffer_init(ArxSniffer *f) {
    if (!f) return;
    memset(f,0,sizeof(*f));
}

void arx_sniffer_start(ArxSniffer *f, uint32_t now_ms) {
    if (!f || !f->enabled) return;
    f->head=f->tail=f->count=0u;
    f->dropped_pending=0u;
    f->last_flush_ms=now_ms;
    f->in_use=true;
}

void arx_sniffer_stop(ArxSniffer *f) {
    if (f) f->in_use=false;
}

static void advance_head(ArxSniffer *f) {
    f->head=(uint16_t)((f->head+ARX_SNIFFER_FRAME_SIZE)%ARX_SNIFFER_BUFFER_SIZE);
    f->count=(uint16_t)(f->count+ARX_SNIFFER_FRAME_SIZE);
}

static void put_u24le(uint8_t *d, uint32_t v) {
    d[0]=(uint8_t)v; d[1]=(uint8_t)(v>>8u); d[2]=(uint8_t)(v>>16u);
}
static void put_u32le(uint8_t *d, uint32_t v) {
    d[0]=(uint8_t)v; d[1]=(uint8_t)(v>>8u); d[2]=(uint8_t)(v>>16u); d[3]=(uint8_t)(v>>24u);
}

static void emit_overflow_marker(ArxSniffer *f, uint32_t timestamp_ms) {
    uint8_t *d=&f->ring[f->head];
    memset(d,0,ARX_SNIFFER_FRAME_SIZE);
    d[0]=ARX_SNIFFER_OVERFLOW_MARKER;
    put_u24le(&d[1],timestamp_ms);
    d[4]=(uint8_t)f->dropped_pending;
    d[5]=(uint8_t)(f->dropped_pending>>8u);
    advance_head(f);
    f->dropped_pending=0u;
}

bool arx_sniffer_push(ArxSniffer *f, const ArxCanFrame *frame) {
    if (!f || !frame || !f->enabled || !f->in_use) return false;

    if (f->dropped_pending>0u &&
        f->count<=ARX_SNIFFER_BUFFER_SIZE-(2u*ARX_SNIFFER_FRAME_SIZE)) {
        emit_overflow_marker(f,frame->timestamp_ms);
    }

    if (f->count>ARX_SNIFFER_BUFFER_SIZE-ARX_SNIFFER_FRAME_SIZE) {
        if (f->dropped_pending<0xFFFFu) f->dropped_pending++;
        f->dropped_total++;
        return false;
    }

    uint8_t dlc=frame->dlc>8u?8u:frame->dlc;
    uint8_t *d=&f->ring[f->head];
    memset(d,0,ARX_SNIFFER_FRAME_SIZE);
    d[0]=(uint8_t)(ARX_SNIFFER_START_NIBBLE|dlc);
    put_u24le(&d[1],frame->timestamp_ms);
    put_u32le(&d[4],frame->id);
    memcpy(&d[8],frame->data,dlc);
    advance_head(f);
    f->captured++;
    return true;
}

size_t arx_sniffer_peek_chunk(
    ArxSniffer *f,
    uint32_t now_ms,
    uint8_t *out,
    size_t cap
) {
    if (!f || !out || !f->in_use || f->count==0u || cap==0u) return 0u;

    size_t n=f->count;
    if (n<ARX_SNIFFER_USB_CHUNK &&
        now_ms-f->last_flush_ms<ARX_SNIFFER_FLUSH_TIMEOUT_MS) return 0u;

    if (n>ARX_SNIFFER_USB_CHUNK) n=ARX_SNIFFER_USB_CHUNK;
    size_t contiguous=ARX_SNIFFER_BUFFER_SIZE-f->tail;
    if (n>contiguous) n=contiguous;
    if (n>cap) n=cap;

    /* Keep USB writes frame-aligned when the caller provides a small buffer. */
    n-=(n%ARX_SNIFFER_FRAME_SIZE);
    if (!n) return 0u;

    memcpy(out,&f->ring[f->tail],n);
    return n;
}

void arx_sniffer_commit_chunk(ArxSniffer *f, size_t n, uint32_t now_ms) {
    if (!f || n==0u || n>f->count || (n%ARX_SNIFFER_FRAME_SIZE)!=0u) return;
    f->tail=(uint16_t)((f->tail+n)%ARX_SNIFFER_BUFFER_SIZE);
    f->count=(uint16_t)(f->count-n);
    f->last_flush_ms=now_ms;
}
