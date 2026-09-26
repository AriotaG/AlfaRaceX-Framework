#include "arx/arx_runtime.h"
#include "arx/arx_telemetry_db.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    ArxCanFrame can[64];
    size_t can_count;
    uint8_t ic[64][ARX_INTERCHIP_FRAME_SIZE];
    size_t ic_count;
    uint8_t pedal[ARX_PEDAL_PACKET_SIZE];
    size_t pedal_count;
    size_t usb_bytes;
    uint8_t usb_data[4096];
    size_t usb_data_len;
    size_t usb_attach_count;
    size_t usb_detach_count;
    ArxUsbMode last_usb_mode;
    size_t led_submits;
} Sink;

static ArxStatus can_send(const ArxCanFrame *f,void *u){
    Sink *s=(Sink*)u;
    assert(s->can_count<64u);
    s->can[s->can_count++]=*f;
    return ARX_STATUS_OK;
}
static bool ic_send(const uint8_t f[ARX_INTERCHIP_FRAME_SIZE],void *u){
    Sink *s=(Sink*)u;
    assert(s->ic_count<64u);
    memcpy(s->ic[s->ic_count++],f,ARX_INTERCHIP_FRAME_SIZE);
    return true;
}
static bool pedal_send(const uint8_t p[ARX_PEDAL_PACKET_SIZE],void *u){
    Sink *s=(Sink*)u;
    memcpy(s->pedal,p,ARX_PEDAL_PACKET_SIZE);
    s->pedal_count++;
    return true;
}
static bool usb_attach(ArxUsbMode mode,void *u){
    Sink *s=(Sink*)u;s->usb_attach_count++;s->last_usb_mode=mode;return true;
}
static bool usb_detach(void *u){
    ((Sink*)u)->usb_detach_count++;return true;
}
static bool usb_send(const uint8_t *d,size_t n,void *u){
    Sink *s=(Sink*)u;
    s->usb_bytes+=n;
    if(d&&n&&s->usb_data_len+n<=sizeof(s->usb_data)){
        memcpy(&s->usb_data[s->usb_data_len],d,n);
        s->usb_data_len+=n;
    }
    return true;
}
static bool led_submit(const ArxRgb rgb[ARX_LED_COUNT],void *u){
    (void)rgb;((Sink*)u)->led_submits++;return true;
}

static ArxRuntimeOps ops(Sink *s){
    ArxRuntimeOps o={
        .can_send=can_send,
        .interchip_send=ic_send,
        .pedal_send=pedal_send,
        .usb_attach=usb_attach,
        .usb_detach=usb_detach,
        .usb_send=usb_send,
        .led_submit=led_submit,
        .user=s
    };
    return o;
}


