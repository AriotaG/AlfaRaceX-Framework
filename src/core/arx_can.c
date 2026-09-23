#include "arx/arx_can.h"
#include <string.h>

static ArxTxQueue *queue_for(ArxCanTransport *transport, ArxPriority priority) {
    if (!transport || priority >= ARX_PRIORITY_COUNT) {
        return NULL;
    }
    return &transport->queues[priority];
}

void arx_can_init(ArxCanTransport *transport) {
    if (!transport) {
        return;
    }
    memset(transport, 0, sizeof(*transport));
}

ArxStatus arx_can_enqueue(
    ArxCanTransport *transport,
    const ArxCanFrame *frame,
    ArxPriority priority,
    uint8_t retries,
    uint32_t deadline_ms
) {
    if (!transport || !frame || frame->dlc > 8u) {
        return ARX_STATUS_INVALID;
    }

    ArxTxQueue *q = queue_for(transport, priority);
    if (!q) {
        return ARX_STATUS_INVALID;
    }
    if (q->count >= ARX_CAN_QUEUE_CAPACITY) {
        transport->tx_dropped++;
        return ARX_STATUS_FULL;
    }

    ArxTxItem *slot = &q->items[q->head];
    slot->frame = *frame;
    slot->priority = priority;
    slot->retries_left = retries;
    slot->deadline_ms = deadline_ms;

    q->head = (uint8_t)((q->head + 1u) % ARX_CAN_QUEUE_CAPACITY);
    q->count++;
    transport->tx_enqueued++;
    return ARX_STATUS_OK;
}

static ArxStatus process_queue(
    ArxCanTransport *transport,
    ArxTxQueue *q,
    uint32_t now_ms,
    ArxCanSendFn sender,
    void *user
) {
    if (q->count == 0u) {
        return ARX_STATUS_EMPTY;
    }

    ArxTxItem *item = &q->items[q->tail];

    if (item->deadline_ms != 0u && now_ms > item->deadline_ms) {
        q->tail = (uint8_t)((q->tail + 1u) % ARX_CAN_QUEUE_CAPACITY);
        q->count--;
        transport->tx_expired++;
        transport->tx_dropped++;
        return ARX_STATUS_EXPIRED;
    }

    ArxStatus status = sender(&item->frame, user);
    if (status == ARX_STATUS_OK) {
        q->tail = (uint8_t)((q->tail + 1u) % ARX_CAN_QUEUE_CAPACITY);
        q->count--;
        transport->tx_sent++;
        return ARX_STATUS_OK;
    }

    if (item->retries_left > 0u) {
        item->retries_left--;
        transport->tx_retry_events++;
        return status;
    }

    q->tail = (uint8_t)((q->tail + 1u) % ARX_CAN_QUEUE_CAPACITY);
    q->count--;
    transport->tx_dropped++;
    return status;
}

ArxStatus arx_can_process_one(
    ArxCanTransport *transport,
    uint32_t now_ms,
    ArxCanSendFn sender,
    void *user
) {
    if (!transport || !sender) {
        return ARX_STATUS_INVALID;
    }

    for (unsigned p = 0; p < ARX_PRIORITY_COUNT; ++p) {
        if (transport->queues[p].count != 0u) {
            return process_queue(transport, &transport->queues[p], now_ms, sender, user);
        }
    }
    return ARX_STATUS_EMPTY;
}
