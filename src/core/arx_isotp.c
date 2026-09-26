#include "arx/arx_isotp.h"
#include <string.h>

void arx_isotp_tx_init(ArxIsoTpTx *tx) {
    if (!tx) return;
    memset(tx, 0, sizeof(*tx));
}

void arx_isotp_rx_init(ArxIsoTpRx *rx) {
    if (!rx) return;
    memset(rx, 0, sizeof(*rx));
}

void arx_isotp_rx_set_block_size(ArxIsoTpRx *rx, uint8_t block_size) {
    if (!rx) return;
    rx->block_size = block_size;
    rx->block_received = 0u;
}

static void frame_base(
    ArxCanFrame *f,
    ArxBus bus,
    uint32_t id,
    bool ext,
    uint8_t dlc,
    uint32_t now_ms
) {
    memset(f, 0, sizeof(*f));
    f->bus = bus;
    f->id = id;
    f->extended_id = ext;
    f->dlc = dlc;
    f->timestamp_ms = now_ms;
    memset(f->data, ARX_ISOTP_PAD_BYTE, sizeof(f->data));
}

bool arx_isotp_tx_start(
    ArxIsoTpTx *tx,
    ArxBus bus,
    uint32_t can_id,
    bool extended_id,
    const uint8_t *payload,
    uint16_t length,
    bool variable_dlc,
    uint32_t now_ms,
    ArxCanFrame *out
) {
    if (!tx || !payload || !out || length == 0u || length > ARX_ISOTP_MAX_PAYLOAD) {
        return false;
    }

    arx_isotp_tx_init(tx);
    tx->bus = bus;
    tx->can_id = can_id;
    tx->extended_id = extended_id;
    tx->variable_dlc = variable_dlc;
    tx->length = length;
    memcpy(tx->payload, payload, length);

    if (length <= 7u) {
        const uint8_t dlc = variable_dlc ? (uint8_t)(length + 1u) : 8u;
        frame_base(out, bus, can_id, extended_id, dlc, now_ms);
        out->data[0] = (uint8_t)length;
        memcpy(&out->data[1], payload, length);

        tx->offset = length;
        tx->last_tx_ms = now_ms;
        tx->state = ARX_ISOTP_TX_COMPLETE;
        return true;
    }

    frame_base(out, bus, can_id, extended_id, 8u, now_ms);
    out->data[0] = (uint8_t)(0x10u | ((length >> 8u) & 0x0Fu));
    out->data[1] = (uint8_t)length;
    memcpy(&out->data[2], payload, 6u);

    tx->offset = 6u;
    tx->sequence = 1u;
    tx->last_tx_ms = now_ms;
    tx->state = ARX_ISOTP_TX_WAIT_FC;
    return true;
}

bool arx_isotp_tx_on_flow_control(
    ArxIsoTpTx *tx,
    const ArxCanFrame *fc,
    uint32_t now_ms
) {
    if (!tx || !fc || tx->state != ARX_ISOTP_TX_WAIT_FC || fc->dlc < 3u ||
        fc->dlc > 8u || fc->bus != tx->bus) {
        return false;
    }
    if ((fc->data[0] & 0xF0u) != 0x30u) return false;

    const uint8_t fs = (uint8_t)(fc->data[0] & 0x0Fu);
    if (fs == 0u) {
        tx->block_size = fc->data[1];
        tx->block_sent = 0u;
        tx->st_min_ms = (fc->data[2] > 127u) ? 1u : fc->data[2];
        tx->last_tx_ms = now_ms;
        tx->state = ARX_ISOTP_TX_SEND_CF;
        return true;
    }

    if (fs == 1u) {
        /* WAIT: restart the FC wait budget in the caller. */
        tx->last_tx_ms = now_ms;
        return true;
    }

    tx->state = ARX_ISOTP_TX_ERROR; /* OVFLW or reserved flow status. */
    return false;
}

bool arx_isotp_tx_next(
    ArxIsoTpTx *tx,
    uint32_t now_ms,
    ArxCanFrame *out
) {
    if (!tx || !out || tx->state != ARX_ISOTP_TX_SEND_CF) return false;
    if (tx->st_min_ms && (uint32_t)(now_ms - tx->last_tx_ms) < tx->st_min_ms) return false;

    frame_base(out, tx->bus, tx->can_id, tx->extended_id, 8u, now_ms);
    out->data[0] = (uint8_t)(0x20u | (tx->sequence & 0x0Fu));

    uint16_t remaining = (uint16_t)(tx->length - tx->offset);
    uint8_t n = remaining > 7u ? 7u : (uint8_t)remaining;
    memcpy(&out->data[1], &tx->payload[tx->offset], n);

    tx->offset = (uint16_t)(tx->offset + n);
    tx->sequence = (uint8_t)((tx->sequence + 1u) & 0x0Fu);
    tx->block_sent++;
    tx->last_tx_ms = now_ms;

    if (tx->offset >= tx->length) {
        tx->state = ARX_ISOTP_TX_COMPLETE;
    } else if (tx->block_size != 0u && tx->block_sent >= tx->block_size) {
        tx->state = ARX_ISOTP_TX_WAIT_FC;
        tx->block_sent = 0u;
    }

    return true;
}

