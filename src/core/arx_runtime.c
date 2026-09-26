#include "arx/arx_runtime.h"
#include "arx/arx_decoder.h"
#include "arx/arx_telemetry_db.h"
#include <string.h>

/* Target builds compile a single hardware role. Host builds leave all three
 * enabled so the integration tests can exercise the complete topology. */
#if defined(ARX_BUILD_C1)
#define ARX_COMPILE_C1 1
#define ARX_COMPILE_C2 0
#define ARX_COMPILE_BH 0
#elif defined(ARX_BUILD_C2)
#define ARX_COMPILE_C1 0
#define ARX_COMPILE_C2 1
#define ARX_COMPILE_BH 0
#elif defined(ARX_BUILD_BH)
#define ARX_COMPILE_C1 0
#define ARX_COMPILE_C2 0
#define ARX_COMPILE_BH 1
#else
#define ARX_COMPILE_C1 1
#define ARX_COMPILE_C2 1
#define ARX_COMPILE_BH 1
#endif

static ArxBus role_bus(ArxRuntimeRole role) {
    switch(role){
        case ARX_RUNTIME_C1:return ARX_BUS_C1;
        case ARX_RUNTIME_C2:return ARX_BUS_C2;
        default:return ARX_BUS_BH;
    }
}

static ArxInterchipRole role_interchip(ArxRuntimeRole role) {
    switch(role){
        case ARX_RUNTIME_C1:return ARX_IC_ROLE_C1;
        case ARX_RUNTIME_C2:return ARX_IC_ROLE_C2;
        default:return ARX_IC_ROLE_BH;
    }
}

static bool enqueue_can(
    ArxRuntime *rt,
    const ArxCanFrame *frame,
    ArxPriority priority,
    uint32_t now_ms
) {
    if(!rt||!frame)return false;
    ArxCanFrame f=*frame;
    f.timestamp_ms=now_ms;
    if(arx_can_enqueue(&rt->can_tx,&f,priority,2u,now_ms+500u)!=ARX_STATUS_OK)
        return false;
    rt->generated_frames++;
    return true;
}

#if ARX_COMPILE_C1 || ARX_COMPILE_C2
static bool ic_push(ArxRuntime *rt,const uint8_t *payload,size_t length) {
    if(!rt||!payload||length==0u)return false;
    ArxInterchipFrame f;
    arx_interchip_frame_build(&f,payload,length);
    return arx_interchip_queue_push(&rt->interchip.tx,&f);
}
#endif

#if ARX_COMPILE_C1 || ARX_COMPILE_C2 || ARX_COMPILE_BH
static bool ic_push_link(ArxRuntime *rt,const ArxLinkFrame *link) {
    if(!rt||!link)return false;
    ArxInterchipFrame f;
    memcpy(f.bytes,link->raw,ARX_INTERCHIP_FRAME_SIZE);
    return arx_interchip_queue_push(&rt->interchip.tx,&f);
}
#endif

#if ARX_COMPILE_C1
static ArxBus elm_bus(ArxElmBus bus) {
    return bus==ARX_ELM_BUS_C2?ARX_BUS_C2:
           bus==ARX_ELM_BUS_BH?ARX_BUS_BH:ARX_BUS_C1;
}

static uint8_t elm_link_destination(ArxElmBus bus) {
    return bus==ARX_ELM_BUS_C2?ARX_LINK_TO_C2:ARX_LINK_TO_BH;
}

static void elm_usb_compact(ArxRuntime *rt) {
    if(!rt||rt->elm_usb_tx_off==0u)return;
    if(rt->elm_usb_tx_off>=rt->elm_usb_tx_len){
        rt->elm_usb_tx_off=0u;
        rt->elm_usb_tx_len=0u;
        return;
    }
    const uint16_t remain=(uint16_t)(rt->elm_usb_tx_len-rt->elm_usb_tx_off);
    for(uint16_t i=0u;i<remain;i++)
        rt->elm_usb_tx[i]=rt->elm_usb_tx[rt->elm_usb_tx_off+i];
    rt->elm_usb_tx_len=remain;
    rt->elm_usb_tx_off=0u;
}

static bool elm_usb_queue(ArxRuntime *rt,const void *data,size_t length) {
    if(!rt||(!data&&length))return false;
    elm_usb_compact(rt);
    if(length>sizeof(rt->elm_usb_tx)-rt->elm_usb_tx_len)return false;
    if(length){
        memcpy(&rt->elm_usb_tx[rt->elm_usb_tx_len],data,length);
        rt->elm_usb_tx_len=(uint16_t)(rt->elm_usb_tx_len+length);
    }
    return true;
}

static bool elm_usb_text(ArxRuntime *rt,const char *text) {
    return text?elm_usb_queue(rt,text,strlen(text)):false;
}

static void elm_usb_line_end(ArxRuntime *rt) {
    if(rt&&rt->elm.linefeeds)(void)elm_usb_text(rt,"\r\n");
    else if(rt)(void)elm_usb_text(rt,"\r");
}

static void elm_usb_prompt(ArxRuntime *rt) {
    if(!rt)return;
    elm_usb_line_end(rt);
    (void)elm_usb_text(rt,">");
}

static char elm_hex(uint8_t n) {
    static const char digits[]="0123456789ABCDEF";
    return digits[n&0x0Fu];
}

static void elm_usb_hex_byte(ArxRuntime *rt,uint8_t value) {
    char out[2]={elm_hex((uint8_t)(value>>4u)),elm_hex((uint8_t)(value&0x0Fu))};
    (void)elm_usb_queue(rt,out,sizeof(out));
}

static void elm_usb_hex_id(ArxRuntime *rt,uint32_t id,bool extended) {
    const uint8_t digits=extended?8u:3u;
    char out[8];
    for(uint8_t i=0u;i<digits;i++){
        const uint8_t shift=(uint8_t)((digits-1u-i)*4u);
        out[i]=elm_hex((uint8_t)((id>>shift)&0x0Fu));
    }
    (void)elm_usb_queue(rt,out,digits);
}

static void elm_usb_emit_event(ArxRuntime *rt,const ArxElmRxEvent *event) {
    if(!rt||!event)return;
    if(rt->elm.headers){
        elm_usb_hex_id(rt,event->can_id,event->extended_id);
        (void)elm_usb_text(rt," ");
    }
    for(uint16_t i=0u;i<event->length;i++){
        elm_usb_hex_byte(rt,event->data[i]);
        if(rt->elm.spaces&&i+1u<event->length)(void)elm_usb_text(rt," ");
    }
    elm_usb_line_end(rt);
}

static void elm_usb_no_data(ArxRuntime *rt) {
    if(!rt)return;
    (void)elm_usb_text(rt,"NO DATA");
    elm_usb_prompt(rt);
}

static void elm_disarm_remote(ArxRuntime *rt) {
    if(!rt||!rt->elm_request_active||
       rt->elm_candidate_index>=rt->elm_candidate_count)return;
    const ArxElmBus bus=rt->elm_candidates[rt->elm_candidate_index];
    if(bus==ARX_ELM_BUS_C1)return;
    ArxLinkFrame link;
    arx_link_build_arm(
        &link,elm_link_destination(bus),false,rt->elm_link_sequence++
    );
    (void)ic_push_link(rt,&link);
}

static bool elm_send_frame(
    ArxRuntime *rt,const ArxCanFrame *frame,ArxElmBus target,uint32_t now_ms
) {
    if(!rt||!frame)return false;
    if(target==ARX_ELM_BUS_C1)
        return enqueue_can(rt,frame,ARX_PRIORITY_HIGH,now_ms);

    ArxLinkFrame link;
    arx_link_build_can(
        &link,elm_link_destination(target),ARX_LINK_REQ,
        frame->extended_id,frame->id,frame->data,frame->dlc,
        rt->elm_link_sequence++
    );
    return ic_push_link(rt,&link);
}

static bool elm_arm_remote(
    ArxRuntime *rt,ArxElmBus bus,uint32_t now_ms
) {
    if(!rt||bus==ARX_ELM_BUS_C1)return true;

    uint32_t value=0u,mask=0u;
    bool extended=rt->elm.tx_extended;
    (void)arx_elm_response_filter(&rt->elm,&value,&mask,&extended);

    ArxLinkFrame cfg;
    arx_link_build_config(
        &cfg,elm_link_destination(bus),value,mask,rt->elm.timeout_ms,
        rt->elm.auto_flow_control,rt->elm_link_sequence++
    );
    if(extended)cfg.raw[2]|=ARX_LINK_FLAG_EXTID;
    cfg.raw[17]=arx_link_checksum(cfg.raw);
    if(!ic_push_link(rt,&cfg))return false;

    ArxLinkFrame arm;
    arx_link_build_arm(
        &arm,elm_link_destination(bus),true,rt->elm_link_sequence++
    );
    if(!ic_push_link(rt,&arm))return false;

    rt->elm_deadline_ms=now_ms+rt->elm.timeout_ms;
    return true;
}

static bool elm_start_candidate(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt||rt->elm_candidate_index>=rt->elm_candidate_count)return false;
    const ArxElmBus candidate=rt->elm_candidates[rt->elm_candidate_index];

    /* Reserve the configuration, arm and request slots together. Runtime ingress
       is serialized, so no interrupt can consume this space during construction. */
    if(candidate!=ARX_ELM_BUS_C1 &&
       rt->interchip.tx.count>ARX_INTERCHIP_QUEUE_SIZE-3u)return false;

    if(!elm_arm_remote(rt,candidate,now_ms))return false;

    ArxCanFrame first;
    if(!arx_elm_transaction_start(
        &rt->elm_transaction,&rt->elm,elm_bus(candidate),
        rt->elm_request.data,rt->elm_request.length,1u,now_ms,&first
    )){
        elm_disarm_remote(rt);
        return false;
    }

    rt->elm_saw_response=false;
    rt->elm_deadline_ms=now_ms+rt->elm.timeout_ms;
    return elm_send_frame(rt,&first,candidate,now_ms);
}

