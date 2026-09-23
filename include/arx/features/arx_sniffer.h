#ifndef ARX_SNIFFER_H
#define ARX_SNIFFER_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_SNIFFER_FRAME_SIZE       16u
#define ARX_SNIFFER_FRAME_COUNT      16u
#define ARX_SNIFFER_BUFFER_SIZE      (ARX_SNIFFER_FRAME_SIZE * ARX_SNIFFER_FRAME_COUNT)
#define ARX_SNIFFER_USB_CHUNK        64u
#define ARX_SNIFFER_FLUSH_TIMEOUT_MS 20u
#define ARX_SNIFFER_START_NIBBLE     0xA0u
#define ARX_SNIFFER_OVERFLOW_MARKER  0xAFu

typedef struct {
    bool enabled;
    bool in_use;

    uint8_t ring[ARX_SNIFFER_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;

    uint16_t dropped_pending;
    uint32_t captured;
    uint32_t dropped_total;
    uint32_t last_flush_ms;
} ArxSniffer;

void arx_sniffer_init(ArxSniffer *f);
void arx_sniffer_start(ArxSniffer *f, uint32_t now_ms);
void arx_sniffer_stop(ArxSniffer *f);

bool arx_sniffer_push(ArxSniffer *f, const ArxCanFrame *frame);

/* Copies one contiguous USB-ready chunk without consuming it. */
size_t arx_sniffer_peek_chunk(
    ArxSniffer *f,
    uint32_t now_ms,
    uint8_t *out,
    size_t out_capacity
);

/* Call only after the transport accepted the chunk. */
void arx_sniffer_commit_chunk(ArxSniffer *f, size_t length, uint32_t now_ms);

#endif
