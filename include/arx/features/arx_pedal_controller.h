#ifndef ARX_PEDAL_CONTROLLER_H
#define ARX_PEDAL_CONTROLLER_H

#include "arx/arx_vehicle_state.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_PEDAL_PACKET_SIZE 9u
#define ARX_PEDAL_UART_BAUD   9600u

typedef enum {
    ARX_PEDAL_DISABLED = 0,
    ARX_PEDAL_AUTO = 1,
    ARX_PEDAL_BYPASS = 2,
    ARX_PEDAL_ALL_WEATHER = 3,
    ARX_PEDAL_NATURAL = 4,
    ARX_PEDAL_DYNAMIC = 5,
    ARX_PEDAL_RACE = 6,
    ARX_PEDAL_HYBRID_ALIGN = 7,
    ARX_PEDAL_KIDS_LIMITER = 8
} ArxPedalMode;

typedef enum {
    ARX_PEDAL_MAP_UNKNOWN = 0,
    ARX_PEDAL_MAP_BYPASS,
    ARX_PEDAL_MAP_ALL_WEATHER,
    ARX_PEDAL_MAP_NATURAL,
    ARX_PEDAL_MAP_DYNAMIC,
    ARX_PEDAL_MAP_RACE
} ArxPedalMap;

typedef struct {
    ArxPedalMode mode;
    int8_t power;                 /* -10 .. +10 */
    ArxPedalMap applied_map;

    uint32_t last_tx_ms;
    uint32_t retry_interval_ms;
    uint32_t tx_count;
    uint32_t reply_count;
    uint32_t mismatch_count;
} ArxPedalController;

void arx_pedal_init(ArxPedalController *f);
bool arx_pedal_set_mode(ArxPedalController *f, ArxPedalMode mode);
bool arx_pedal_set_power(ArxPedalController *f, int8_t power);

ArxPedalMap arx_pedal_target_map(const ArxPedalController *f, ArxDnaMode dna_mode);
ArxPedalMap arx_pedal_parse_reply(uint8_t reply_byte);
void arx_pedal_on_reply(ArxPedalController *f, uint8_t reply_byte);

bool arx_pedal_needs_sync(
    const ArxPedalController *f,
    ArxDnaMode dna_mode,
    bool engine_running,
    uint32_t now_ms
);

bool arx_pedal_build_map_packet(
    const ArxPedalController *f,
    ArxPedalMap map,
    uint8_t out[ARX_PEDAL_PACKET_SIZE]
);

bool arx_pedal_build_sync_packet(
    ArxPedalController *f,
    ArxDnaMode dna_mode,
    bool engine_running,
    uint32_t now_ms,
    uint8_t out[ARX_PEDAL_PACKET_SIZE]
);

/* Returns true when limiter override is required. */
bool arx_pedal_kids_override_required(
    const ArxPedalController *f,
    bool diesel,
    uint16_t rpm,
    float speed_kmh
);

/* Builds the 0% accelerator override packet used by the limiter mode. */
bool arx_pedal_build_zero_override(
    const ArxPedalController *f,
    uint8_t out[ARX_PEDAL_PACKET_SIZE]
);

#endif