static void elm_finish_request(ArxRuntime *rt,bool success) {
    if(!rt)return;
    elm_disarm_remote(rt);
    if(success&&rt->elm_candidate_index<rt->elm_candidate_count)
        arx_elm_router_remember(
            &rt->elm_router,&rt->elm,rt->elm_candidates[rt->elm_candidate_index]
        );
    rt->elm_request_active=false;
    rt->elm_transaction.active=false;
    if(success)elm_usb_prompt(rt);
}

static bool elm_try_next_candidate(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt)return false;
    elm_disarm_remote(rt);
    while(++rt->elm_candidate_index<rt->elm_candidate_count){
        if(elm_start_candidate(rt,now_ms))return true;
    }
    rt->elm_request_active=false;
    rt->elm_transaction.active=false;
    elm_usb_no_data(rt);
    return false;
}

static void elm_handle_response(
    ArxRuntime *rt,const ArxCanFrame *frame,uint32_t now_ms
) {
    if(!rt||!frame||!rt->elm_request_active)return;

    if(arx_elm_transaction_on_flow_control(
        &rt->elm_transaction,frame,now_ms
    )){
        rt->elm_saw_response=true;
        rt->elm_deadline_ms=now_ms+rt->elm.timeout_ms;
        return;
    }

    ArxElmRxEvent event;
    const ArxElmRxEventType kind=arx_elm_transaction_on_rx(
        &rt->elm_transaction,&rt->elm,frame,&event
    );
    if(kind==ARX_ELM_RX_NONE)return;

    rt->elm_saw_response=true;
    rt->elm_deadline_ms=now_ms+rt->elm.timeout_ms;

    if(kind==ARX_ELM_RX_NEED_FLOW_CONTROL){
        ArxCanFrame fc;
        if(arx_elm_transaction_build_flow_control(
            &rt->elm_transaction,&rt->elm,now_ms,&fc
        )){
            const ArxElmBus target=rt->elm_candidates[rt->elm_candidate_index];
            (void)elm_send_frame(rt,&fc,target,now_ms);
        }
        return;
    }

    if(kind==ARX_ELM_RX_PENDING)return;

    if(kind==ARX_ELM_RX_RAW_FRAME||kind==ARX_ELM_RX_PAYLOAD){
        elm_usb_emit_event(rt,&event);
        elm_finish_request(rt,true);
        return;
    }

    if(kind==ARX_ELM_RX_ERROR)(void)elm_try_next_candidate(rt,now_ms);
}

static void elm_tick(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt||!rt->elm_request_active)return;
    if(rt->elm_candidate_index>=rt->elm_candidate_count){
        rt->elm_request_active=false;
        elm_usb_no_data(rt);
        return;
    }

    if((int32_t)(now_ms-rt->elm_deadline_ms)>=0){
        (void)elm_try_next_candidate(rt,now_ms);
        return;
    }
    const ArxElmBus target=rt->elm_candidates[rt->elm_candidate_index];
    const bool room=target==ARX_ELM_BUS_C1
        ?rt->can_tx.queues[ARX_PRIORITY_HIGH].count<ARX_CAN_QUEUE_CAPACITY
        :rt->interchip.tx.count<ARX_INTERCHIP_QUEUE_SIZE;
    ArxCanFrame next;
    if(room&&arx_elm_transaction_next_tx(&rt->elm_transaction,now_ms,&next)){
        (void)elm_send_frame(rt,&next,target,now_ms);
    }
}

static void elm_process_line(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt||rt->elm_line_len==0u)return;
    rt->elm_line[rt->elm_line_len]='\0';

    if(rt->elm.echo){
        (void)elm_usb_queue(rt,rt->elm_line,rt->elm_line_len);
        elm_usb_line_end(rt);
    }

    const char *p=rt->elm_line;
    while(*p==' '||*p=='\t')p++;
    const bool at=(p[0]=='A'||p[0]=='a')&&(p[1]=='T'||p[1]=='t');

    if(rt->elm_request_active){
        (void)elm_usb_text(rt,"BUS BUSY");
        elm_usb_prompt(rt);
        rt->elm_line_len=0u;
        return;
    }

    if(at){
        char reply[192];
        const size_t n=arx_elm327_command(&rt->elm,p,reply,sizeof(reply));
        (void)elm_usb_queue(rt,reply,n);
        rt->elm_line_len=0u;
        return;
    }

    if(!arx_elm327_prepare_request(&rt->elm,p,&rt->elm_request)){
        (void)elm_usb_text(rt,"?");
        elm_usb_prompt(rt);
        rt->elm_line_len=0u;
        return;
    }

    rt->elm_candidate_count=arx_elm_router_candidates(
        &rt->elm_router,&rt->elm,rt->elm_candidates
    );
    rt->elm_candidate_index=0u;
    rt->elm_request_active=rt->elm_candidate_count>0u;
    if(!rt->elm_request_active||!elm_start_candidate(rt,now_ms)){
        rt->elm_request_active=false;
        elm_usb_no_data(rt);
    }

    rt->elm_line_len=0u;
}
#endif

#if ARX_COMPILE_C2 || ARX_COMPILE_BH
static void diag_slave_handle_link(
    ArxRuntime *rt,const ArxLinkFrame *link,uint32_t now_ms
) {
    if(!rt||!link)return;
    const uint8_t type=link->raw[1];

    if(type==ARX_LINK_CFG){
        rt->diag_filter_value=arx_link_can_id(link);
        rt->diag_filter_mask=
            ((uint32_t)link->raw[8]<<24u)|((uint32_t)link->raw[9]<<16u)|
            ((uint32_t)link->raw[10]<<8u)|(uint32_t)link->raw[11];
        rt->diag_timeout_ms=(uint16_t)(((uint16_t)link->raw[12]<<8u)|link->raw[13]);
        if(rt->diag_timeout_ms==0u)rt->diag_timeout_ms=ARX_ELM_DEFAULT_TIMEOUT;
        rt->diag_link_extended=(link->raw[2]&ARX_LINK_FLAG_EXTID)!=0u;
        return;
    }

    if(type==ARX_LINK_ARM){
        rt->diag_link_armed=(link->raw[2]&ARX_LINK_FLAG_ARM_ON)!=0u;
        rt->diag_deadline_ms=now_ms+rt->diag_timeout_ms;
        return;
    }

    if(type==ARX_LINK_REQ&&rt->diag_link_armed){
        ArxCanFrame frame={0};
        frame.bus=role_bus(rt->role);
        frame.id=arx_link_can_id(link);
        frame.extended_id=(link->raw[2]&ARX_LINK_FLAG_EXTID)!=0u;
        frame.dlc=arx_link_dlc(link);
        frame.timestamp_ms=now_ms;
        memcpy(frame.data,arx_link_data(link),frame.dlc);
        (void)enqueue_can(rt,&frame,ARX_PRIORITY_HIGH,now_ms);
        rt->diag_deadline_ms=now_ms+rt->diag_timeout_ms;
    }
}

static void diag_slave_on_can(
    ArxRuntime *rt,const ArxCanFrame *frame,uint32_t now_ms
) {
    if(!rt||!frame||!rt->diag_link_armed)return;
    if((int32_t)(now_ms-rt->diag_deadline_ms)>=0){
        rt->diag_link_armed=false;
        return;
    }
    if(frame->extended_id!=rt->diag_link_extended)return;
    if(rt->diag_filter_mask!=0u &&
       (frame->id&rt->diag_filter_mask)!=(rt->diag_filter_value&rt->diag_filter_mask))
        return;

    ArxLinkFrame response;
    arx_link_build_can(
        &response,ARX_LINK_TO_MASTER,ARX_LINK_RSP,
        frame->extended_id,frame->id,frame->data,frame->dlc,
        rt->diag_link_sequence++
    );
    if(ic_push_link(rt,&response))
        rt->diag_deadline_ms=now_ms+rt->diag_timeout_ms;
}
#endif

static bool dest_matches(ArxRuntimeRole role,uint8_t dest) {
    if(role==ARX_RUNTIME_C1)
        return dest==ARX_IC_TO_C1 || dest==ARX_IC_TO_C1_BH ||
               dest==ARX_IC_TO_C1_C2 || dest==ARX_IC_ALL_SLEEP ||
               dest==ARX_IC_ALL_CLEAR_DTC;
    if(role==ARX_RUNTIME_C2)
        return dest==ARX_IC_TO_C2 || dest==ARX_IC_TO_C2_BH ||
               dest==ARX_IC_TO_C1_C2 || dest==ARX_IC_ALL_SLEEP ||
               dest==ARX_IC_ALL_CLEAR_DTC;
    return dest==ARX_IC_TO_BH || dest==ARX_IC_TO_BH_PARAM_TEXT ||
           dest==ARX_IC_TO_C2_BH || dest==ARX_IC_TO_C1_BH ||
           dest==ARX_IC_BH_GET_STATUS || dest==ARX_IC_BH_CHIME ||
           dest==ARX_IC_ALL_SLEEP || dest==ARX_IC_ALL_CLEAR_DTC;
}

#if ARX_COMPILE_C1
static uint8_t raw_acc_status(const ArxVehicleState *s) {
    switch(s->acc_state){
        case ARX_ACC_OFF:return 0u;
        case ARX_ACC_ENABLED:return 1u;
        case ARX_ACC_ENGAGED:return 2u;
        case ARX_ACC_BRAKE_ONLY:return 3u;
        case ARX_ACC_OVERRIDE:return 4u;
        case ARX_ACC_CANCEL:return 5u;
        case ARX_ACC_SUGGESTION_ENGAGED:return 6u;
        case ARX_ACC_SUGGESTION_OVERRIDE:return 7u;
        default:return 0u;
    }
}
#endif

static void update_engine_state(ArxRuntime *rt,uint32_t now_ms) {
    const bool running=(rt->vehicle.valid_mask&ARX_VS_ENGINE_RPM) &&
                       rt->vehicle.engine_rpm>=400u;
    if(running&&!rt->engine_running){
        rt->engine_running=true;
        rt->engine_running_since_ms=now_ms;
    }else if(!running){
        rt->engine_running=false;
        rt->engine_running_since_ms=0u;
    }
}

#if ARX_COMPILE_C1
static bool engine_running_long_enough(const ArxRuntime *rt,uint32_t now_ms) {
    return rt&&rt->engine_running&&rt->engine_running_since_ms &&
           now_ms-rt->engine_running_since_ms>=5000u;
}
#endif


