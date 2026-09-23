#ifndef ARX_ISOTP_H
#define ARX_ISOTP_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_ISOTP_MAX_PAYLOAD 255u
#define ARX_ISOTP_PAD_BYTE    0xAAu

typedef enum {
    ARX_ISOTP_TX_IDLE = 0,
    ARX_ISOTP_TX_WAIT_FC,
    ARX_ISOTP_TX_SEND_CF,
    ARX_ISOTP_TX_COMPLETE,
    ARX_ISOTP_TX_ERROR
} ArxIsoTpTxState;

typedef struct {
    ArxIsoTpTxState state;
    ArxBus bus;
    uint32_t can_id;
    bool extended_id;
    bool variable_dlc;

    uint8_t payload[ARX_ISOTP_MAX_PAYLOAD];
    uint16_t length;
    uint16_t offset;

    uint8_t sequence;
    uint8_t block_size;
    uint8_t block_sent;
    uint8_t st_min_ms;
    uint32_t last_tx_ms;
} ArxIsoTpTx;

typedef enum {
    ARX_ISOTP_RX_IDLE = 0,
    ARX_ISOTP_RX_RECEIVING,
    ARX_ISOTP_RX_COMPLETE,
    ARX_ISOTP_RX_ERROR
} ArxIsoTpRxState;

typedef enum {
    ARX_ISOTP_RX_EVENT_NONE = 0,
    ARX_ISOTP_RX_EVENT_NEED_FLOW_CONTROL,
    ARX_ISOTP_RX_EVENT_COMPLETE,
    ARX_ISOTP_RX_EVENT_ERROR
} ArxIsoTpRxEvent;

typedef struct {
    ArxIsoTpRxState state;
    uint32_t source_id;
    bool source_extended;

    uint8_t payload[ARX_ISOTP_MAX_PAYLOAD];
    uint16_t total_length;
    uint16_t received;
    uint8_t next_sequence;
    uint8_t block_size;
    uint8_t block_received;
} ArxIsoTpRx;

void arx_isotp_tx_init(ArxIsoTpTx *tx);
void arx_isotp_rx_init(ArxIsoTpRx *rx);
void arx_isotp_rx_set_block_size(ArxIsoTpRx *rx, uint8_t block_size);

bool arx_isotp_tx_start(
    ArxIsoTpTx *tx,
    ArxBus bus,
    uint32_t can_id,
    bool extended_id,
    const uint8_t *payload,
    uint16_t length,
    bool variable_dlc,
    uint32_t now_ms,
    ArxCanFrame *first_frame
);

bool arx_isotp_tx_on_flow_control(
    ArxIsoTpTx *tx,
    const ArxCanFrame *fc,
    uint32_t now_ms
);

bool arx_isotp_tx_next(
    ArxIsoTpTx *tx,
    uint32_t now_ms,
    ArxCanFrame *out
);

ArxIsoTpRxEvent arx_isotp_rx_feed(
    ArxIsoTpRx *rx,
    const ArxCanFrame *frame
);

bool arx_isotp_build_flow_control(
    ArxBus bus,
    uint32_t can_id,
    bool extended_id,
    const uint8_t *custom_data,
    uint8_t custom_length,
    uint32_t now_ms,
    ArxCanFrame *out
);

#endif
