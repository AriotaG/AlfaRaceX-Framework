#include "arx/arx_config.h"
#include "arx/arx_telemetry_db.h"
#include "arx/features/arx_performance.h"
#include "arx/features/arx_max_hold.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    ArxRuntimeConfig c;
    arx_config_defaults(&c);
    assert(c.smart_start_stop_enabled);
    assert(c.shift_threshold_rpm==4500u);
    assert(c.launch_torque_threshold_nm==100u);
    assert(c.diesel_profile);
    assert(c.immobilizer_enabled);
    assert(c.clear_faults_enabled);
    assert(c.seatbelt_alarm_enabled);
    assert(!c.ipc_my23);
    assert(!c.dyno_enabled);
    assert(!c.awd_control_enabled);

    uint16_t erased[ARX_CONFIG_LEGACY_SLOT_COUNT];
    for(size_t i=0;i<ARX_CONFIG_LEGACY_SLOT_COUNT;i++) erased[i]=0xFFFFu;
    ArxRuntimeConfig clean;
    assert(arx_config_import_legacy_slots(erased,&clean));
    assert(clean.immobilizer_enabled);
    assert(clean.smart_start_stop_enabled);
    assert(clean.clear_faults_enabled);
    assert(clean.diesel_profile);
    assert(clean.seatbelt_alarm_enabled);
    assert(clean.shift_threshold_rpm==4500u);
    assert(clean.launch_torque_threshold_nm==100u);
    assert(!clean.ipc_my23);
    assert(!clean.dyno_enabled);
    assert(!clean.front_brake_override_enabled);
    assert(!clean.awd_control_enabled);
    assert(!clean.read_faults_enabled);
    assert(clean.pedal_mode==ARX_CFG_PEDAL_DISABLED);
    assert(clean.pedal_power==0);

    ArxConfigImage img;
    memset(&img,0,sizeof(img));
    assert(arx_config_block_build(&c,1u,&img.slot[0]));
    c.shift_threshold_rpm=5000u;
    assert(arx_config_block_build(&c,2u,&img.slot[1]));
    const ArxConfigBlock *sel=arx_config_image_select(&img);
    assert(sel && sel->generation==2u && sel->payload.shift_threshold_rpm==5000u);
    img.slot[1].payload.shift_threshold_rpm=1234u;
    assert(!arx_config_block_validate(&img.slot[1]));
    sel=arx_config_image_select(&img);
    assert(sel && sel->generation==1u);

    uint16_t legacy[32]={0};
    legacy[1]=1; legacy[4]=4750; legacy[7]=1; legacy[10]=1;
    legacy[15]=1; legacy[17]=125; legacy[19]=6; legacy[28]=(uint8_t)-3;
    legacy[30]=1; legacy[31]=1;
    assert(arx_config_import_legacy_slots(legacy,&c));
    assert(c.smart_start_stop_enabled && c.shift_threshold_rpm==4750u);
    assert(c.dyno_enabled && c.awd_control_enabled && c.diesel_profile);
    assert(c.launch_torque_threshold_nm==125u && c.pedal_mode==6u);
    assert(c.pedal_power==-3 && c.sniffer_enabled && c.elm327_enabled);

    uint8_t visible[35]={0};
    visible[0]=1; visible[15]=1; visible[16]=1; visible[34]=1;
    uint16_t packed[3]={0};
    uint8_t unpacked[35]={0};
    arx_config_pack_visibility(visible,35,packed,3);
    arx_config_unpack_visibility(packed,3,unpacked,35);
    assert(!memcmp(visible,unpacked,sizeof(visible)));

    ArxPerformanceStats p;
    arx_performance_init(&p);
    arx_performance_update(&p,0.0f,1000);
    arx_performance_update(&p,1.0f,1010);
    assert(p.zero_to_100_state==ARX_RUN_ACTIVE);
    arx_performance_update(&p,100.0f,6010);
    assert(p.zero_to_100_state==ARX_RUN_COMPLETE);
    assert(p.zero_to_100_s>4.99f && p.zero_to_100_s<5.02f);
    assert(p.best_dirty);

    ArxMaxHold mh;
    arx_max_hold_init(&mh);
    arx_max_hold_set_enabled(&mh,true);
    arx_max_hold_update(&mh,0,10.0f);
    arx_max_hold_update(&mh,0,9.0f);
    arx_max_hold_update(&mh,0,12.0f);
    float v=0;
    assert(arx_max_hold_get(&mh,0,&v)&&v==12.0f);

    size_t n=0;
    const ArxTelemetryDefinition *db=arx_telemetry_diesel(&n);
    assert(n>10u);
    const ArxTelemetryDefinition *d=arx_telemetry_find(db,n,"dpf_load");
    assert(d && d->request_id==0x18DA10F1u && d->request[2]==0x18u && d->request[3]==0xE4u);

    puts("config/performance tests: OK");
    return 0;
}