static void runtime_bind_preferences(ArxRuntime *rt,uint32_t now_ms,bool apply_usb) {
    rt->start_stop.enabled=rt->config.smart_start_stop_enabled;
    rt->shift.enabled=rt->config.shift_indicator_enabled;
    rt->shift.ipc_my23=rt->config.ipc_my23;
    rt->shift.base_threshold_rpm=rt->config.shift_threshold_rpm;
    rt->drive_style.enabled=rt->config.esc_tc_customizer_enabled;
    rt->drive_style.show_race_mask=rt->config.race_mask_enabled;
    rt->dpf.enabled=rt->config.regeneration_alert_enabled;
    rt->acc.virtual_pad_enabled=rt->config.acc_virtual_pad_enabled;
    rt->acc.has_virtual_pad_enabled=rt->config.has_virtual_pad_enabled;
    rt->acc.autostart_mode=(ArxAccAutostartMode)rt->config.acc_autostart_mode;
    rt->awd.enabled=rt->config.awd_control_enabled;
    rt->brake.enabled=rt->config.front_brake_override_enabled;
    rt->brake.launch_torque_nm=rt->config.launch_torque_threshold_nm;
    rt->exhaust.enabled=rt->config.exhaust_flap_enabled;
    rt->seatbelt.feature_enabled=true;
    rt->immobilizer.enabled=rt->config.immobilizer_enabled;
    rt->park_mute.enabled=rt->config.front_park_mute_enabled;
    rt->park_mirror.enabled=rt->config.park_mirror_enabled;
    rt->windows.close_mode=(ArxWindowMode)rt->config.close_windows_mode;
    rt->windows.open_mode=(ArxWindowMode)rt->config.open_windows_mode;
    rt->route.enabled=rt->config.route_messages_enabled;
    (void)arx_pedal_set_mode(&rt->pedal,(ArxPedalMode)rt->config.pedal_mode);
    (void)arx_pedal_set_power(&rt->pedal,rt->config.pedal_power);
    rt->leds.enabled=rt->config.led_strip_enabled;

    rt->menu_caps.read_faults_available=rt->config.read_faults_enabled;
    rt->menu_caps.clear_faults_available=rt->config.clear_faults_enabled;
    rt->menu_caps.dyno_available=rt->config.dyno_enabled;
    rt->menu_caps.esc_tc_available=rt->config.esc_tc_customizer_enabled;
    rt->menu_caps.brake_available=rt->config.front_brake_override_enabled;
    rt->menu_caps.awd_available=rt->config.awd_control_enabled;
    rt->menu_caps.has_available=rt->config.has_virtual_pad_enabled;
    rt->menu_caps.exhaust_available=rt->config.exhaust_flap_enabled;

    if(apply_usb){
        if(rt->config.sniffer_enabled){
            rt->sniffer.enabled=true;
            if(!rt->sniffer.in_use) arx_sniffer_start(&rt->sniffer,now_ms);
        }else{
            arx_sniffer_stop(&rt->sniffer);
            rt->sniffer.enabled=false;
        }

#if ARX_COMPILE_C1
        if(rt->role==ARX_RUNTIME_C1){
            rt->elm.enabled=rt->config.elm327_enabled;
            if(rt->config.elm327_enabled)
                (void)arx_usb_mode_request(&rt->usb_mode,ARX_USB_MODE_DIAGNOSTIC,now_ms);
            else if(rt->config.sniffer_enabled)
                (void)arx_usb_mode_request(&rt->usb_mode,ARX_USB_MODE_SNIFFER,now_ms);
            else
                (void)arx_usb_mode_request(&rt->usb_mode,ARX_USB_MODE_NONE,now_ms);
        }else
#endif
        {
            /* The physical reference-firmware backup proves that BH/C2 expose
               USB MSC when they are not temporarily used as CAN sniffers. */
            (void)arx_usb_mode_request(
                &rt->usb_mode,
                rt->config.sniffer_enabled?ARX_USB_MODE_SNIFFER:ARX_USB_MODE_LEGACY_MSC,
                now_ms
            );
        }
    }
}

#if ARX_COMPILE_C1
static bool menu_render(ArxRuntime *rt);

static bool telemetry_definition_index(
    const char *key,
    const ArxTelemetryDefinition **definition,
    size_t *index
) {
    size_t count=0u;
    const ArxTelemetryDefinition *db=arx_telemetry_diesel(&count);
    const ArxTelemetryDefinition *d=arx_telemetry_find(db,count,key);
    if(!d)return false;
    const size_t i=(size_t)(d-db);
    if(i>=ARX_RUNTIME_TELEMETRY_CACHE_MAX)return false;
    if(definition)*definition=d;
    if(index)*index=i;
    return true;
}

static bool telemetry_native_value(
    const ArxRuntime *rt,
    const char *key,
    float *value
) {
    if(!rt||!key||!value)return false;

    if(!strcmp(key,"oil_pressure")){
        if(!(rt->vehicle.valid_mask&ARX_VS_OIL_PRESSURE))return false;
        *value=rt->vehicle.oil_pressure_bar;return true;
    }
    if(!strcmp(key,"engine_power")){
        if((rt->vehicle.valid_mask&(ARX_VS_ENGINE_RPM|ARX_VS_TORQUE))!=(ARX_VS_ENGINE_RPM|ARX_VS_TORQUE))
            return false;
        *value=(float)rt->vehicle.engine_torque_nm*(float)rt->vehicle.engine_rpm*0.000142378f;
        return true;
    }
    if(!strcmp(key,"engine_torque")){
        if(!(rt->vehicle.valid_mask&ARX_VS_TORQUE))return false;
        *value=(float)rt->vehicle.engine_torque_nm;return true;
    }
    if(!strcmp(key,"oil_temperature")){
        if(!(rt->vehicle.valid_mask&ARX_VS_OIL_TEMP))return false;
        *value=rt->vehicle.oil_temperature_c;return true;
    }
    if(!strcmp(key,"gear")){
        if(!(rt->vehicle.valid_mask&ARX_VS_GEAR))return false;
        *value=(float)rt->vehicle.current_gear;return true;
    }
    if(!strcmp(key,"speed")){
        if(!(rt->vehicle.valid_mask&ARX_VS_SPEED))return false;
        *value=rt->vehicle.vehicle_speed_kmh;return true;
    }
    if(!strcmp(key,"dpf_regen_mode")){
        *value=(float)rt->dpf.last_regen_mode;return true;
    }
    if(!strcmp(key,"battery_current")){
        if(!(rt->vehicle.valid_mask&ARX_VS_BATTERY_CURR))return false;
        *value=rt->vehicle.battery_current_a;return true;
    }
    if(!strcmp(key,"seatbelt_alarm")){
        if(rt->seatbelt.state==ARX_SEATBELT_ENABLED){*value=0.0f;return true;}
        if(rt->seatbelt.state==ARX_SEATBELT_DISABLED){*value=1.0f;return true;}
        return false;
    }
    if(!strcmp(key,"performance_0_100")){
        *value=rt->performance.zero_to_100_s;return true;
    }
    if(!strcmp(key,"performance_100_200")){
        *value=rt->performance.hundred_to_200_s;return true;
    }
    if(!strcmp(key,"best_0_100")){
        *value=rt->performance.best_zero_to_100_s;return true;
    }
    if(!strcmp(key,"best_100_200")){
        *value=rt->performance.best_hundred_to_200_s;return true;
    }
    if(!strcmp(key,"dna_mode")){
        if(!(rt->vehicle.valid_mask&ARX_VS_DNA))return false;
        *value=(float)rt->vehicle.dna_mode;return true;
    }
    if(!strcmp(key,"pedal_map")){
        if(rt->pedal.applied_map==ARX_PEDAL_MAP_UNKNOWN)return false;
        *value=(float)rt->pedal.applied_map;return true;
    }
    return false;
}

static bool telemetry_value(ArxRuntime *rt,const char *key,float *value) {
    const ArxTelemetryDefinition *d=0;
    size_t index=0u;
    if(!rt||!key||!value||!telemetry_definition_index(key,&d,&index))return false;

    if(d->source==ARX_SIGNAL_UDS){
        if(rt->telemetry_valid[index]){
            *value=rt->telemetry_values[index];
            return true;
        }
        /* The reference firmware uses UDS 0x19BD as the diesel SoC source, but retain the
           native 0x41A value as a startup fallback until the first UDS reply. */
        if(!strcmp(key,"battery_soc")&&(rt->vehicle.valid_mask&ARX_VS_BATTERY_SOC)){
            *value=rt->vehicle.battery_soc_percent;
            return true;
        }
        return false;
    }
    return telemetry_native_value(rt,key,value);
}

static bool telemetry_render_current_page(ArxRuntime *rt,char text[ARX_MENU_TEXT_LEN+1u]) {
    size_t count=0u;
    const ArxTelemetryPage *pages=arx_telemetry_diesel_pages(&count);
    if(!rt||!text||count==0u)return false;
    const ArxTelemetryPage *p=&pages[(size_t)rt->menu.param_page%count];
    float a=0.0f,b=0.0f;
    const bool av=telemetry_value(rt,p->primary_key,&a);
    const bool bv=!strcmp(p->primary_key,p->secondary_key)
        ?av:telemetry_value(rt,p->secondary_key,&b);
    if(!strcmp(p->primary_key,p->secondary_key))b=a;
    return arx_telemetry_format_page(p,a,av,b,bv,text);
}

static void telemetry_accept_response(
    ArxRuntime *rt,
    const ArxCanFrame *frame
) {
    if(!rt||!frame||!frame->extended_id||!rt->config.diesel_profile)return;

    size_t page_count=0u;
    const ArxTelemetryPage *pages=arx_telemetry_diesel_pages(&page_count);
    if(!page_count)return;
    const ArxTelemetryPage *p=&pages[(size_t)rt->menu.param_page%page_count];
    const char *keys[2]={p->primary_key,p->secondary_key};
    bool updated=false;

    for(unsigned slot=0u;slot<2u;slot++){
        if(slot==1u&&!strcmp(keys[0],keys[1]))break;
        const ArxTelemetryDefinition *d=0;
        size_t index=0u;
        if(!telemetry_definition_index(keys[slot],&d,&index)||d->source!=ARX_SIGNAL_UDS)
            continue;
        float value=0.0f;
        if(arx_telemetry_decode_response(d,frame,&value)){
            rt->telemetry_values[index]=value;
            rt->telemetry_valid[index]=1u;
            updated=true;
        }
    }
    if(updated&&rt->menu.visible&&rt->menu.level==ARX_MENU_LEVEL_SUB&&
       rt->menu.main_page==1u)
        (void)menu_render(rt);
}

