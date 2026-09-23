#ifndef ARX_STORAGE_H
#define ARX_STORAGE_H

#include "arx/arx_config.h"
#include <stdbool.h>
#include <stdint.h>

#define ARX_STORAGE_SETTINGS_ADDR 0x0801F800u
#define ARX_STORAGE_STATS_ADDR    0x0801F000u
#define ARX_STORAGE_VISIBLE_ADDR  0x0801E800u
#define ARX_STORAGE_PAGE_BYTES    2048u

typedef struct {
    uint16_t (*read_halfword)(uint32_t address, void *user);
    bool (*erase_page)(uint32_t address, void *user);
    bool (*program_halfword)(uint32_t address, uint16_t value, void *user);
    void *user;
} ArxStorageBackend;

typedef struct {
    uint8_t left_park_h;
    uint8_t left_park_v;
    uint8_t right_park_h;
    uint16_t right_park_v;
    bool normal_position_uninitialized;
    uint8_t left_normal_h;
    uint8_t left_normal_v;
    uint8_t right_normal_h;
    uint16_t right_normal_v;
} ArxMirrorStorage;

bool arx_config_export_legacy_slots(
    const ArxRuntimeConfig *config,
    uint16_t slots[ARX_CONFIG_LEGACY_SLOT_COUNT]
);

bool arx_storage_read_settings(
    const ArxStorageBackend *backend,
    ArxRuntimeConfig *config
);

bool arx_storage_write_settings(
    const ArxStorageBackend *backend,
    const ArxRuntimeConfig *config
);

bool arx_storage_read_visible(
    const ArxStorageBackend *backend,
    uint8_t *visible,
    uint16_t count
);

bool arx_storage_write_visible(
    const ArxStorageBackend *backend,
    const uint8_t *visible,
    uint16_t count
);

bool arx_storage_read_mirror(
    const ArxStorageBackend *backend,
    ArxMirrorStorage *mirror
);

bool arx_storage_write_mirror(
    const ArxStorageBackend *backend,
    const ArxMirrorStorage *mirror
);

float arx_storage_read_best_seconds(
    const ArxStorageBackend *backend,
    uint8_t index
);

bool arx_storage_write_best_seconds(
    const ArxStorageBackend *backend,
    float zero_to_100_s,
    float hundred_to_200_s
);

#endif