static void elm_runtime_unit(void){
    Sink sink={0};
    ArxRuntimeOps o=ops(&sink);
    ArxRuntime c1;
    arx_runtime_init(&c1,ARX_RUNTIME_C1,&o);

    ArxRuntimeConfig cfg;
    arx_config_defaults(&cfg);
    cfg.elm327_enabled=true;
    cfg.sniffer_enabled=false;
    arx_runtime_apply_config(&c1,&cfg,10u);
    arx_runtime_tick(&c1,10u);
    assert(sink.usb_attach_count==1u);
    assert(sink.last_usb_mode==ARX_USB_MODE_DIAGNOSTIC);
    arx_runtime_usb_configured(&c1,11u);

    const uint8_t ati[]={'A','T','I','\r'};
    arx_runtime_usb_rx(&c1,ati,sizeof(ati),12u);
    for(uint32_t t=12u;t<40u;t++)arx_runtime_tick(&c1,t);
    assert(sink.usb_data_len<sizeof(sink.usb_data));
    sink.usb_data[sink.usb_data_len]='\0';
    assert(strstr((const char*)sink.usb_data,"ELM327 v1.4")!=NULL);

    sink.usb_data_len=0u;
    const uint8_t obd[]={'0','1','0','C','\r'};
    arx_runtime_usb_rx(&c1,obd,sizeof(obd),100u);
    assert(c1.elm_request_active);
    assert(arx_runtime_drain_can(&c1,100u,8u)>=1u);
    bool found_request=false;
    for(size_t i=0u;i<sink.can_count;i++){
        if(sink.can[i].id==0x7DFu&&sink.can[i].dlc==8u&&
           sink.can[i].data[0]==0x02u&&sink.can[i].data[1]==0x01u&&
           sink.can[i].data[2]==0x0Cu)
            found_request=true;
    }
    assert(found_request);

    /* A valid ISO-TP packet from an unrelated ECU must not complete the USB request. */
    const uint32_t request_deadline=c1.elm_deadline_ms;
    ArxCanFrame unrelated={.bus=ARX_BUS_C1,.id=0x123u,.dlc=4u,.data={3,0x62,0xF1,0x90}};
    arx_runtime_on_can(&c1,&unrelated,100u);
    assert(c1.elm_request_active && c1.elm_deadline_ms==request_deadline);
    unrelated.id=0x7E8u;unrelated.extended_id=true;
    arx_runtime_on_can(&c1,&unrelated,100u);
    assert(c1.elm_request_active && c1.elm_deadline_ms==request_deadline);

    ArxCanFrame rsp={.bus=ARX_BUS_C1,.id=0x7E8u,.dlc=8u,
        .data={0x04u,0x41u,0x0Cu,0x1Au,0xF8u,0u,0u,0u}};
    arx_runtime_on_can(&c1,&rsp,101u);
    for(uint32_t t=101u;t<130u;t++)arx_runtime_tick(&c1,t);
    assert(!c1.elm_request_active);
    assert(sink.usb_data_len<sizeof(sink.usb_data));
    sink.usb_data[sink.usb_data_len]='\0';
    assert(strstr((const char*)sink.usb_data,"41 0C 1A F8")!=NULL);

    /* Force the same default functional request through C2 to prove that the
       target diagnostic link is wired, not merely the portable router. */
    Sink master_sink={0},c2_sink={0};
    ArxRuntimeOps master_ops=ops(&master_sink);
    ArxRuntimeOps slave_ops=ops(&c2_sink);
    ArxRuntime master,c2;
    arx_runtime_init(&master,ARX_RUNTIME_C1,&master_ops);
    arx_runtime_init(&c2,ARX_RUNTIME_C2,&slave_ops);
    arx_runtime_apply_config(&master,&cfg,3000u);
    arx_runtime_tick(&master,3000u);
    arx_runtime_usb_configured(&master,3001u);
    arx_elm_router_remember(&master.elm_router,&master.elm,ARX_ELM_BUS_C2);

    arx_runtime_usb_rx(&master,obd,sizeof(obd),3002u);
    assert(master.elm_request_active);
    assert(arx_runtime_drain_interchip(&master,3002u,8u)==3u);
    assert(master_sink.ic_count==3u);

    for(size_t i=0u;i<master_sink.ic_count;i++)
        arx_runtime_on_interchip(&c2,master_sink.ic[i],3003u+(uint32_t)i);

    assert(arx_runtime_drain_can(&c2,3006u,8u)>=1u);
    bool c2_request=false;
    for(size_t i=0u;i<c2_sink.can_count;i++){
        if(c2_sink.can[i].id==0x7DFu&&c2_sink.can[i].data[1]==0x01u&&
           c2_sink.can[i].data[2]==0x0Cu)c2_request=true;
    }
    assert(c2_request);

    ArxCanFrame c2rsp={.bus=ARX_BUS_C2,.id=0x7E8u,.dlc=8u,
        .data={0x04u,0x41u,0x0Cu,0x1Au,0xF8u,0u,0u,0u}};
    arx_runtime_on_can(&c2,&c2rsp,3007u);
    assert(arx_runtime_drain_interchip(&c2,3007u,8u)>=1u);
    assert(c2_sink.ic_count>=1u);
    arx_runtime_on_interchip(
        &master,c2_sink.ic[c2_sink.ic_count-1u],3008u
    );
    assert(!master.elm_request_active);
    for(uint32_t t=3008u;t<3040u;t++)arx_runtime_tick(&master,t);
    assert(master_sink.usb_data_len<sizeof(master_sink.usb_data));
    master_sink.usb_data[master_sink.usb_data_len]='\0';
    assert(strstr((const char*)master_sink.usb_data,"41 0C 1A F8")!=NULL);

    /* Slave roles default to the MSC compatibility class when sniffer is off. */
    Sink msc_sink={0};
    ArxRuntimeOps msc_ops=ops(&msc_sink);
    ArxRuntime bh;
    arx_runtime_init(&bh,ARX_RUNTIME_BH,&msc_ops);
    arx_runtime_tick(&bh,0u);
    assert(msc_sink.usb_attach_count==1u);
    assert(msc_sink.last_usb_mode==ARX_USB_MODE_LEGACY_MSC);
}