static void telemetry_poll_current_page(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt||rt->elm_request_active||!rt->config.telemetry_enabled||!rt->config.diagnostics_enabled||
       !rt->config.diesel_profile||!rt->menu.visible||
       rt->menu.level!=ARX_MENU_LEVEL_SUB||rt->menu.main_page!=1u)return;
    if(rt->telemetry_last_poll_ms&&now_ms-rt->telemetry_last_poll_ms<500u)return;
    rt->telemetry_last_poll_ms=now_ms;

    size_t page_count=0u;
    const ArxTelemetryPage *pages=arx_telemetry_diesel_pages(&page_count);
    if(!page_count)return;
    const ArxTelemetryPage *p=&pages[(size_t)rt->menu.param_page%page_count];

    unsigned slot=rt->telemetry_poll_slot&1u;
    if(!strcmp(p->primary_key,p->secondary_key))slot=0u;
    const char *key=slot?p->secondary_key:p->primary_key;

    const ArxTelemetryDefinition *d=0;
    size_t index=0u;
    if(telemetry_definition_index(key,&d,&index)&&d->source==ARX_SIGNAL_UDS){
        ArxCanFrame request;
        if(arx_telemetry_build_request(d,ARX_BUS_C1,now_ms,&request))
            (void)enqueue_can(rt,&request,ARX_PRIORITY_LOW,now_ms);
    }

    if(strcmp(p->primary_key,p->secondary_key))
        rt->telemetry_poll_slot^=1u;
    else
        rt->telemetry_poll_slot=0u;

    (void)menu_render(rt);
}

static bool menu_send_text(ArxRuntime *rt,const char text[ARX_MENU_TEXT_LEN+1u]) {
    if(!rt||rt->role!=ARX_RUNTIME_C1||!text)return false;
    uint8_t p[ARX_INTERCHIP_FRAME_SIZE];
    p[0]=ARX_IC_TO_BH_PARAM_TEXT;
    memcpy(&p[1],text,ARX_MENU_TEXT_LEN);
    return ic_push(rt,p,sizeof(p));
}

static bool menu_render(ArxRuntime *rt) {
    if(!rt||rt->role!=ARX_RUNTIME_C1)return false;
    char text[ARX_MENU_TEXT_LEN+1u];
    memset(text,' ',ARX_MENU_TEXT_LEN);text[ARX_MENU_TEXT_LEN]='\0';

    if(!rt->menu.visible)return menu_send_text(rt,text);

    if(rt->menu.level==ARX_MENU_LEVEL_MAIN){
        if(!arx_menu_main_text(rt->menu.main_page,text))return false;
        if(rt->menu.main_page==15u){
            const char *mh=rt->max_hold.enabled?"Max Hold ON":"Max Hold OFF";
            memset(text,' ',ARX_MENU_TEXT_LEN);
            memcpy(text,mh,strlen(mh));
            text[ARX_MENU_TEXT_LEN]='\0';
        }
        return menu_send_text(rt,text);
    }

    if(rt->menu.main_page==9u){
        if(!arx_menu_setup_text(rt->menu.setup_page,&rt->config,text))return false;
        return menu_send_text(rt,text);
    }

    if(rt->menu.main_page==10u){
        const uint8_t page=rt->menu.param_page;
        if(page==0u){
            const char *x="SAVE&EXIT";memcpy(text,x,9u);
        }else{
            const uint8_t idx=(uint8_t)(page-1u);
            text[0]=rt->visible_params[idx]?'X':'O';
            memcpy(&text[1]," PARAM ",7u);
            const unsigned page_no=(unsigned)idx+1u;
            text[8]=(char)('0'+((page_no/10u)%10u));
            text[9]=(char)('0'+(page_no%10u));
        }
        return menu_send_text(rt,text);
    }

    if(rt->menu.main_page==2u){
        const char *x=(rt->faults.state==ARX_FAULTS_COMPLETE)?"DTC READY":"DTC WAIT";
        memcpy(text,x,strlen(x));
        return menu_send_text(rt,text);
    }

    if(rt->menu.main_page==1u){
        if(rt->config.diesel_profile)
            (void)telemetry_render_current_page(rt,text);
        else{
            const char *x="Gasoline pending";
            memcpy(text,x,strlen(x));
        }
        return menu_send_text(rt,text);
    }

    return menu_send_text(rt,text);
}

static void menu_apply_setup_effect(ArxRuntime *rt,ArxMenuEffect effect,uint32_t now_ms) {
    if(!rt)return;

    runtime_bind_preferences(rt,now_ms,false);

    if(effect&ARX_MENU_EFFECT_PEDAL_RESYNC)
        rt->pedal.applied_map=ARX_PEDAL_MAP_UNKNOWN;

    if(effect&ARX_MENU_EFFECT_CAPTURE_PARK_MIRROR){
        const uint8_t p[2]={ARX_IC_TO_BH,ARX_IC_BH_MIRROR_STORE};
        (void)ic_push(rt,p,sizeof(p));
    }

    if(effect&ARX_MENU_EFFECT_SAVE_CONFIG){
        if(rt->ops.persist_config)(void)rt->ops.persist_config(&rt->config,rt->ops.user);
        runtime_bind_preferences(rt,now_ms,true);
    }

    if(effect&ARX_MENU_EFFECT_SYNC_CONFIG)
        arx_runtime_queue_config_sync(rt);
}

static void menu_activate_main(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt)return;
    ArxCanFrame out;
    const bool stationary=(rt->vehicle.valid_mask&ARX_VS_SPEED)&&rt->vehicle.vehicle_speed_kmh==0.0f;

    switch(rt->menu.main_page){
        case 1u:
            rt->menu.level=ARX_MENU_LEVEL_SUB;rt->menu.param_page=0u;break;
        case 2u:
            if(rt->config.read_faults_enabled&&arx_faults_start_read(&rt->faults,now_ms,&out)){
                (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);
                rt->menu.level=ARX_MENU_LEVEL_SUB;
            }
            break;
        case 3u:
            if(rt->config.clear_faults_enabled){
                if(arx_faults_build_clear(0x10u,&out))(void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);
                const uint8_t p[1]={ARX_IC_ALL_CLEAR_DTC};(void)ic_push(rt,p,sizeof(p));
            }
            break;
        case 5u:
            if(stationary){const uint8_t p[2]={ARX_IC_TO_C2,ARX_IC_C2_DYNO_TOGGLE};(void)ic_push(rt,p,sizeof(p));}
            break;
        case 6u:{
            const uint8_t p[2]={ARX_IC_TO_C2,ARX_IC_C2_ESC_TC_TOGGLE};(void)ic_push(rt,p,sizeof(p));break;
        }
        case 7u:{
            uint8_t cmd=ARX_IC_C2_BRAKE_NORMAL;
            if(!rt->remote_brake_forced&&stationary&&rt->remote_dyno_active)cmd=ARX_IC_C2_BRAKE_FORCE;
            const uint8_t p[2]={ARX_IC_TO_C2,cmd};(void)ic_push(rt,p,sizeof(p));break;
        }
        case 8u:
            if(rt->awd.state==ARX_AWD_NORMAL){if(stationary)(void)arx_awd_request_disable(&rt->awd,true);}
            else arx_awd_request_normal(&rt->awd);
            break;
        case 9u:
            rt->menu.level=ARX_MENU_LEVEL_SUB;rt->menu.setup_page=0u;break;
        case 10u:
            rt->menu.level=ARX_MENU_LEVEL_SUB;rt->menu.param_page=0u;break;
        case 11u:
            arx_acc_request_has_press(&rt->acc);
            {const uint8_t p[2]={ARX_IC_TO_C2,ARX_IC_C2_HAS_TOGGLE};(void)ic_push(rt,p,sizeof(p));}
            break;
        case 12u:arx_qv_exhaust_toggle(&rt->exhaust);break;
        case 13u:
            if(rt->ops.save_log)(void)rt->ops.save_log(rt->ops.user);
            {const uint8_t p[2]={ARX_IC_TO_C2_BH,ARX_IC_SHARED_SAVE_LOG};(void)ic_push(rt,p,sizeof(p));}
            break;
        case 14u:
            arx_performance_reset_best(&rt->performance);
            if(rt->ops.persist_performance)(void)rt->ops.persist_performance(
                rt->performance.best_zero_to_100_s,rt->performance.best_hundred_to_200_s,rt->ops.user);
            break;
        case 15u:
            arx_max_hold_set_enabled(&rt->max_hold,!rt->max_hold.enabled);rt->menu.max_hold=rt->max_hold.enabled;break;
        default:break;
    }
    (void)menu_render(rt);
}

static void menu_activate(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt)return;
    if(rt->menu.level==ARX_MENU_LEVEL_MAIN){menu_activate_main(rt,now_ms);return;}

    if(rt->menu.main_page==9u){
        ArxMenuEffect e=arx_menu_activate_setup(&rt->menu,&rt->config);
        menu_apply_setup_effect(rt,e,now_ms);
    }else if(rt->menu.main_page==10u){
        if(rt->menu.param_page==0u){
            if(rt->ops.persist_visibility)
                (void)rt->ops.persist_visibility(rt->visible_params,sizeof(rt->visible_params),rt->ops.user);
            rt->menu.level=ARX_MENU_LEVEL_MAIN;
        }else{
            uint8_t idx=(uint8_t)(rt->menu.param_page-1u);
            if(idx<sizeof(rt->visible_params))rt->visible_params[idx]=!rt->visible_params[idx];
        }
    }else if(rt->menu.main_page==2u){
        arx_faults_init(&rt->faults,0x18DA40F1u,0x18DAF140u);
        rt->menu.level=ARX_MENU_LEVEL_MAIN;
    }else{
        rt->menu.level=ARX_MENU_LEVEL_MAIN;
    }
    (void)menu_render(rt);
}

