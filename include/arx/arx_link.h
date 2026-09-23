#ifndef ARX_LINK_H
#define ARX_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_LINK_FRAME_SIZE 19u

enum {
    ARX_LINK_TO_C2     = 0x0Eu,
    ARX_LINK_TO_BH     = 0x0Fu,
    ARX_LINK_TO_MASTER = 0x10u
};

typedef enum {
    ARX_LINK_REQ   = 0x01,
    ARX_LINK_CFG   = 0x02,
    ARX_LINK_RSP   = 0x03,
    ARX_LINK_END   = 0x04,
    ARX_LINK_FCCFG = 0x05,
    ARX_LINK_ARM   = 0x06
} ArxLinkType;

enum {
    ARX_LINK_FLAG_EXTID  = 0x01u,
    ARX_LINK_FLAG_NODATA = 0x02u,
    ARX_LINK_FLAG_ARM_ON = 0x01u,
    ARX_LINK_CFG_AUTOFC  = 0x01u
};

typedef struct {
    uint8_t raw[ARX_LINK_FRAME_SIZE];
} ArxLinkFrame;

uint8_t arx_link_checksum(const uint8_t raw[ARX_LINK_FRAME_SIZE]);
bool arx_link_validate(const ArxLinkFrame *f);

void arx_link_build_can(
    ArxLinkFrame *f,
    uint8_t destination,
    ArxLinkType type,
    bool extended_id,
    uint32_t can_id,
    const uint8_t *data,
    uint8_t dlc,
    uint8_t sequence
);

void arx_link_build_config(
    ArxLinkFrame *f,
    uint8_t destination,
    uint32_t filter_value,
    uint32_t filter_mask,
    uint16_t timeout_ms,
    bool auto_flow_control,
    uint8_t sequence
);

void arx_link_build_flow_control_config(
    ArxLinkFrame *f,
    uint8_t destination,
    uint32_t can_id,
    bool extended_id,
    const uint8_t *data,
    uint8_t length,
    uint8_t sequence
);

void arx_link_build_arm(ArxLinkFrame *f, uint8_t destination, bool enabled, uint8_t sequence);
void arx_link_build_end(ArxLinkFrame *f, bool no_data, uint8_t sequence);

uint32_t arx_link_can_id(const ArxLinkFrame *f);
uint8_t arx_link_dlc(const ArxLinkFrame *f);
const uint8_t *arx_link_data(const ArxLinkFrame *f);

#endif