static void telemetry_runtime_unit(void){
    Sink sink={0};
    ArxRuntimeOps o=ops(&sink);
    ArxRuntime rt;
    arx_runtime_init(&rt,ARX_RUNTIME_C1,&o);

    ArxRuntimeConfig cfg;
    arx_config_defaults(&cfg);
    cfg.diesel_profile=true;
    cfg.sniffer_enabled=false;
    cfg.elm327_enabled=false;
    arx_runtime_apply_config(&rt,&cfg,0u);

    rt.menu.visible=true;
    rt.menu.level=ARX_MENU_LEVEL_SUB;
    rt.menu.main_page=1u;
    rt.menu.param_page=5u; /* BAT SoC + current, exact reference diesel page 6 */

    ArxCanFrame bat={.bus=ARX_BUS_C1,.id=0x41Au,.dlc=6,
        .data={0,70,0,0,0x9C,0x40}};
    arx_runtime_on_can(&rt,&bat,500u);
    assert(rt.vehicle.valid_mask&ARX_VS_BATTERY_CURR);
    assert(rt.vehicle.battery_current_a>-0.1f&&rt.vehicle.battery_current_a<0.1f);

    rt.config.diagnostics_enabled=false;
    arx_runtime_tick(&rt,900u);
    assert(rt.telemetry_last_poll_ms==0u);
    rt.config.diagnostics_enabled=true;
    rt.config.telemetry_enabled=false;
    arx_runtime_tick(&rt,950u);
    assert(rt.telemetry_last_poll_ms==0u);
    rt.config.telemetry_enabled=true;
    arx_runtime_tick(&rt,1000u);
    (void)arx_runtime_drain_can(&rt,1000u,16u);
    bool found_soc_request=false;
    for(size_t i=0u;i<sink.can_count;i++){
        if(sink.can[i].id==0x18DA10F1u&&sink.can[i].dlc==4u&&
           sink.can[i].data[1]==0x22u&&sink.can[i].data[2]==0x19u&&
           sink.can[i].data[3]==0xBDu)
            found_soc_request=true;
    }
    assert(found_soc_request);

    ArxCanFrame soc={.bus=ARX_BUS_C1,.id=0x18DAF110u,.extended_id=true,.dlc=5u,
        .data={0x04,0x62,0x19,0xBD,75}};
    arx_runtime_on_can(&rt,&soc,1010u);

    size_t n=0u;
    const ArxTelemetryDefinition *db=arx_telemetry_diesel(&n);
    const ArxTelemetryDefinition *d=arx_telemetry_find(db,n,"battery_soc");
    assert(d);
    size_t idx=(size_t)(d-db);
    assert(idx<ARX_RUNTIME_TELEMETRY_CACHE_MAX);
    assert(rt.telemetry_valid[idx]);
    assert(rt.telemetry_values[idx]>74.9f&&rt.telemetry_values[idx]<75.1f);
    assert(rt.interchip.tx.count>0u); /* formatted dashboard text queued for BH */
}