ArxIsoTpRxEvent arx_isotp_rx_feed(
    ArxIsoTpRx *rx,
    const ArxCanFrame *frame
) {
    if (!rx || !frame) return ARX_ISOTP_RX_EVENT_ERROR;
    if (rx->state == ARX_ISOTP_RX_RECEIVING &&
        (frame->bus != rx->source_bus || frame->id != rx->source_id ||
         frame->extended_id != rx->source_extended)) return ARX_ISOTP_RX_EVENT_NONE;
    if (frame->dlc == 0u || frame->dlc > 8u) {
        rx->state = ARX_ISOTP_RX_ERROR;
        return ARX_ISOTP_RX_EVENT_ERROR;
    }

    const uint8_t pci = frame->data[0];
    const uint8_t type = (uint8_t)(pci & 0xF0u);

    if (type == 0x00u) {
        uint8_t len = (uint8_t)(pci & 0x0Fu);
        if (len == 0u || len > 7u || (uint8_t)(len + 1u) > frame->dlc) {
            rx->state = ARX_ISOTP_RX_ERROR;
            return ARX_ISOTP_RX_EVENT_ERROR;
        }

        rx->source_bus = frame->bus;
        rx->source_id = frame->id;
        rx->source_extended = frame->extended_id;
        rx->total_length = len;
        rx->received = len;
        memcpy(rx->payload, &frame->data[1], len);
        rx->state = ARX_ISOTP_RX_COMPLETE;
        return ARX_ISOTP_RX_EVENT_COMPLETE;
    }

    if (type == 0x10u) {
        if (frame->dlc < 8u) {
            rx->state = ARX_ISOTP_RX_ERROR;
            return ARX_ISOTP_RX_EVENT_ERROR;
        }

        uint16_t total = (uint16_t)(((uint16_t)(pci & 0x0Fu) << 8u) | frame->data[1]);
        if (total <= 7u || total > ARX_ISOTP_MAX_PAYLOAD) {
            rx->state = ARX_ISOTP_RX_ERROR;
            return ARX_ISOTP_RX_EVENT_ERROR;
        }
        rx->source_bus = frame->bus;
        rx->source_id = frame->id;
        rx->source_extended = frame->extended_id;
        rx->total_length = total;
        rx->received = total < 6u ? total : 6u;
        memcpy(rx->payload, &frame->data[2], rx->received);
        rx->next_sequence = 1u;
        rx->block_received = 0u;
        rx->state = ARX_ISOTP_RX_RECEIVING;

        if (rx->received >= rx->total_length) {
            rx->state = ARX_ISOTP_RX_COMPLETE;
            return ARX_ISOTP_RX_EVENT_COMPLETE;
        }
        return ARX_ISOTP_RX_EVENT_NEED_FLOW_CONTROL;
    }

    if (type == 0x20u) {
        if (rx->state != ARX_ISOTP_RX_RECEIVING) {
            rx->state = ARX_ISOTP_RX_ERROR;
            return ARX_ISOTP_RX_EVENT_ERROR;
        }

        const uint8_t sn = (uint8_t)(pci & 0x0Fu);
        if (sn != (rx->next_sequence & 0x0Fu)) {
            rx->state = ARX_ISOTP_RX_ERROR;
            return ARX_ISOTP_RX_EVENT_ERROR;
        }

        rx->next_sequence = (uint8_t)((rx->next_sequence + 1u) & 0x0Fu);

        uint16_t remaining = (uint16_t)(rx->total_length - rx->received);
        uint8_t n = remaining > 7u ? 7u : (uint8_t)remaining;
        if ((uint8_t)(n + 1u) > frame->dlc) {
            rx->state = ARX_ISOTP_RX_ERROR;
            return ARX_ISOTP_RX_EVENT_ERROR;
        }

        memcpy(&rx->payload[rx->received], &frame->data[1], n);
        rx->received = (uint16_t)(rx->received + n);
        if (rx->block_size != 0u) rx->block_received++;

        if (rx->received >= rx->total_length) {
            rx->state = ARX_ISOTP_RX_COMPLETE;
            return ARX_ISOTP_RX_EVENT_COMPLETE;
        }
        if (rx->block_size != 0u && rx->block_received >= rx->block_size) {
            rx->block_received = 0u;
            return ARX_ISOTP_RX_EVENT_NEED_FLOW_CONTROL;
        }
        return ARX_ISOTP_RX_EVENT_NONE;
    }

    /* Flow-control frames are consumed by the TX state machine, not RX. */
    return ARX_ISOTP_RX_EVENT_NONE;
}

bool arx_isotp_build_flow_control(
    ArxBus bus,
    uint32_t can_id,
    bool extended_id,
    const uint8_t *custom_data,
    uint8_t custom_length,
    uint32_t now_ms,
    ArxCanFrame *out
) {
    if (!out) return false;
    frame_base(out, bus, can_id, extended_id, 8u, now_ms);

    if (custom_data && custom_length) {
        if (custom_length > 8u) custom_length = 8u;
        memcpy(out->data, custom_data, custom_length);
    } else {
        out->data[0] = 0x30u;
        out->data[1] = 0x00u;
        out->data[2] = 0x00u;
    }
    return true;
}
