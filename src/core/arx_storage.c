#include "arx/arx_storage.h"
#include <string.h>

static bool valid_backend(const ArxStorageBackend *b) {
    return b && b->read_halfword && b->erase_page && b->program_halfword;
}

bool arx_config_export_legacy_slots(
    const ArxRuntimeConfig *c,
    uint16_t s[ARX_CONFIG_LEGACY_SLOT_COUNT]
) {
    if(!c||!s) return false;
    memset(s,0,sizeof(uint16_t)*ARX_CONFIG_LEGACY_SLOT_COUNT);

    s[0]=c->immobilizer_enabled;
    s[1]=c->smart_start_stop_enabled;
    s[2]=c->led_strip_enabled;
    s[3]=c->shift_indicator_enabled;
    s[4]=c->shift_threshold_rpm;
    s[5]=c->ipc_my23;
    s[6]=c->route_messages_enabled;
    s[7]=c->dyno_enabled;
    s[8]=c->acc_virtual_pad_enabled;
    s[9]=c->front_brake_override_enabled;
    s[10]=c->awd_control_enabled;

    /* Slot 12 of the deployed layout is intentionally left disabled. */
    s[11]=0u;

    s[12]=c->clear_faults_enabled;
    s[13]=c->esc_tc_customizer_enabled;
    s[14]=c->read_faults_enabled;
    s[15]=c->diesel_profile;
    s[16]=c->regeneration_alert_enabled;
    s[17]=c->launch_torque_threshold_nm;
    s[18]=c->seatbelt_alarm_enabled;
    s[19]=c->pedal_mode;
    s[20]=c->odometer_blink_mask_enabled;
    s[21]=c->race_mask_enabled;
    s[22]=c->park_mirror_enabled;
    s[23]=c->acc_autostart_mode;
    s[24]=c->close_windows_mode;
    s[25]=c->open_windows_mode;
    s[26]=c->has_virtual_pad_enabled;
    s[27]=c->exhaust_flap_enabled;
    s[28]=(uint8_t)c->pedal_power;
    s[29]=c->front_park_mute_enabled;
    s[30]=c->sniffer_enabled;
    s[31]=c->elm327_enabled;
    return true;
}

bool arx_storage_read_settings(
    const ArxStorageBackend *b,
    ArxRuntimeConfig *config
) {
    if(!valid_backend(b)||!config) return false;

    uint16_t slots[ARX_CONFIG_LEGACY_SLOT_COUNT];
    for(uint8_t i=0;i<ARX_CONFIG_LEGACY_SLOT_COUNT;i++){
        slots[i]=b->read_halfword(
            ARX_STORAGE_SETTINGS_ADDR + (uint32_t)i*4u,b->user
        );
    }
    return arx_config_import_legacy_slots(slots,config);
}

bool arx_storage_write_settings(
    const ArxStorageBackend *b,
    const ArxRuntimeConfig *config
) {
    if(!valid_backend(b)||!config) return false;

    uint16_t slots[ARX_CONFIG_LEGACY_SLOT_COUNT];
    if(!arx_config_export_legacy_slots(config,slots)) return false;
    if(!b->erase_page(ARX_STORAGE_SETTINGS_ADDR,b->user)) return false;

    for(uint8_t i=0;i<ARX_CONFIG_LEGACY_SLOT_COUNT;i++){
        if(!b->program_halfword(
            ARX_STORAGE_SETTINGS_ADDR+(uint32_t)i*4u,
            slots[i],b->user)) return false;
    }
    return true;
}

bool arx_storage_read_visible(
    const ArxStorageBackend *b,
    uint8_t *visible,
    uint16_t count
) {
    if(!valid_backend(b)||!visible) return false;
    uint16_t packed[30];
    for(uint8_t i=0;i<30u;i++){
        packed[i]=b->read_halfword(
            ARX_STORAGE_VISIBLE_ADDR+(uint32_t)i*4u,b->user
        );
    }
    arx_config_unpack_visibility(packed,30u,visible,count);
    return true;
}

