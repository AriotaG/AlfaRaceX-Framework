#ifndef ARX_ELM_TRANSPORT_H
#define ARX_ELM_TRANSPORT_H

#include "arx/arx_isotp.h"
#include "arx/features/arx_elm327.h"
#include <stdbool.h>
#include <stdint.h>

#define ARX_ELM_ROUTE_CACHE_LEN 16u

typedef enum {
    ARX_ELM_BUS_C1 = 0,
    ARX_ELM_BUS_C2,
    ARX_ELM_BUS_BH
} ArxElmBus;

typedef struct {
    uint16_t address;
    ArxElmBus bus;
} ArxElmRouteEntry;

typedef struct {
    ArxElmRouteEntry entries[ARX_ELM_ROUTE_CACHE_LEN];
    uint8_t count;
} ArxElmRouter;

typedef enum {
    ARX_ELM_RX_NONE = 0,
    ARX_ELM_RX_RAW_FRAME,
    ARX_ELM_RX_PAYLOAD,
    ARX_ELM_RX_PENDING,
    ARX_ELM_RX_NEED_FLOW_CONTROL,
    ARX_ELM_RX_ERROR
} ArxElmRxEventType;

typedef struct {
    ArxElmRxEventType type;
    uint32_t can_id;
    bool extended_id;
    uint8_t data[ARX_ISOTP_MAX_PAYLOAD];
    uint16_t length;
} ArxElmRxEvent;

typedef struct {
    bool active;
    bool raw_mode;
    bool auto_flow_control;
    uint8_t expected_responses;
    uint8_t received_responses;
    bool saw_response_pending;

    ArxBus bus;
    ArxIsoTpTx tx;
    ArxIsoTpRx rx;
    uint8_t rx_block_size;
    uint8_t rx_st_min_ms;
} ArxElmTransaction;

void arx_elm_router_init(ArxElmRouter *router);
uint16_t arx_elm_target_address(const ArxElm327 *cfg);

uint8_t arx_elm_router_candidates(
    const ArxElmRouter *router,
    const ArxElm327 *cfg,
    ArxElmBus out[3]
);

void arx_elm_router_remember(
    ArxElmRouter *router,
    const ArxElm327 *cfg,
    ArxElmBus bus
);


bool arx_elm_response_filter(
    const ArxElm327 *cfg,
    uint32_t *value,
    uint32_t *mask,
    bool *extended_id
);

void arx_elm_transaction_init(ArxElmTransaction *tx);

bool arx_elm_transaction_start(
    ArxElmTransaction *tx,
    const ArxElm327 *cfg,
    ArxBus bus,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t expected_responses,
    uint32_t now_ms,
    ArxCanFrame *first_tx
);

bool arx_elm_transaction_on_flow_control(
    ArxElmTransaction *tx,
    const ArxCanFrame *frame,
    uint32_t now_ms
);

bool arx_elm_transaction_next_tx(
    ArxElmTransaction *tx,
    uint32_t now_ms,
    ArxCanFrame *out
);

ArxElmRxEventType arx_elm_transaction_on_rx(
    ArxElmTransaction *tx,
    const ArxElm327 *cfg,
    const ArxCanFrame *frame,
    ArxElmRxEvent *event
);

bool arx_elm_transaction_build_flow_control(
    const ArxElmTransaction *tx,
    const ArxElm327 *cfg,
    uint32_t now_ms,
    ArxCanFrame *out
);

#endif
