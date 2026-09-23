#ifndef ARX_INTERCHIP_H
#define ARX_INTERCHIP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_INTERCHIP_FRAME_SIZE 19u
#define ARX_INTERCHIP_QUEUE_SIZE 10u
#define ARX_INTERCHIP_PAD        0x20u

enum {
    ARX_IC_TO_C1                = 0x01u,
    ARX_IC_TO_C2                = 0x02u,
    ARX_IC_TO_BH_PARAM_TEXT     = 0x03u,
    ARX_IC_ALL_SLEEP            = 0x04u,
    ARX_IC_C2_SLEEP_ACK         = 0x05u,
    ARX_IC_BH_SLEEP_ACK         = 0x06u,
    ARX_IC_ALL_CLEAR_DTC        = 0x07u,
    ARX_IC_BH_GET_STATUS        = 0x08u,
    ARX_IC_BH_CHIME             = 0x09u,
    ARX_IC_TO_BH                = 0x0Au,
    ARX_IC_TO_C2_BH             = 0x0Bu,
    ARX_IC_TO_C1_BH             = 0x0Cu,
    ARX_IC_TO_C1_C2             = 0x0Du
};

enum {
    ARX_IC_C1_LANE_SINGLE       = 0x1Fu,
    ARX_IC_C1_LANE_DOUBLE       = 0x20u,
    ARX_IC_C1_BRAKE_NORMAL      = 0x21u,
    ARX_IC_C1_BRAKE_FORCE       = 0x22u,
    ARX_IC_C1_USB_C2_ON         = 0x23u,
    ARX_IC_C1_USB_C2_OFF        = 0x24u,
    ARX_IC_C1_DYNO_ON           = 0x25u,
    ARX_IC_C1_DYNO_OFF          = 0x26u,
    ARX_IC_C1_USB_BH_ON         = 0x27u,
    ARX_IC_C1_USB_BH_OFF        = 0x28u,

    ARX_IC_C2_DYNO_TOGGLE       = 0x20u,
    ARX_IC_C2_BRAKE_NORMAL      = 0x21u,
    ARX_IC_C2_BRAKE_FORCE       = 0x22u,
    ARX_IC_C2_GET_STATUS        = 0x23u,
    ARX_IC_C2_ESC_TC_TOGGLE     = 0x24u,
    ARX_IC_C2_RACE_MASK_DEFAULT = 0x27u,
    ARX_IC_C2_RACE_MASK_SHOW    = 0x28u,
    ARX_IC_C2_HAS_TOGGLE        = 0x29u,
    ARX_IC_C2_PDC_MUTE_OFF      = 0x2Bu,
    ARX_IC_C2_PDC_MUTE_ON       = 0x2Cu,

    ARX_IC_BH_ODO_MASK_ON       = 0x20u,
    ARX_IC_BH_ODO_MASK_OFF      = 0x21u,
    ARX_IC_BH_MIRROR_OFF        = 0x22u,
    ARX_IC_BH_MIRROR_ON         = 0x23u,
    ARX_IC_BH_MIRROR_STORE      = 0x24u,

    ARX_IC_SHARED_PEDAL_MODE    = 0x39u,
    ARX_IC_SHARED_HAS_OFF       = 0x3Au,
    ARX_IC_SHARED_HAS_ON        = 0x3Bu,
    ARX_IC_SHARED_ESC_TC_OFF    = 0x3Cu,
    ARX_IC_SHARED_ESC_TC_ON     = 0x3Du,
    ARX_IC_SHARED_SAVE_LOG      = 0x3Eu,
    ARX_IC_SHARED_SNIFFER_OFF   = 0x3Fu,
    ARX_IC_SHARED_SNIFFER_ON    = 0x40u,

    ARX_IC_C1_BH_RACE_SHOW      = 0x40u,
    ARX_IC_C1_BH_RACE_HIDE      = 0x41u,

    ARX_IC_C1_C2_LANE_DOUBLE    = 0x50u
};

typedef struct {
    uint8_t bytes[ARX_INTERCHIP_FRAME_SIZE];
} ArxInterchipFrame;

typedef struct {
    ArxInterchipFrame items[ARX_INTERCHIP_QUEUE_SIZE];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    uint32_t dropped;
} ArxInterchipQueue;

typedef enum {
    ARX_IC_ROLE_C1 = 0,
    ARX_IC_ROLE_C2,
    ARX_IC_ROLE_BH
} ArxInterchipRole;

typedef struct {
    ArxInterchipRole role;
    ArxInterchipQueue tx;

    uint32_t boot_ignore_ms;
    uint32_t master_tx_period_ms;
    uint32_t slave_reply_window_ms;
    uint32_t c2_status_period_ms;
    uint32_t bh_status_period_ms;

    uint32_t last_tx_ms;
    uint32_t last_master_request_ms;
    uint32_t last_c2_status_request_ms;
    uint32_t last_bh_status_request_ms;
} ArxInterchip;

void arx_interchip_init(ArxInterchip *link, ArxInterchipRole role);

void arx_interchip_frame_build(
    ArxInterchipFrame *frame,
    const uint8_t *payload,
    size_t payload_len
);

bool arx_interchip_queue_push(
    ArxInterchipQueue *q,
    const ArxInterchipFrame *frame
);

bool arx_interchip_queue_peek(
    const ArxInterchipQueue *q,
    const ArxInterchipFrame **frame
);

void arx_interchip_queue_commit(ArxInterchipQueue *q);

void arx_interchip_note_master_request(
    ArxInterchip *link,
    uint32_t now_ms
);

bool arx_interchip_tx_allowed(
    const ArxInterchip *link,
    uint32_t now_ms
);

#endif