int main(void){
    elm_runtime_unit();
    telemetry_runtime_unit();

    Sink sink={0};
    ArxRuntimeOps o=ops(&sink);
    ArxRuntime c1;
    arx_runtime_init(&c1,ARX_RUNTIME_C1,&o);

    ArxRuntimeConfig cfg;
    arx_config_defaults(&cfg);
    cfg.ipc_my23=false;
    cfg.shift_indicator_enabled=true;
    cfg.shift_threshold_rpm=3500u;
    cfg.smart_start_stop_enabled=true;
    cfg.pedal_mode=ARX_CFG_PEDAL_DYNAMIC;
    cfg.pedal_power=0;
    cfg.led_strip_enabled=true;
    cfg.sniffer_enabled=true;
    cfg.esc_tc_customizer_enabled=true;
    cfg.race_mask_enabled=true;
    arx_runtime_apply_config(&c1,&cfg,0u);

    /* Engine state. */
    ArxCanFrame rpm={.bus=ARX_BUS_C1,.id=0x0FC,.dlc=8,
        .data={0x1F,0x40,0,0,0,0,0,0}}; /* 2000 rpm */
    arx_runtime_on_can(&c1,&rpm,11000u);
    assert(c1.vehicle.engine_rpm==2000u);

    /* Start/Stop is reported enabled. */
    ArxCanFrame ss={.bus=ARX_BUS_C1,.id=0x226,.dlc=8,.data={0,0,0,0,0,0,0,0}};
    arx_runtime_on_can(&c1,&ss,11001u);

    /* Template to modify when the five-second engine grace expires. */
    ArxCanFrame b1={.bus=ARX_BUS_C1,.id=0x4B1,.dlc=8,
        .data={1,2,3,4,5,0xA0,7,8}};
    arx_runtime_on_can(&c1,&b1,11002u);
    arx_runtime_tick(&c1,16001u);
    assert(sink.usb_attach_count==1u);
    assert(sink.last_usb_mode==ARX_USB_MODE_SNIFFER);
    assert(c1.usb_mode.state==ARX_USB_WAIT_HOST);
    arx_runtime_usb_configured(&c1,16002u);
    assert(c1.usb_mode.state==ARX_USB_CONFIGURED);
    arx_runtime_usb_command(&c1,16003u);
    assert(c1.usb_mode.last_command_ms==16003u);
    assert(arx_runtime_drain_can(&c1,16001u,16u)>=1u);

    bool found_ss=false;
    for(size_t i=0;i<sink.can_count;i++){
        if(sink.can[i].id==0x4B1u){
            found_ss=true;
            assert((sink.can[i].data[5]&0x38u)==0x08u);
        }
    }
    assert(found_ss);

    /* Shift frame is reproduced with urgency. */
    ArxCanFrame rpm_hi={.bus=ARX_BUS_C1,.id=0x0FC,.dlc=8,
        .data={0x36,0xB0,0,0,0,0,0,0}}; /* 3500 rpm */
    arx_runtime_on_can(&c1,&rpm_hi,17000u);
    ArxCanFrame sh={.bus=ARX_BUS_C1,.id=0x2ED,.dlc=8};
    arx_runtime_on_can(&c1,&sh,17001u);
    (void)arx_runtime_drain_can(&c1,17001u,16u);
    bool found_shift=false;
    for(size_t i=0;i<sink.can_count;i++){
        if(sink.can[i].id==0x2EDu&&(sink.can[i].data[6]&0x03u)==1u)
            found_shift=true;
    }
    assert(found_shift);

    /* Pedal sync runs from the scheduler. */
    arx_runtime_tick(&c1,18000u);
    assert(sink.pedal_count>=1u);
    assert(sink.pedal[0]=='#'&&sink.pedal[1]==0xB6u);

    /* Configuration synchronization uses the 19-byte link. */
    arx_runtime_queue_config_sync(&c1);
    assert(c1.interchip.tx.count>=8u);
    c1.interchip.last_tx_ms=0u;
    assert(arx_runtime_drain_interchip(&c1,2001u,8u)==1u);
    assert(sink.ic_count==1u);

    ArxRuntimeConfig usb_off=cfg;
    usb_off.sniffer_enabled=false;
    usb_off.elm327_enabled=false;
    arx_runtime_apply_config(&c1,&usb_off,2100u);
    assert(c1.usb_mode.state==ARX_USB_DETACH_REQUESTED);
    arx_runtime_tick(&c1,2101u);
    assert(sink.usb_detach_count==1u);
    assert(c1.usb_mode.state==ARX_USB_DETACHED);

    /* C2 receives configuration and toggle command. */
    Sink c2sink={0};
    ArxRuntimeOps c2ops=ops(&c2sink);
    ArxRuntime c2;
    arx_runtime_init(&c2,ARX_RUNTIME_C2,&c2ops);
    ArxRuntimeConfig c2cfg=cfg;
    c2cfg.esc_tc_customizer_enabled=true;
    arx_runtime_apply_config(&c2,&c2cfg,0u);

    uint8_t cmd[ARX_INTERCHIP_FRAME_SIZE];
    memset(cmd,ARX_INTERCHIP_PAD,sizeof(cmd));
    cmd[0]=ARX_IC_TO_C2;
    cmd[1]=ARX_IC_C2_ESC_TC_TOGGLE;
    arx_runtime_on_interchip(&c2,cmd,3000u);
    assert(c2.drive_style.inversion_active);

    /* C2 publishes visual state to C1+BH queue. */
    assert(c2.interchip.tx.count==1u);

    /* C2 park mute observes real bus data and produces a virtual button. */
    c2.park_mute.enabled=true;
    ArxCanFrame pdc={.bus=ARX_BUS_C2,.id=0x3E7,.dlc=8,.data={0x40}};
    ArxCanFrame brake={.bus=ARX_BUS_C2,.id=0x107,.dlc=8,.data={40}};
    ArxCanFrame led={.bus=ARX_BUS_C2,.id=0x54A,.dlc=8,.data={0,0,0,0}};
    arx_runtime_on_can(&c2,&pdc,3100u);
    arx_runtime_on_can(&c2,&led,3101u);
    arx_runtime_on_can(&c2,&brake,3102u);
    arx_runtime_tick(&c2,3103u);
    (void)arx_runtime_drain_can(&c2,3103u,16u);
    bool found_pdc=false;
    for(size_t i=0;i<c2sink.can_count;i++)
        if(c2sink.can[i].id==0x5B0u&&c2sink.can[i].data[1]==0x20u)found_pdc=true;
    assert(found_pdc);

    /* Destination collision test: value 0x40 means race-display sync on C1+BH,
       but sniffer-on on C2+BH. */
    Sink bhsink={0};
    ArxRuntimeOps bhops=ops(&bhsink);
    ArxRuntime bh;
    arx_runtime_init(&bh,ARX_RUNTIME_BH,&bhops);
    ArxRuntimeConfig bhcfg=cfg;
    bhcfg.esc_tc_customizer_enabled=true;
    arx_runtime_apply_config(&bh,&bhcfg,0u);

    memset(cmd,ARX_INTERCHIP_PAD,sizeof(cmd));
    cmd[0]=ARX_IC_TO_C1_BH;
    cmd[1]=ARX_IC_C1_BH_RACE_SHOW;
    arx_runtime_on_interchip(&bh,cmd,4000u);
    assert(bh.drive_style.inversion_active);
    assert(!bh.sniffer.enabled || bhcfg.sniffer_enabled);

    memset(cmd,ARX_INTERCHIP_PAD,sizeof(cmd));
    cmd[0]=ARX_IC_TO_C2_BH;
    cmd[1]=ARX_IC_SHARED_SNIFFER_ON;
    arx_runtime_on_interchip(&bh,cmd,4001u);
    assert(bh.sniffer.enabled);

    /* Chime is a destination-only request and is materialized from the next
       native 0x5AC frame. */
    memset(cmd,ARX_INTERCHIP_PAD,sizeof(cmd));
    cmd[0]=ARX_IC_BH_CHIME;
    arx_runtime_on_interchip(&bh,cmd,4100u);
    assert(bh.bh_chime_requested);

    ArxCanFrame native_chime={.bus=ARX_BUS_BH,.id=0x5AC,.dlc=8,
        .data={0xC0,0x80,0,0,0,0,0,0}};
    arx_runtime_on_can(&bh,&native_chime,4101u);
    (void)arx_runtime_drain_can(&bh,4101u,8u);
    bool got_chime=false;
    for(size_t i=0;i<bhsink.can_count;i++){
        if(bhsink.can[i].id==0x5ACu){
            got_chime=true;
            assert((bhsink.can[i].data[0]&0xC0u)==0u);
            assert((bhsink.can[i].data[1]&0xC0u)==0x40u);
            assert((bhsink.can[i].data[3]&0xE0u)==0xE0u);
        }
    }
    assert(got_chime && !bh.bh_chime_requested);

    /* Park-position store command captures the next steady mirror sample. */
    memset(cmd,ARX_INTERCHIP_PAD,sizeof(cmd));
    cmd[0]=ARX_IC_TO_BH;
    cmd[1]=ARX_IC_BH_MIRROR_STORE;
    arx_runtime_on_interchip(&bh,cmd,4200u);
    assert(bh.park_mirror.capture_park);
    ArxCanFrame mirror={.bus=ARX_BUS_BH,.id=0x5A6,.dlc=8,
        .data={10,20,30,0x01,0x20,0,0,0}};
    arx_runtime_on_can(&bh,&mirror,4201u);
    assert(bh.park_mirror.calibrated_park);
    assert(bh.park_mirror.park.left_h==10u);

    puts("runtime tests: OK");
    return 0;
}
