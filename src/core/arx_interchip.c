#include "arx/arx_interchip.h"
#include <string.h>

void arx_interchip_init(ArxInterchip *link, ArxInterchipRole role) {
    if (!link) return;
    memset(link, 0, sizeof(*link));

    link->role = role;
    link->boot_ignore_ms = 2000u;
    link->master_tx_period_ms = 250u;
    link->slave_reply_window_ms = 200u;
    link->c2_status_period_ms = 1010u;
    link->bh_status_period_ms = 1260u;
}

void arx_interchip_frame_build(
    ArxInterchipFrame *frame,
    const uint8_t *payload,
    size_t payload_len
) {
    if (!frame) return;

    memset(frame->bytes, ARX_INTERCHIP_PAD, sizeof(frame->bytes));
    if (!payload) return;

    if (payload_len > ARX_INTERCHIP_FRAME_SIZE) {
        payload_len = ARX_INTERCHIP_FRAME_SIZE;
    }
    memcpy(frame->bytes, payload, payload_len);
}

bool arx_interchip_queue_push(
    ArxInterchipQueue *q,
    const ArxInterchipFrame *frame
) {
    if (!q || !frame) return false;
    if (q->count >= ARX_INTERCHIP_QUEUE_SIZE) {
        q->dropped++;
        return false;
    }

    q->items[q->tail] = *frame;
    q->tail = (uint8_t)((q->tail + 1u) % ARX_INTERCHIP_QUEUE_SIZE);
    q->count++;
    return true;
}

bool arx_interchip_queue_peek(
    const ArxInterchipQueue *q,
    const ArxInterchipFrame **frame
) {
    if (!q || !frame || q->count == 0u) return false;
    *frame = &q->items[q->head];
    return true;
}

void arx_interchip_queue_commit(ArxInterchipQueue *q) {
    if (!q || q->count == 0u) return;
    q->head = (uint8_t)((q->head + 1u) % ARX_INTERCHIP_QUEUE_SIZE);
    q->count--;
}

void arx_interchip_note_master_request(
    ArxInterchip *link,
    uint32_t now_ms
) {
    if (link) link->last_master_request_ms = now_ms;
}

bool arx_interchip_tx_allowed(
    const ArxInterchip *link,
    uint32_t now_ms
) {
    if (!link || now_ms < link->boot_ignore_ms) return false;

    if (link->role == ARX_IC_ROLE_C1) {
        return (uint32_t)(now_ms - link->last_tx_ms) >
               link->master_tx_period_ms;
    }

    return (uint32_t)(now_ms - link->last_master_request_ms) <
           link->slave_reply_window_ms;
}
