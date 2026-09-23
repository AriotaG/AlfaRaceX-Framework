#ifndef ARX_ELM327_H
#define ARX_ELM327_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_ELM_ID_STRING       "ELM327 v1.4"
#define ARX_ELM_DESCRIPTION     "OBDII to RS232 Interpreter"
#define ARX_ELM_DEFAULT_TIMEOUT 200u
#define ARX_ELM_FC_TIMEOUT      250u
#define ARX_ELM_PROBE_TIMEOUT   200u
#define ARX_ELM_IDLE_EXIT_MS    120000u
#define ARX_ELM_PAD_BYTE        0xAAu
#define ARX_ELM_MAX_PAYLOAD     255u

typedef enum {
    ARX_ELM_PROTOCOL_11_500=6,
    ARX_ELM_PROTOCOL_29_500=7,
    ARX_ELM_PROTOCOL_11_250=8,
    ARX_ELM_PROTOCOL_29_250=9,
    ARX_ELM_PROTOCOL_J1939_250=10,
    ARX_ELM_PROTOCOL_USER1=11,
    ARX_ELM_PROTOCOL_USER2=12
} ArxElmProtocol;

typedef struct {
    uint32_t can_id;
    bool extended_id;
    uint8_t data[ARX_ELM_MAX_PAYLOAD];
    uint16_t length;
    bool auto_format;
    bool auto_flow_control;
} ArxElmRequest;

typedef struct {
    bool enabled;
    bool echo;
    bool headers;
    bool spaces;
    bool linefeeds;
    bool variable_dlc;
    bool auto_format;
    bool auto_flow_control;
    bool allow_long;
    bool monitor_all;

    uint8_t adaptive_timing;
    uint16_t timeout_ms;

    ArxElmProtocol protocol;
    bool protocol_auto;
    uint8_t bitrate_divisor;
    uint8_t can_priority;

    /* User protocol programmable parameters. */
    uint8_t pp2c;
    uint8_t pp2d;
    uint8_t pp2e;
    uint8_t pp2f;

    uint32_t tx_header;
    bool tx_extended;

    uint32_t filter_value;
    uint32_t filter_mask;

    uint32_t fc_header;
    bool fc_extended;
    uint8_t fc_data[8];
    uint8_t fc_length;
    uint8_t fc_mode;

    uint32_t commands;
} ArxElm327;

void arx_elm327_init(ArxElm327 *f);
size_t arx_elm327_command(ArxElm327 *f, const char *command, char *reply, size_t reply_capacity);
bool arx_elm327_prepare_request(const ArxElm327 *f, const char *hex_command, ArxElmRequest *request);
bool arx_elm327_filter_accept(const ArxElm327 *f, uint32_t can_id);
uint16_t arx_elm327_effective_bitrate_kbps(const ArxElm327 *f);

#endif
