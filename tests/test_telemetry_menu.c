#include "arx/arx_telemetry_db.h"
#include "arx/features/arx_menu.h"
#include "arx/arx_decoder.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    size_t n=0;const ArxTelemetryDefinition *db=arx_telemetry_diesel(&n);
    assert(n>=40u);

    size_t page_count=0u;
    const ArxTelemetryPage *pages=arx_telemetry_diesel_pages(&page_count);
    assert(pages&&page_count==ARX_DIESEL_DASHBOARD_PAGE_COUNT);
    assert(!strcmp(pages[0].primary_key,"engine_power"));
    assert(!strcmp(pages[5].primary_key,"battery_soc"));
    assert(!strcmp(pages[5].secondary_key,"battery_current"));
    assert(!strcmp(pages[54].primary_key,"pedal_map"));
    char rendered[19];
    assert(arx_telemetry_format_page(&pages[5],75.0f,true,0.0f,true,rendered));
    assert(!strncmp(rendered,"BAT",3u));
    assert(strstr(rendered,"75")!=NULL);
    assert(strstr(rendered,"0.0A")!=NULL);
    for(size_t i=0u;i<page_count;i++){
        assert(arx_telemetry_find(db,n,pages[i].primary_key));
        assert(arx_telemetry_find(db,n,pages[i].secondary_key));
    }

    const ArxTelemetryDefinition *soc=arx_telemetry_find(db,n,"battery_soc");
    assert(soc&&soc->source==ARX_SIGNAL_UDS&&soc->request[2]==0x19u&&soc->request[3]==0xBDu);
    ArxCanFrame soc_req;
    assert(arx_telemetry_build_request(soc,ARX_BUS_C1,10u,&soc_req));
    assert(soc_req.id==0x18DA10F1u);

    const ArxTelemetryDefinition *current=arx_telemetry_find(db,n,"battery_current");
    assert(current&&current->source==ARX_SIGNAL_NATIVE);

    ArxVehicleState vs;arx_vehicle_state_init(&vs);
    ArxCanFrame bat={.bus=ARX_BUS_C1,.id=0x41Au,.extended_id=false,.dlc=6,
        .data={0,80,0,0,0x9C,0x40}};
    arx_decode_frame(&bat,&vs);
    assert(vs.valid_mask&ARX_VS_BATTERY_SOC);
    assert(vs.valid_mask&ARX_VS_BATTERY_CURR);
    assert(vs.battery_soc_percent>79.9f&&vs.battery_soc_percent<80.1f);
    assert(vs.battery_current_a>-0.1f&&vs.battery_current_a<0.1f);

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
