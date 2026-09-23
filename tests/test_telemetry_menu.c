#include "arx/arx_telemetry_db.h"
#include "arx/features/arx_menu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    size_t n=0;const ArxTelemetryDefinition *db=arx_telemetry_diesel(&n);
    assert(n>=40u);

    const ArxTelemetryDefinition *d=arx_telemetry_find(db,n,"rail_pressure");
    assert(d&&d->request_id==0x18DA10F1u&&d->request[2]==0x19u&&d->request[3]==0x47u);
    ArxCanFrame req;
    assert(arx_telemetry_build_request(d,ARX_BUS_C1,123u,&req));
    assert(req.extended_id&&req.dlc==4u&&req.data[1]==0x22u);

    ArxCanFrame rsp={.bus=ARX_BUS_C1,.id=0x18DAF110u,.extended_id=true,.dlc=6,
        .data={0x05,0x62,0x19,0x47,0x03,0xE8}};
    float v=0.0f;
    assert(arx_telemetry_decode_response(d,&rsp,&v));
    assert(v>49.99f&&v<50.01f);

    d=arx_telemetry_find(db,n,"turbo_pressure");
    assert(d);
    rsp.id=d->response_id;rsp.data[2]=0x19;rsp.data[3]=0x5A;rsp.data[4]=0x80;rsp.data[5]=0x00;
    assert(arx_telemetry_decode_response(d,&rsp,&v));
    assert(v>-1.001f&&v<-0.999f); /* raw 32768 + (-32768), then -1 */

    ArxRuntimeConfig c;arx_config_defaults(&c);
    ArxMenuState m;arx_menu_init(&m);
    char text[ARX_MENU_TEXT_LEN+1u];
    assert(arx_menu_main_text(1u,text));
    assert(!strncmp(text,"Show Parameters",15u));

    m.setup_page=2u;c.launch_torque_threshold_nm=600u;
    (void)arx_menu_activate_setup(&m,&c);assert(c.launch_torque_threshold_nm==25u);
    m.setup_page=5u;c.shift_threshold_rpm=6000u;
    (void)arx_menu_activate_setup(&m,&c);assert(c.shift_threshold_rpm==1500u);
    m.setup_page=20u;c.pedal_mode=8u;
    assert(arx_menu_activate_setup(&m,&c)&ARX_MENU_EFFECT_PEDAL_RESYNC);assert(c.pedal_mode==0u);
    m.setup_page=21u;c.pedal_power=10;
    (void)arx_menu_activate_setup(&m,&c);assert(c.pedal_power==-10);
    m.setup_page=29u;c.sniffer_enabled=false;c.elm327_enabled=true;
    (void)arx_menu_activate_setup(&m,&c);assert(c.sniffer_enabled&&!c.elm327_enabled);
    m.setup_page=30u;
    (void)arx_menu_activate_setup(&m,&c);assert(c.elm327_enabled&&!c.sniffer_enabled);

    ArxMenuCapabilities caps={.read_faults_available=false,.clear_faults_available=true,
        .dyno_available=true,.esc_tc_available=true,.brake_available=true,.awd_available=true,
        .has_available=true,.exhaust_available=true};
    m.main_page=1u;assert(arx_menu_next_main(&m,&caps,1u)==3u); /* page 2 skipped */

    assert(arx_menu_setup_text(21u,&c,text));
    assert(strstr(text,"Pedal Power")!=NULL);

    puts("telemetry/menu tests: OK");
    return 0;
}