bool arx_storage_write_visible(
    const ArxStorageBackend *b,
    const uint8_t *visible,
    uint16_t count
) {
    if(!valid_backend(b)||!visible) return false;
    uint16_t packed[30];
    arx_config_pack_visibility(visible,count,packed,30u);

    if(!b->erase_page(ARX_STORAGE_VISIBLE_ADDR,b->user)) return false;
    for(uint8_t i=0;i<30u;i++){
        if(!b->program_halfword(
            ARX_STORAGE_VISIBLE_ADDR+(uint32_t)i*4u,
            packed[i],b->user)) return false;
    }
    return true;
}

bool arx_storage_read_mirror(
    const ArxStorageBackend *b,
    ArxMirrorStorage *m
) {
    if(!valid_backend(b)||!m) return false;
    uint16_t s[9];
    for(uint8_t i=0;i<9u;i++)
        s[i]=b->read_halfword(ARX_STORAGE_SETTINGS_ADDR+(uint32_t)i*4u,b->user);

    m->left_park_h=(uint8_t)s[0];
    m->left_park_v=(uint8_t)s[1];
    m->right_park_h=(uint8_t)s[2];
    m->right_park_v=s[3];
    m->normal_position_uninitialized=(s[4]!=0u);
    m->left_normal_h=(uint8_t)s[5];
    m->left_normal_v=(uint8_t)s[6];
    m->right_normal_h=(uint8_t)s[7];
    m->right_normal_v=s[8];
    return true;
}

bool arx_storage_write_mirror(
    const ArxStorageBackend *b,
    const ArxMirrorStorage *m
) {
    if(!valid_backend(b)||!m) return false;
    const uint16_t s[9]={
        m->left_park_h,m->left_park_v,m->right_park_h,m->right_park_v,
        m->normal_position_uninitialized,
        m->left_normal_h,m->left_normal_v,m->right_normal_h,m->right_normal_v
    };

    if(!b->erase_page(ARX_STORAGE_SETTINGS_ADDR,b->user)) return false;
    for(uint8_t i=0;i<9u;i++){
        if(!b->program_halfword(
            ARX_STORAGE_SETTINGS_ADDR+(uint32_t)i*4u,
            s[i],b->user)) return false;
    }
    return true;
}

float arx_storage_read_best_seconds(
    const ArxStorageBackend *b,
    uint8_t index
) {
    if(!b||!b->read_halfword||index<1u||index>2u) return 0.0f;
    uint16_t raw=b->read_halfword(
        ARX_STORAGE_STATS_ADDR+(uint32_t)(index-1u)*4u,b->user
    );
    return (float)raw/1000.0f;
}

bool arx_storage_write_best_seconds(
    const ArxStorageBackend *b,
    float zero_to_100_s,
    float hundred_to_200_s
) {
    if(!valid_backend(b)) return false;
    /* Validate before float-to-integer conversion (NaN/Inf/out-of-range are undefined). */
    if(!(zero_to_100_s>=0.0f&&zero_to_100_s<=65.535f)||
       !(hundred_to_200_s>=0.0f&&hundred_to_200_s<=65.535f)) return false;

    uint32_t a=(uint32_t)(zero_to_100_s*1000.0f+0.5f);
    uint32_t c=(uint32_t)(hundred_to_200_s*1000.0f+0.5f);
    if(a>0xFFFFu) a=0xFFFFu;
    if(c>0xFFFFu) c=0xFFFFu;

    if(!b->erase_page(ARX_STORAGE_STATS_ADDR,b->user)) return false;

    /* Keep the historical four-byte stride while writing deterministic values. */
    if(!b->program_halfword(ARX_STORAGE_STATS_ADDR,(uint16_t)a,b->user)) return false;
    if(!b->program_halfword(ARX_STORAGE_STATS_ADDR+4u,(uint16_t)c,b->user)) return false;
    return true;
}
