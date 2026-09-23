#ifndef ARX_CAN_H
#define ARX_CAN_H

#include "arx/arx_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ARX_CAN_QUEUE_CAPACITY 32u

typedef struct {
    ArxCanFrame frame;
    ArxPriority priority;
    uint8_t retries_left;
    uint32_t deadline_ms;
} ArxTxItem;

typedef struct {
    ArxTxItem items[ARX_CAN_QUEUE_CAPACITY];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} ArxTxQueue;

typedef struct {
    ArxTxQueue queues[ARX_PRIORITY_COUNT];
    uint32_t tx_enqueued;
    uint32_t tx_sent;
    uint32_t tx_dropped;
    uint32_t tx_expired;
    uint32_t tx_retry_events;
} ArxCanTransport;

typedef ArxStatus (*ArxCanSendFn)(const ArxCanFrame *frame, void *user);

void arx_can_init(ArxCanTransport *transport);
ArxStatus arx_can_enqueue(
    ArxCanTransport *transport,
    const ArxCanFrame *frame,
    ArxPriority priority,
    uint8_t retries,
    uint32_t deadline_ms
);
ArxStatus arx_can_process_one(
    ArxCanTransport *transport,
    uint32_t now_ms,
    ArxCanSendFn sender,
    void *user
);

#ifdef __cplusplus
}
#endif

#endif
