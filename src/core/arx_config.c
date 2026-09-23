#include "arx/arx_config.h"
#include <stddef.h>
#include <string.h>

void arx_config_defaults(ArxRuntimeConfig *c) {
    if (!c) return;
    memset(c,0,sizeof(*c));

    /*
     * Clean-flash defaults mirror the deployed compile-time fallback profile:
     * active-by-default core services stay active; optional vehicle features
     * remain disabled until stored configuration enables them.
     */
    c->immobilizer_enabled=true;
    c->smart_start_stop_enabled=true;
    c->clear_faults_enabled=true;
    c->diesel_profile=true;
    c->seatbelt_alarm_enabled=true;

    c->ipc_my23=false;
    c->dyno_enabled=false;
    c->front_brake_override_enabled=false;
    c->awd_control_enabled=false;
    c->read_faults_enabled=false;

    c->shift_threshold_rpm=4500u;
    c->launch_torque_threshold_nm=100u;
    c->pedal_mode=ARX_CFG_PEDAL_DISABLED;
    c->pedal_power=0;

    c->telemetry_enabled=true;
    c->diagnostics_enabled=true;
    c->sgw_detection_enabled=true;
    c->dynamic_shift_in_dynamic_enabled=false;
}

bool arx_config_sanitize(ArxRuntimeConfig *c) {
    if (!c) return false;
    bool changed=false;

    if (c->shift_threshold_rpm<1500u || c->shift_threshold_rpm>6000u) {
        c->shift_threshold_rpm=4500u; changed=true;
    }
    if (c->launch_torque_threshold_nm<25u || c->launch_torque_threshold_nm>600u) {
        c->launch_torque_threshold_nm=100u; changed=true;
    }
    if (c->pedal_mode>ARX_CFG_PEDAL_LIMITER) {
        c->pedal_mode=ARX_CFG_PEDAL_DISABLED; changed=true;
    }
    if (c->acc_autostart_mode>2u) {
        c->acc_autostart_mode=0u; changed=true;
    }
    if (c->close_windows_mode>2u) {
        c->close_windows_mode=0u; changed=true;
    }
    if (c->open_windows_mode>2u) {
        c->open_windows_mode=0u; changed=true;
    }
    if (c->pedal_power < -10 || c->pedal_power > 10) {
        c->pedal_power=0; changed=true;
    }
    return changed;
}

uint32_t arx_crc32(const void *data, uint32_t length) {
    const uint8_t *bytes=(const uint8_t *)data;
    uint32_t crc=0xFFFFFFFFu;
    if (!bytes && length) return 0u;

    for (uint32_t i=0;i<length;i++) {
        crc^=bytes[i];
        for (unsigned bit=0;bit<8u;bit++) {
            const uint32_t mask=(uint32_t)-(int32_t)(crc&1u);
            crc=(crc>>1u)^(0xEDB88320u&mask);
        }
    }
    return ~crc;
}

static uint32_t block_crc(const ArxConfigBlock *b) {
    /* CRC starts at generation and covers generation + payload. */
    return arx_crc32(
        &b->generation,
        (uint32_t)(sizeof(b->generation)+sizeof(b->payload))
    );
}

bool arx_config_block_build(
    const ArxRuntimeConfig *config,
    uint32_t generation,
    ArxConfigBlock *block
) {
    if (!config || !block) return false;

    memset(block,0,sizeof(*block));
    block->magic=ARX_CONFIG_MAGIC;
    block->version=ARX_CONFIG_VERSION;
    block->payload_size=(uint16_t)sizeof(ArxRuntimeConfig);
    block->generation=generation;
    block->payload=*config;
    (void)arx_config_sanitize(&block->payload);
    block->crc32=block_crc(block);
    return true;
}

bool arx_config_block_validate(const ArxConfigBlock *b) {
    if (!b) return false;
    if (b->magic!=ARX_CONFIG_MAGIC ||
        b->version!=ARX_CONFIG_VERSION ||
        b->payload_size!=sizeof(ArxRuntimeConfig)) return false;
    return b->crc32==block_crc(b);
}

const ArxConfigBlock *arx_config_image_select(const ArxConfigImage *image) {
    if (!image) return NULL;
    const bool a=arx_config_block_validate(&image->slot[0]);
    const bool b=arx_config_block_validate(&image->slot[1]);

    if (!a && !b) return NULL;
    if (a && !b) return &image->slot[0];
    if (!a && b) return &image->slot[1];

    return (image->slot[1].generation>image->slot[0].generation)
        ? &image->slot[1] : &image->slot[0];
}

