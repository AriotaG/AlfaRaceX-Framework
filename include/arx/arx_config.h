#ifndef ARX_CONFIG_H
#define ARX_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#define ARX_CONFIG_MAGIC   0x41525843u /* ARXC */
#define ARX_CONFIG_VERSION 2u
#define ARX_CONFIG_LEGACY_SLOT_COUNT 32u

typedef enum {
    ARX_CFG_PEDAL_DISABLED = 0,
    ARX_CFG_PEDAL_AUTO = 1,
    ARX_CFG_PEDAL_BYPASS = 2,
    ARX_CFG_PEDAL_ALL_WEATHER = 3,
    ARX_CFG_PEDAL_NATURAL = 4,
    ARX_CFG_PEDAL_DYNAMIC = 5,
    ARX_CFG_PEDAL_RACE = 6,
    ARX_CFG_PEDAL_HYBRID = 7,
    ARX_CFG_PEDAL_LIMITER = 8
} ArxConfigPedalMode;

typedef struct {
    bool immobilizer_enabled;
    bool smart_start_stop_enabled;
    bool led_strip_enabled;
    bool shift_indicator_enabled;
    uint16_t shift_threshold_rpm;
    bool ipc_my23;
    bool route_messages_enabled;
    bool dyno_enabled;
    bool acc_virtual_pad_enabled;
    bool front_brake_override_enabled;
    bool awd_control_enabled;

    bool clear_faults_enabled;
    bool esc_tc_customizer_enabled;
    bool read_faults_enabled;
    bool diesel_profile;
    bool regeneration_alert_enabled;
    uint16_t launch_torque_threshold_nm;
    bool seatbelt_alarm_enabled;

    uint8_t pedal_mode;
    bool odometer_blink_mask_enabled;
    bool race_mask_enabled;
    bool park_mirror_enabled;
    uint8_t acc_autostart_mode;       /* 0 off, 1 RES, 2 + */
    uint8_t close_windows_mode;       /* 0..2 */
    uint8_t open_windows_mode;        /* 0..2 */
    bool has_virtual_pad_enabled;
    bool exhaust_flap_enabled;
    int8_t pedal_power;               /* -10..+10 */
    bool front_park_mute_enabled;
    bool sniffer_enabled;
    bool elm327_enabled;

    /* ARX-native controls, not part of the legacy 32-slot map. */
    bool telemetry_enabled;
    bool diagnostics_enabled;
    bool sgw_detection_enabled;
    /*
     * Reserved for the MY20 IPC-gated Dynamic shift experiment.
     * Intentionally not consumed by the stable runtime until the display gate
     * is identified and physically validated.
     */
    bool dynamic_shift_in_dynamic_enabled;
} ArxRuntimeConfig;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t payload_size;
    uint32_t generation;
    ArxRuntimeConfig payload;
    uint32_t crc32;
} ArxConfigBlock;

typedef struct {
    ArxConfigBlock slot[2];
} ArxConfigImage;

void arx_config_defaults(ArxRuntimeConfig *config);
/* Volatile first-boot bench policy; never writes or migrates the stored reference settings. */
void arx_config_apply_bench_start(ArxRuntimeConfig *config);
bool arx_config_sanitize(ArxRuntimeConfig *config);

uint32_t arx_crc32(const void *data, uint32_t length);

bool arx_config_block_build(
    const ArxRuntimeConfig *config,
    uint32_t generation,
    ArxConfigBlock *block
);
bool arx_config_block_validate(const ArxConfigBlock *block);

const ArxConfigBlock *arx_config_image_select(const ArxConfigImage *image);
uint8_t arx_config_image_next_slot(const ArxConfigImage *image);

/* Imports the historical 32-value configuration layout into typed ARX config. */
bool arx_config_import_legacy_slots(
    const uint16_t slots[ARX_CONFIG_LEGACY_SLOT_COUNT],
    ArxRuntimeConfig *out
);

/* Packs visibility flags MSB-first, matching the original parameter-page ordering. */
void arx_config_pack_visibility(
    const uint8_t *visible,
    uint16_t count,
    uint16_t *packed,
    uint16_t packed_count
);
void arx_config_unpack_visibility(
    const uint16_t *packed,
    uint16_t packed_count,
    uint8_t *visible,
    uint16_t count
);

#endif