static void menu_on_2fa(ArxRuntime *rt,const ArxCanFrame *frame,uint32_t now_ms) {
    if(!rt||!frame||frame->dlc<1u)return;
    if(!rt->cruise_control_disabled||raw_acc_status(&rt->vehicle)!=0u)return;

    const uint8_t param_count=rt->config.diesel_profile?ARX_DIESEL_DASHBOARD_PAGE_COUNT:46u;
    ArxMenuInputEvent e=arx_menu_input_on_button(
        &rt->menu_input,&rt->menu,&rt->menu_caps,frame->data[0],param_count,param_count
    );
    if(e==ARX_MENU_INPUT_RENDER)(void)menu_render(rt);
    else if(e==ARX_MENU_INPUT_VISIBILITY_CHANGED)(void)menu_render(rt);
    else if(e==ARX_MENU_INPUT_ACTIVATE)menu_activate(rt,now_ms);
}

static void menu_engine_visibility(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt||rt->role!=ARX_RUNTIME_C1)return;
    if(!rt->engine_running){
        if(rt->menu.visible&&rt->menu_shutdown_requested_ms==0u)
            rt->menu_shutdown_requested_ms=now_ms;
        if(rt->menu.visible&&rt->menu_shutdown_requested_ms&&
           now_ms-rt->menu_shutdown_requested_ms>39000u){
            rt->menu.visible=false;
            rt->menu_was_visible_before_engine_off=true;
            rt->menu_shutdown_requested_ms=0u;
            (void)menu_render(rt);
        }
    }else{
        rt->menu_shutdown_requested_ms=0u;
        if(rt->menu_was_visible_before_engine_off){
            rt->menu.visible=true;
            rt->menu_was_visible_before_engine_off=false;
            (void)menu_render(rt);
        }
    }
}
#endif

void arx_runtime_init(
    ArxRuntime *rt,
    ArxRuntimeRole role,
    const ArxRuntimeOps *ops
) {
    if(!rt)return;
    memset(rt,0,sizeof(*rt));
    rt->role=role;
    if(ops)rt->ops=*ops;

    arx_config_defaults(&rt->config);
    arx_vehicle_state_init(&rt->vehicle);
    arx_can_init(&rt->can_tx);
    arx_interchip_init(&rt->interchip,role_interchip(role));
    arx_power_init(&rt->power);
    arx_usb_mode_init(&rt->usb_mode);

#if ARX_COMPILE_C1
    arx_start_stop_init(&rt->start_stop);
    arx_dynamic_shift_init(&rt->shift);
    arx_dpf_alert_init(&rt->dpf);
    arx_acc_init(&rt->acc);
    arx_awd_init(&rt->awd);
    arx_qv_exhaust_init(&rt->exhaust);
    arx_seatbelt_init(&rt->seatbelt);
    arx_faults_init(&rt->faults,0x18DA40F1u,0x18DAF140u);
    arx_immobilizer_init(&rt->immobilizer);
    arx_windows_init(&rt->windows);
    arx_pedal_init(&rt->pedal);
    arx_led_strip_init(&rt->leds);
    arx_route_service_init(&rt->route);
    arx_menu_init(&rt->menu);
    arx_menu_input_init(&rt->menu_input);
    rt->cruise_control_disabled=true;
    memset(rt->visible_params,1,sizeof(rt->visible_params));
    arx_performance_init(&rt->performance);
    arx_max_hold_init(&rt->max_hold);
    if(role==ARX_RUNTIME_C1){
        arx_elm327_init(&rt->elm);
        arx_elm_router_init(&rt->elm_router);
        arx_elm_transaction_init(&rt->elm_transaction);
    }
#endif
#if ARX_COMPILE_C2
    arx_dyno_init(&rt->dyno);
    arx_acc_init(&rt->acc);
    arx_brake_override_init(&rt->brake);
    arx_park_mute_init(&rt->park_mute);
#endif
#if ARX_COMPILE_BH
    arx_park_mirror_init(&rt->park_mirror);
    arx_dashboard_init(&rt->dashboard);
#endif
#if ARX_COMPILE_C1 || ARX_COMPILE_C2 || ARX_COMPILE_BH
    arx_drive_style_init(&rt->drive_style);
    arx_sniffer_init(&rt->sniffer);
#endif

    arx_runtime_apply_config(rt,&rt->config,0u);
}

void arx_runtime_apply_config(
    ArxRuntime *rt,
    const ArxRuntimeConfig *config,
    uint32_t now_ms
) {
    if(!rt||!config)return;
    rt->config=*config;
    (void)arx_config_sanitize(&rt->config);
    runtime_bind_preferences(rt,now_ms,true);
}

static void handle_common_state_updates(
    ArxRuntime *rt,
    const ArxCanFrame *frame,
    uint32_t now_ms
) {
    arx_decode_frame(frame,&rt->vehicle);
    update_engine_state(rt,now_ms);

    if(frame->id==0x0FCu&&!frame->extended_id){
        if(rt->role==ARX_RUNTIME_C1)
            arx_start_stop_on_engine_rpm(&rt->start_stop,rt->vehicle.engine_rpm,now_ms);
        if(rt->role==ARX_RUNTIME_C1)
            arx_awd_on_engine_rpm(&rt->awd,rt->vehicle.engine_rpm);
        if(rt->role==ARX_RUNTIME_C2)
            arx_brake_on_engine_rpm(&rt->brake,rt->vehicle.engine_rpm);
        if(rt->role==ARX_RUNTIME_C1)
            arx_qv_exhaust_on_engine_rpm(&rt->exhaust,rt->vehicle.engine_rpm);
    }

    if((frame->id==0x384u&&rt->role==ARX_RUNTIME_C1) ||
       (frame->id==0x46Cu&&rt->role==ARX_RUNTIME_BH)){
        arx_drive_style_update_actual_mode(&rt->drive_style,rt->vehicle.dna_mode);
    }
}