uint8_t arx_config_image_next_slot(const ArxConfigImage *image) {
    const ArxConfigBlock *current=arx_config_image_select(image);
    if (!current) return 0u;
    return current==&image->slot[0] ? 1u : 0u;
}

bool arx_config_import_legacy_slots(
    const uint16_t s[ARX_CONFIG_LEGACY_SLOT_COUNT],
    ArxRuntimeConfig *c
) {
    if (!s || !c) return false;
    arx_config_defaults(c);

    /*
     * An erased half-word is 0xFFFF. For boolean settings the deployed reader
     * accepts only 0/1 and otherwise falls back to the compile-time default.
     * Keep exactly that semantic rather than treating 0xFFFF as true.
     */
#define ARX_IMPORT_BOOL(field, idx) \
    do { if (s[(idx)] <= 1u) c->field = (s[(idx)] != 0u); } while (0)

    ARX_IMPORT_BOOL(immobilizer_enabled,0);
    ARX_IMPORT_BOOL(smart_start_stop_enabled,1);
    ARX_IMPORT_BOOL(led_strip_enabled,2);
    ARX_IMPORT_BOOL(shift_indicator_enabled,3);
    if (s[4] != 0xFFFFu) c->shift_threshold_rpm=s[4];
    ARX_IMPORT_BOOL(ipc_my23,5);
    ARX_IMPORT_BOOL(route_messages_enabled,6);
    ARX_IMPORT_BOOL(dyno_enabled,7);
    ARX_IMPORT_BOOL(acc_virtual_pad_enabled,8);
    ARX_IMPORT_BOOL(front_brake_override_enabled,9);
    ARX_IMPORT_BOOL(awd_control_enabled,10);

    /* Slot 12 historically held a dormant experimental function; ignored. */

    ARX_IMPORT_BOOL(clear_faults_enabled,12);
    ARX_IMPORT_BOOL(esc_tc_customizer_enabled,13);
    ARX_IMPORT_BOOL(read_faults_enabled,14);
    ARX_IMPORT_BOOL(diesel_profile,15);
    ARX_IMPORT_BOOL(regeneration_alert_enabled,16);
    if (s[17] != 0xFFFFu) c->launch_torque_threshold_nm=s[17];
    ARX_IMPORT_BOOL(seatbelt_alarm_enabled,18);

    if (s[19] <= ARX_CFG_PEDAL_LIMITER)
        c->pedal_mode=(uint8_t)s[19];

    ARX_IMPORT_BOOL(odometer_blink_mask_enabled,20);
    ARX_IMPORT_BOOL(race_mask_enabled,21);
    ARX_IMPORT_BOOL(park_mirror_enabled,22);

    if (s[23] <= 2u) c->acc_autostart_mode=(uint8_t)s[23];
    if (s[24] <= 2u) c->close_windows_mode=(uint8_t)s[24];
    if (s[25] <= 2u) c->open_windows_mode=(uint8_t)s[25];

    ARX_IMPORT_BOOL(has_virtual_pad_enabled,26);
    ARX_IMPORT_BOOL(exhaust_flap_enabled,27);

    if (s[28] != 0xFFFFu)
        c->pedal_power=(int8_t)(uint8_t)s[28];

    ARX_IMPORT_BOOL(front_park_mute_enabled,29);
    ARX_IMPORT_BOOL(sniffer_enabled,30);
    ARX_IMPORT_BOOL(elm327_enabled,31);

#undef ARX_IMPORT_BOOL

    (void)arx_config_sanitize(c);
    return true;
}

void arx_config_pack_visibility(
    const uint8_t *visible,uint16_t count,uint16_t *packed,uint16_t packed_count
) {
    if (!visible || !packed) return;
    memset(packed,0,(size_t)packed_count*sizeof(*packed));
    for (uint16_t i=0;i<count;i++) {
        uint16_t w=(uint16_t)(i/16u);
        if (w>=packed_count) break;
        uint16_t bit=(uint16_t)(15u-(i%16u));
        if (visible[i]&1u) packed[w]|=(uint16_t)(1u<<bit);
    }
}

void arx_config_unpack_visibility(
    const uint16_t *packed,uint16_t packed_count,uint8_t *visible,uint16_t count
) {
    if (!packed || !visible) return;
    for (uint16_t i=0;i<count;i++) {
        uint16_t w=(uint16_t)(i/16u);
        if (w>=packed_count) { visible[i]=0u; continue; }
        uint16_t bit=(uint16_t)(15u-(i%16u));
        visible[i]=(uint8_t)((packed[w]>>bit)&1u);
    }
}