void arx_runtime_on_can(
    ArxRuntime *rt,
    const ArxCanFrame *frame,
    uint32_t now_ms
) {
    if(!rt||!frame)return;
    if(frame->bus!=role_bus(rt->role))return;

    rt->rx_frames++;
    arx_power_note_can_rx(&rt->power,now_ms);

    if(rt->sniffer.enabled)(void)arx_sniffer_push(&rt->sniffer,frame);

    handle_common_state_updates(rt,frame,now_ms);

#if ARX_COMPILE_C1
    if(rt->role==ARX_RUNTIME_C1&&rt->elm_request_active&&
       rt->elm_candidate_index<rt->elm_candidate_count&&
       rt->elm_candidates[rt->elm_candidate_index]==ARX_ELM_BUS_C1)
        elm_handle_response(rt,frame,now_ms);
#endif

#if ARX_COMPILE_C2 || ARX_COMPILE_BH
    if(rt->role==ARX_RUNTIME_C2||rt->role==ARX_RUNTIME_BH)
        diag_slave_on_can(rt,frame,now_ms);
#endif

#if ARX_COMPILE_C1
    if(rt->role==ARX_RUNTIME_C1&&!frame->extended_id&&frame->id==0x101u&&
       (rt->vehicle.valid_mask&ARX_VS_SPEED)){
        arx_performance_update(&rt->performance,rt->vehicle.vehicle_speed_kmh,now_ms);
        arx_max_hold_update(&rt->max_hold,0u,rt->vehicle.vehicle_speed_kmh);
        if(rt->performance.best_dirty&&rt->ops.persist_performance){
            if(rt->ops.persist_performance(rt->performance.best_zero_to_100_s,
                rt->performance.best_hundred_to_200_s,rt->ops.user))
                rt->performance.best_dirty=false;
        }
    }
#endif

    ArxCanFrame out;

#if ARX_COMPILE_C1
    if(rt->role==ARX_RUNTIME_C1){
        if(!frame->extended_id&&frame->id==0x4B1u&&frame->dlc>=6u){
            rt->template_4b1=*frame;
            rt->template_4b1_valid=true;
        }

        if(!frame->extended_id&&frame->id==0x1EFu&&frame->dlc==8u){
            rt->template_1ef=*frame;
            rt->template_1ef_valid=true;
            arx_windows_observe_rf(&rt->windows,frame,now_ms);
        }

        if(!frame->extended_id&&frame->id==0x226u)
            (void)arx_start_stop_observe_status_226(&rt->start_stop,frame);

        if(!frame->extended_id&&frame->id==0x5A5u&&frame->dlc>=1u)
            rt->cruise_control_disabled=((frame->data[0]&0x80u)==0u);

        if(!frame->extended_id&&frame->id==0x384u){
            arx_drive_style_observe_384_c1(&rt->drive_style,frame);
            if(arx_drive_style_transform_384_c1(&rt->drive_style,frame,&out))
                (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);
        }

        if(!frame->extended_id&&frame->id==0x4AFu&&
           arx_drive_style_transform_4af(&rt->drive_style,frame,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

        if(!frame->extended_id&&frame->id==0x5A8u&&
           arx_drive_style_transform_5a8(&rt->drive_style,frame,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

        if(!frame->extended_id&&frame->id==0x2EDu&&
           arx_dynamic_shift_build_frame(
               &rt->shift,frame,rt->vehicle.engine_rpm,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

        if(!frame->extended_id&&frame->id==0x5AEu){
            bool started=false,ended=false;
            (void)arx_dpf_alert_on_5ae(&rt->dpf,frame,&started,&ended);
            if(arx_dpf_alert_build_visual(&rt->dpf,frame,&out))
                (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);
            if(started&&rt->dpf.sound_alert_enabled){
                const uint8_t p[1]={ARX_IC_BH_CHIME};
                (void)ic_push(rt,p,sizeof(p));
            }
        }

        if(!frame->extended_id&&frame->id==0x2FAu){
            menu_on_2fa(rt,frame,now_ms);
            const bool stationary=(rt->vehicle.valid_mask&ARX_VS_SPEED) &&
                                  rt->vehicle.vehicle_speed_kmh==0.0f;
            if(arx_acc_transform_2fa(
                &rt->acc,frame,raw_acc_status(&rt->vehicle),stationary,
                rt->vehicle.acc_brake_intervention,now_ms,&out))
                (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);
        }

        if(!frame->extended_id&&frame->id==0x412u&&frame->dlc==5u)
            (void)arx_led_strip_update_pedal(&rt->leds,frame->data[3],now_ms);
        if(!frame->extended_id&&frame->id==0x2EFu)
            arx_led_strip_set_gear(&rt->leds,rt->vehicle.current_gear);

        if(rt->config.odometer_blink_mask_enabled&&
           arx_odometer_build_no_blink(frame,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

        if(frame->extended_id&&frame->id==0x18DABAF1u)
            (void)arx_route_service_on_request(&rt->route,frame);
        else if(rt->route.pending&&
                arx_route_service_capture(&rt->route,frame,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_LOW,now_ms);

        if(frame->extended_id&&frame->id==0x18DAF140u){
            if(arx_faults_on_response(&rt->faults,frame,now_ms,&out))
                (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);
        }

        if(frame->extended_id&&frame->id==0x18DAF160u){
            if(arx_seatbelt_on_response(&rt->seatbelt,frame,now_ms,&out))
                (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);
        }

        if(frame->extended_id)
            telemetry_accept_response(rt,frame);

        (void)arx_immobilizer_observe(
            &rt->immobilizer,frame,engine_running_long_enough(rt,now_ms),now_ms
        );
    }
#endif

#if ARX_COMPILE_C2
    if(rt->role==ARX_RUNTIME_C2){
        arx_park_mute_observe(&rt->park_mute,frame);
        arx_park_mute_evaluate(&rt->park_mute);

        if(!frame->extended_id&&frame->id==0x2FAu&&
           arx_acc_transform_2fa(&rt->acc,frame,0u,false,false,now_ms,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

        if(!frame->extended_id&&frame->id==0x384u&&
           arx_drive_style_transform_384_c2(&rt->drive_style,frame,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

        if(!frame->extended_id&&frame->id==0x4AFu&&
           arx_drive_style_transform_4af(&rt->drive_style,frame,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

        if(frame->extended_id&&frame->id==0x18DAF128u){
            if(arx_dyno_on_response(&rt->dyno,frame,now_ms,&out))
                (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);
        }
    }
#endif

#if ARX_COMPILE_BH
    if(rt->role==ARX_RUNTIME_BH){
        if(!frame->extended_id&&frame->id==0x5ACu&&frame->dlc>=4u){
            rt->template_5ac=*frame;
            rt->template_5ac_valid=true;
            if(rt->bh_chime_requested){
                out=*frame;
                out.data[0]=(uint8_t)(out.data[0]&0x3Fu);
                out.data[1]=(uint8_t)((out.data[1]&0x3Fu)|0x40u);
                out.data[3]|=0xE0u;
                if(enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms))
                    rt->bh_chime_requested=false;
            }
        }

        if(!frame->extended_id&&frame->id==0x46Cu&&
           arx_drive_style_transform_46c(&rt->drive_style,frame,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

        if(!frame->extended_id&&frame->id==0x25Au&&
           arx_drive_style_transform_25a(&rt->drive_style,frame,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

        if(rt->config.odometer_blink_mask_enabled&&
           arx_odometer_build_no_blink(frame,&out))
            (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

        const bool was_capture_park=rt->park_mirror.capture_park;
        const bool was_capture_normal=rt->park_mirror.capture_normal;
        (void)arx_park_mirror_observe_position(&rt->park_mirror,frame);
        if(rt->ops.persist_mirror&&
           ((was_capture_park&&!rt->park_mirror.capture_park)||
            (was_capture_normal&&!rt->park_mirror.capture_normal))){
            ArxMirrorStorage m={
                .left_park_h=rt->park_mirror.park.left_h,
                .left_park_v=rt->park_mirror.park.left_v,
                .right_park_h=rt->park_mirror.park.right_h,
                .right_park_v=rt->park_mirror.park.right_v,
                .normal_position_uninitialized=!rt->park_mirror.calibrated_normal,
                .left_normal_h=rt->park_mirror.normal.left_h,
                .left_normal_v=rt->park_mirror.normal.left_v,
                .right_normal_h=rt->park_mirror.normal.right_h,
                .right_normal_v=rt->park_mirror.normal.right_v
            };
            (void)rt->ops.persist_mirror(&m,rt->ops.user);
        }
        arx_park_mirror_update(
            &rt->park_mirror,rt->vehicle.current_gear,rt->vehicle.turn_indicator,
            rt->vehicle.engine_rpm,now_ms
        );
    }
#endif

}

void arx_runtime_on_pedal_reply(ArxRuntime *rt,uint8_t reply_byte) {
#if ARX_COMPILE_C1
    if(rt&&rt->role==ARX_RUNTIME_C1)arx_pedal_on_reply(&rt->pedal,reply_byte);
#else
    (void)rt;(void)reply_byte;
#endif
}

void arx_runtime_usb_configured(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt)return;
    arx_usb_mode_note_configured(&rt->usb_mode,now_ms);
}

void arx_runtime_usb_command(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt)return;
    arx_usb_mode_note_command(&rt->usb_mode,now_ms);
}

void arx_runtime_usb_rx(
    ArxRuntime *rt,const uint8_t *data,size_t length,uint32_t now_ms
) {
    if(!rt||!data||length==0u)return;
    arx_runtime_usb_command(rt,now_ms);

#if ARX_COMPILE_C1
    if(rt->role!=ARX_RUNTIME_C1||
       rt->usb_mode.mode!=ARX_USB_MODE_DIAGNOSTIC)return;

    for(size_t i=0u;i<length;i++){
        const uint8_t ch=data[i];
        if(ch=='\n')continue;
        if(ch=='\r'){
            elm_process_line(rt,now_ms);
            continue;
        }
        if(ch==0x08u||ch==0x7Fu){
            if(rt->elm_line_len)rt->elm_line_len--;
            continue;
        }
        if(ch<0x20u||ch>0x7Eu)continue;
        if(rt->elm_line_len+1u>=sizeof(rt->elm_line)){
            rt->elm_line_len=0u;
            (void)elm_usb_text(rt,"?");
            elm_usb_prompt(rt);
            continue;
        }
        rt->elm_line[rt->elm_line_len++]=(char)ch;
    }
#else
    (void)now_ms;
#endif
}

#if ARX_COMPILE_C1
static void interchip_c1(ArxRuntime *rt,uint8_t cmd) {
    switch(cmd){
        case ARX_IC_C1_BRAKE_NORMAL:rt->remote_brake_forced=false;break;
        case ARX_IC_C1_BRAKE_FORCE:rt->remote_brake_forced=true;break;
        case ARX_IC_C1_DYNO_ON:rt->remote_dyno_active=true;break;
        case ARX_IC_C1_DYNO_OFF:rt->remote_dyno_active=false;break;
        case ARX_IC_C1_USB_C2_ON:rt->remote_usb_c2_active=true;break;
        case ARX_IC_C1_USB_C2_OFF:rt->remote_usb_c2_active=false;break;
        case ARX_IC_C1_USB_BH_ON:rt->remote_usb_bh_active=true;break;
        case ARX_IC_C1_USB_BH_OFF:rt->remote_usb_bh_active=false;break;
        case ARX_IC_C1_LANE_SINGLE:{
            const uint8_t p[2]={ARX_IC_TO_C2,ARX_IC_C2_ESC_TC_TOGGLE};
            (void)ic_push(rt,p,sizeof(p));
            break;
        }
        case ARX_IC_C1_BH_RACE_SHOW:
            if(rt->config.esc_tc_customizer_enabled)
                rt->drive_style.inversion_active=true;
            break;
        case ARX_IC_C1_BH_RACE_HIDE:
            rt->drive_style.inversion_active=false;
            break;
        default:break;
    }
}
#endif

#if ARX_COMPILE_C2
static void interchip_c2(ArxRuntime *rt,uint8_t cmd,uint32_t now_ms) {
    ArxCanFrame out;
    switch(cmd){
        case ARX_IC_C2_DYNO_TOGGLE:
            if(rt->config.dyno_enabled&&arx_dyno_toggle(&rt->dyno,now_ms,&out))
                (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);
            break;
        case ARX_IC_C2_BRAKE_NORMAL:
            arx_brake_request_release(&rt->brake);
            break;
        case ARX_IC_C2_BRAKE_FORCE:{
            const bool stationary=(rt->vehicle.valid_mask&ARX_VS_SPEED) &&
                                  rt->vehicle.vehicle_speed_kmh==0.0f;
            (void)arx_brake_request_force(&rt->brake,stationary,rt->dyno.enabled);
            break;
        }
        case ARX_IC_C2_ESC_TC_TOGGLE:
            if(rt->drive_style.enabled&&rt->dyno.state==ARX_DYNO_IDLE)
                rt->drive_style.inversion_active=!rt->drive_style.inversion_active;
            {
                uint8_t p[2]={ARX_IC_TO_C1_BH,
                    (rt->drive_style.inversion_active&&rt->drive_style.show_race_mask)
                    ?ARX_IC_C1_BH_RACE_SHOW:ARX_IC_C1_BH_RACE_HIDE};
                (void)ic_push(rt,p,sizeof(p));
            }
            break;
        case ARX_IC_C2_RACE_MASK_DEFAULT:rt->drive_style.show_race_mask=false;break;
        case ARX_IC_C2_RACE_MASK_SHOW:rt->drive_style.show_race_mask=true;break;
        case ARX_IC_C2_HAS_TOGGLE:arx_acc_request_has_press(&rt->acc);break;
        case ARX_IC_C2_PDC_MUTE_OFF:rt->park_mute.enabled=false;break;
        case ARX_IC_C2_PDC_MUTE_ON:rt->park_mute.enabled=true;break;
        case ARX_IC_SHARED_ESC_TC_OFF:
            rt->drive_style.enabled=false;rt->drive_style.inversion_active=false;break;
        case ARX_IC_SHARED_ESC_TC_ON:rt->drive_style.enabled=true;break;
        case ARX_IC_SHARED_HAS_OFF:rt->acc.has_virtual_pad_enabled=false;break;
        case ARX_IC_SHARED_HAS_ON:rt->acc.has_virtual_pad_enabled=true;break;
        case ARX_IC_SHARED_SAVE_LOG:
            if(rt->ops.save_log) (void)rt->ops.save_log(rt->ops.user);
            break;
        case ARX_IC_SHARED_SNIFFER_OFF:
            arx_sniffer_stop(&rt->sniffer);
            rt->sniffer.enabled=false;
            (void)arx_usb_mode_request(&rt->usb_mode,ARX_USB_MODE_LEGACY_MSC,now_ms);
            break;
        case ARX_IC_SHARED_SNIFFER_ON:
            rt->sniffer.enabled=true;
            arx_sniffer_start(&rt->sniffer,now_ms);
            (void)arx_usb_mode_request(&rt->usb_mode,ARX_USB_MODE_SNIFFER,now_ms);
            break;
        default:break;
    }
}
#endif

#if ARX_COMPILE_BH
static void interchip_bh(
    ArxRuntime *rt,
    uint8_t dest,
    uint8_t cmd,
    uint8_t arg,
    uint32_t now_ms
) {
    /* Command values are reused by different destination classes on the deployed
       19-byte link, therefore destination is part of the command identity. */
    if(dest==ARX_IC_TO_C1_BH){
        if(cmd==ARX_IC_C1_BH_RACE_SHOW){
            if(rt->config.esc_tc_customizer_enabled)
                rt->drive_style.inversion_active=true;
        }else if(cmd==ARX_IC_C1_BH_RACE_HIDE){
            rt->drive_style.inversion_active=false;
        }
        return;
    }

    if(dest==ARX_IC_TO_C2_BH){
        switch(cmd){
            case ARX_IC_SHARED_PEDAL_MODE:
                rt->config.pedal_mode=arg;
                return;
            case ARX_IC_SHARED_HAS_OFF:
                rt->acc.has_virtual_pad_enabled=false;
                return;
            case ARX_IC_SHARED_HAS_ON:
                rt->acc.has_virtual_pad_enabled=true;
                return;
            case ARX_IC_SHARED_ESC_TC_OFF:
                rt->drive_style.enabled=false;
                rt->drive_style.inversion_active=false;
                return;
            case ARX_IC_SHARED_ESC_TC_ON:
                rt->drive_style.enabled=true;
                return;
            case ARX_IC_SHARED_SAVE_LOG:
                if(rt->ops.save_log)(void)rt->ops.save_log(rt->ops.user);
                return;
            case ARX_IC_SHARED_SNIFFER_OFF:
                arx_sniffer_stop(&rt->sniffer);
                rt->sniffer.enabled=false;
                (void)arx_usb_mode_request(&rt->usb_mode,ARX_USB_MODE_LEGACY_MSC,now_ms);
                return;
            case ARX_IC_SHARED_SNIFFER_ON:
                rt->sniffer.enabled=true;
                arx_sniffer_start(&rt->sniffer,now_ms);
                (void)arx_usb_mode_request(&rt->usb_mode,ARX_USB_MODE_SNIFFER,now_ms);
                return;
            default:
                return;
        }
    }

    if(dest==ARX_IC_BH_CHIME){
        rt->bh_chime_requested=true;
        return;
    }

    switch(cmd){
        case ARX_IC_BH_ODO_MASK_ON:
            rt->config.odometer_blink_mask_enabled=true;
            break;
        case ARX_IC_BH_ODO_MASK_OFF:
            rt->config.odometer_blink_mask_enabled=false;
            break;
        case ARX_IC_BH_MIRROR_OFF:
            rt->park_mirror.enabled=false;
            break;
        case ARX_IC_BH_MIRROR_ON:
            rt->park_mirror.enabled=true;
            break;
        case ARX_IC_BH_MIRROR_STORE:
            arx_park_mirror_request_capture_park(&rt->park_mirror);
            break;
        default:
            break;
    }
}
#endif

void arx_runtime_on_interchip(
    ArxRuntime *rt,
    const uint8_t raw[ARX_INTERCHIP_FRAME_SIZE],
    uint32_t now_ms
) {
    if(!rt||!raw)return;
    const uint8_t dest=raw[0];

    if(dest>=ARX_LINK_TO_C2&&dest<=ARX_LINK_TO_MASTER){
        ArxLinkFrame link;
        memcpy(link.raw,raw,ARX_LINK_FRAME_SIZE);
        if(!arx_link_validate(&link))return;

#if ARX_COMPILE_C1
        if(rt->role==ARX_RUNTIME_C1&&dest==ARX_LINK_TO_MASTER){
            if(!rt->elm_request_active||
               rt->elm_candidate_index>=rt->elm_candidate_count)return;
            const ArxElmBus candidate=rt->elm_candidates[rt->elm_candidate_index];
            if(candidate==ARX_ELM_BUS_C1)return;

            if(link.raw[1]==ARX_LINK_RSP){
                ArxCanFrame frame={0};
                frame.bus=elm_bus(candidate);
                frame.id=arx_link_can_id(&link);
                frame.extended_id=(link.raw[2]&ARX_LINK_FLAG_EXTID)!=0u;
                frame.dlc=arx_link_dlc(&link);
                frame.timestamp_ms=now_ms;
                memcpy(frame.data,arx_link_data(&link),frame.dlc);
                elm_handle_response(rt,&frame,now_ms);
            }else if(link.raw[1]==ARX_LINK_END&&
                     (link.raw[2]&ARX_LINK_FLAG_NODATA)!=0u){
                (void)elm_try_next_candidate(rt,now_ms);
            }
            return;
        }
#endif

#if ARX_COMPILE_C2 || ARX_COMPILE_BH
        if((rt->role==ARX_RUNTIME_C2&&dest==ARX_LINK_TO_C2)||
           (rt->role==ARX_RUNTIME_BH&&dest==ARX_LINK_TO_BH)){
            arx_interchip_note_master_request(&rt->interchip,now_ms);
            diag_slave_handle_link(rt,&link,now_ms);
            return;
        }
#endif
        return;
    }

    if(!dest_matches(rt->role,dest))return;

    arx_interchip_note_master_request(&rt->interchip,now_ms);
#if ARX_COMPILE_BH
    if(rt->role==ARX_RUNTIME_BH&&dest==ARX_IC_TO_BH_PARAM_TEXT){
        char text[ARX_DASHBOARD_TEXT_LEN+1u];
        memcpy(text,&raw[1],ARX_DASHBOARD_TEXT_LEN);
        text[ARX_DASHBOARD_TEXT_LEN]='\0';
        arx_dashboard_set_text(&rt->dashboard,text,1u);
        return;
    }
#endif

    const uint8_t cmd=raw[1];
    const uint8_t arg=raw[2];

    if(dest==ARX_IC_ALL_CLEAR_DTC){
        if(rt->config.clear_faults_enabled){
            ArxCanFrame out;
            uint8_t ecu=(rt->role==ARX_RUNTIME_C1)?0x10u:
                        (rt->role==ARX_RUNTIME_C2)?0x28u:0x40u;
            if(arx_faults_build_clear(ecu,&out)){
                out.bus=role_bus(rt->role);
                (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);
            }
        }
        return;
    }

#if ARX_COMPILE_C1
    if(rt->role==ARX_RUNTIME_C1){interchip_c1(rt,cmd);return;}
#endif
#if ARX_COMPILE_C2
    if(rt->role==ARX_RUNTIME_C2){interchip_c2(rt,cmd,now_ms);return;}
#endif
#if ARX_COMPILE_BH
    if(rt->role==ARX_RUNTIME_BH){interchip_bh(rt,dest,cmd,arg,now_ms);return;}
#endif
    (void)arg;
}

void arx_runtime_queue_config_sync(ArxRuntime *rt) {
#if ARX_COMPILE_C1
    if(!rt||rt->role!=ARX_RUNTIME_C1)return;

    uint8_t p[3];

    p[0]=ARX_IC_TO_C2_BH;
    p[1]=rt->config.esc_tc_customizer_enabled?ARX_IC_SHARED_ESC_TC_ON:ARX_IC_SHARED_ESC_TC_OFF;
    (void)ic_push(rt,p,2u);

    p[0]=ARX_IC_TO_C2_BH;p[1]=ARX_IC_SHARED_PEDAL_MODE;p[2]=rt->config.pedal_mode;
    (void)ic_push(rt,p,3u);

    p[0]=ARX_IC_TO_BH;
    p[1]=rt->config.odometer_blink_mask_enabled?ARX_IC_BH_ODO_MASK_ON:ARX_IC_BH_ODO_MASK_OFF;
    (void)ic_push(rt,p,2u);

    p[0]=ARX_IC_TO_C2;
    p[1]=rt->config.race_mask_enabled?ARX_IC_C2_RACE_MASK_SHOW:ARX_IC_C2_RACE_MASK_DEFAULT;
    (void)ic_push(rt,p,2u);

    p[0]=ARX_IC_TO_BH;
    p[1]=rt->config.park_mirror_enabled?ARX_IC_BH_MIRROR_ON:ARX_IC_BH_MIRROR_OFF;
    (void)ic_push(rt,p,2u);

    p[0]=ARX_IC_TO_C2_BH;
    p[1]=rt->config.has_virtual_pad_enabled?ARX_IC_SHARED_HAS_ON:ARX_IC_SHARED_HAS_OFF;
    (void)ic_push(rt,p,2u);

    p[0]=ARX_IC_TO_C2;
    p[1]=rt->config.front_park_mute_enabled?ARX_IC_C2_PDC_MUTE_ON:ARX_IC_C2_PDC_MUTE_OFF;
    (void)ic_push(rt,p,2u);

    p[0]=ARX_IC_TO_C2_BH;
    p[1]=rt->config.sniffer_enabled?ARX_IC_SHARED_SNIFFER_ON:ARX_IC_SHARED_SNIFFER_OFF;
    (void)ic_push(rt,p,2u);
#else
    (void)rt;
#endif
}

#if ARX_COMPILE_C1
static void periodic_c1(ArxRuntime *rt,uint32_t now_ms) {
    ArxCanFrame out;
    menu_engine_visibility(rt,now_ms);
    telemetry_poll_current_page(rt,now_ms);

    if(rt->template_4b1_valid&&arx_start_stop_should_toggle(&rt->start_stop,now_ms)){
        if(arx_start_stop_build_toggle(&rt->template_4b1,&out)){
            if(enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms))
                arx_start_stop_mark_toggled(&rt->start_stop);
        }
    }

    if(arx_drive_style_periodic_384_c1(
        &rt->drive_style,rt->vehicle.engine_rpm,now_ms,&out))
        (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

    if(arx_awd_tick(&rt->awd,now_ms,&out))
        (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);

    if(arx_qv_exhaust_tick(&rt->exhaust,now_ms,&out))
        (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);

    if(arx_immobilizer_next_frame(&rt->immobilizer,now_ms,&out))
        (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);

    arx_seatbelt_tick(&rt->seatbelt,now_ms);
    arx_faults_tick(&rt->faults,now_ms);

    if(rt->template_1ef_valid&&
       arx_windows_build_action(&rt->windows,&rt->template_1ef,now_ms,&out))
        (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);

    if(rt->ops.pedal_send){
        uint8_t packet[ARX_PEDAL_PACKET_SIZE];
        if(arx_pedal_kids_override_required(
            &rt->pedal,rt->config.diesel_profile,rt->vehicle.engine_rpm,
            rt->vehicle.vehicle_speed_kmh)){
            if(arx_pedal_build_zero_override(&rt->pedal,packet)&&
               rt->ops.pedal_send(packet,rt->ops.user))
                rt->pedal_packets++;
        }else if(arx_pedal_build_sync_packet(
            &rt->pedal,rt->vehicle.dna_mode,rt->engine_running,now_ms,packet)){
            if(rt->ops.pedal_send(packet,rt->ops.user))
                rt->pedal_packets++;
        }
    }

    if(rt->leds.enabled&&rt->ops.led_submit){
        if(arx_led_strip_should_shutdown(&rt->leds,now_ms)){
            ArxRgb off[ARX_LED_COUNT]={{0,0,0}};
            (void)rt->ops.led_submit(off,rt->ops.user);
        }else{
            ArxRgb rgb[ARX_LED_COUNT];
            if(arx_led_strip_render(&rt->leds,rgb,now_ms)==ARX_LED_COUNT)
                (void)rt->ops.led_submit(rgb,rt->ops.user);
        }
    }

    if(now_ms-rt->last_c2_status_request_ms>1010u){
        const uint8_t p[2]={ARX_IC_TO_C2,ARX_IC_C2_GET_STATUS};
        if(ic_push(rt,p,sizeof(p)))rt->last_c2_status_request_ms=now_ms;
    }
    if(now_ms-rt->last_bh_status_request_ms>1260u){
        const uint8_t p[1]={ARX_IC_BH_GET_STATUS};
        if(ic_push(rt,p,sizeof(p)))rt->last_bh_status_request_ms=now_ms;
    }
}
#endif

#if ARX_COMPILE_C2
static void periodic_c2(ArxRuntime *rt,uint32_t now_ms) {
    ArxCanFrame out;
    if(arx_dyno_tick(&rt->dyno,now_ms,&out))
        (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);
    if(arx_brake_tick(&rt->brake,now_ms,&out))
        (void)enqueue_can(rt,&out,ARX_PRIORITY_HIGH,now_ms);
    if(arx_park_mute_next_button_frame(&rt->park_mute,now_ms,&out))
        (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);
}
#endif

#if ARX_COMPILE_BH
static void periodic_bh(ArxRuntime *rt,uint32_t now_ms) {
    ArxCanFrame out;
    if(arx_park_mirror_build_command(&rt->park_mirror,now_ms,&out))
        (void)enqueue_can(rt,&out,ARX_PRIORITY_NORMAL,now_ms);
    if(arx_dashboard_next_telematic_frame(&rt->dashboard,now_ms,&out))
        (void)enqueue_can(rt,&out,ARX_PRIORITY_LOW,now_ms);
}
#endif

static bool runtime_usb_attach(ArxUsbMode mode,void *user) {
    ArxRuntime *rt=(ArxRuntime*)user;
    return rt&&rt->ops.usb_attach&&rt->ops.usb_attach(mode,rt->ops.user);
}

static bool runtime_usb_detach(void *user) {
    ArxRuntime *rt=(ArxRuntime*)user;
    return rt&&rt->ops.usb_detach&&rt->ops.usb_detach(rt->ops.user);
}

static void periodic_usb(ArxRuntime *rt,uint32_t now_ms) {
    ArxUsbOps usb_ops={
        .attach=runtime_usb_attach,
        .detach=runtime_usb_detach,
        .user=rt
    };
    (void)arx_usb_mode_process(&rt->usb_mode,now_ms,&usb_ops);

#if ARX_COMPILE_C1
    if(rt->role==ARX_RUNTIME_C1){
        elm_tick(rt,now_ms);
        if(rt->ops.usb_send&&
           rt->usb_mode.mode==ARX_USB_MODE_DIAGNOSTIC&&
           rt->usb_mode.state==ARX_USB_CONFIGURED&&
           rt->elm_usb_tx_off<rt->elm_usb_tx_len){
            const uint16_t remain=(uint16_t)(rt->elm_usb_tx_len-rt->elm_usb_tx_off);
            const size_t n=remain>64u?64u:remain;
            if(rt->ops.usb_send(&rt->elm_usb_tx[rt->elm_usb_tx_off],n,rt->ops.user)){
                rt->elm_usb_tx_off=(uint16_t)(rt->elm_usb_tx_off+n);
                rt->usb_bytes+=(uint32_t)n;
                if(rt->elm_usb_tx_off==rt->elm_usb_tx_len){
                    rt->elm_usb_tx_off=0u;
                    rt->elm_usb_tx_len=0u;
                }
            }
        }
    }
#endif

    if(!rt->ops.usb_send||!rt->sniffer.enabled||
       rt->usb_mode.mode!=ARX_USB_MODE_SNIFFER||
       rt->usb_mode.state!=ARX_USB_CONFIGURED)return;

    uint8_t chunk[ARX_SNIFFER_USB_CHUNK];
    size_t n=arx_sniffer_peek_chunk(
        &rt->sniffer,now_ms,chunk,sizeof(chunk)
    );
    if(n&&rt->ops.usb_send(chunk,n,rt->ops.user)){
        arx_sniffer_commit_chunk(&rt->sniffer,n,now_ms);
        rt->usb_bytes+=(uint32_t)n;
    }
}

void arx_runtime_tick(ArxRuntime *rt,uint32_t now_ms) {
    if(!rt)return;
#if ARX_COMPILE_C1
    if(rt->role==ARX_RUNTIME_C1)periodic_c1(rt,now_ms);
#endif
#if ARX_COMPILE_C2
    if(rt->role==ARX_RUNTIME_C2)periodic_c2(rt,now_ms);
#endif
#if ARX_COMPILE_BH
    if(rt->role==ARX_RUNTIME_BH)periodic_bh(rt,now_ms);
#endif
    periodic_usb(rt,now_ms);
}

size_t arx_runtime_drain_can(ArxRuntime *rt,uint32_t now_ms,size_t budget) {
    if(!rt||!rt->ops.can_send)return 0u;
    size_t sent=0u;
    while(sent<budget){
        ArxStatus s=arx_can_process_one(
            &rt->can_tx,now_ms,rt->ops.can_send,rt->ops.user
        );
        if(s==ARX_STATUS_EMPTY)break;
        if(s==ARX_STATUS_OK)sent++;
        else break;
    }
    return sent;
}

static bool interchip_diag_offset(
    const ArxInterchipQueue *q,uint8_t *offset
) {
    if(!q||!offset)return false;
    for(uint8_t i=0u;i<q->count;i++){
        const uint8_t index=(uint8_t)((q->head+i)%ARX_INTERCHIP_QUEUE_SIZE);
        const uint8_t dest=q->items[index].bytes[0];
        if(dest>=ARX_LINK_TO_C2&&dest<=ARX_LINK_TO_MASTER){
            *offset=i;
            return true;
        }
    }
    return false;
}

static void interchip_remove_offset(ArxInterchipQueue *q,uint8_t offset) {
    if(!q||offset>=q->count)return;
    for(uint8_t i=offset;i+1u<q->count;i++){
        const uint8_t dst=(uint8_t)((q->head+i)%ARX_INTERCHIP_QUEUE_SIZE);
        const uint8_t src=(uint8_t)((q->head+i+1u)%ARX_INTERCHIP_QUEUE_SIZE);
        q->items[dst]=q->items[src];
    }
    q->tail=(uint8_t)((q->tail+ARX_INTERCHIP_QUEUE_SIZE-1u)%ARX_INTERCHIP_QUEUE_SIZE);
    q->count--;
}

size_t arx_runtime_drain_interchip(ArxRuntime *rt,uint32_t now_ms,size_t budget) {
    if(!rt||!rt->ops.interchip_send)return 0u;
    size_t sent=0u;

    while(sent<budget){
        uint8_t diag_offset=0u;
        if(interchip_diag_offset(&rt->interchip.tx,&diag_offset)){
            if(!arx_interchip_ready(&rt->interchip,now_ms))break;
            const uint8_t index=(uint8_t)(
                (rt->interchip.tx.head+diag_offset)%ARX_INTERCHIP_QUEUE_SIZE
            );
            const ArxInterchipFrame frame=rt->interchip.tx.items[index];
            if(!rt->ops.interchip_send(frame.bytes,rt->ops.user))break;
            interchip_remove_offset(&rt->interchip.tx,diag_offset);
            rt->interchip.last_tx_ms=now_ms;
            rt->interchip_tx_frames++;
            sent++;
            continue;
        }

        if(!arx_interchip_tx_allowed(&rt->interchip,now_ms))break;
        const ArxInterchipFrame *frame=NULL;
        if(!arx_interchip_queue_peek(&rt->interchip.tx,&frame))break;
        if(!rt->ops.interchip_send(frame->bytes,rt->ops.user))break;

        arx_interchip_queue_commit(&rt->interchip.tx);
        rt->interchip.last_tx_ms=now_ms;
        rt->interchip_tx_frames++;
        sent++;

        /* Normal master traffic keeps the deployed 250 ms slot. */
        if(rt->role==ARX_RUNTIME_C1)break;
    }
    return sent;
}
