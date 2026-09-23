#include "arx/features/arx_menu.h"
#include <string.h>

static const char *main_text[ARX_MAIN_MENU_COUNT]={
    "                  ",
    "Show Parameters   ",
    "Read Faults       ",
    "Clear Faults      ",
    "Immobilizer   ON  ",
    "Toggle DYNO       ",
    "Toggle ESC/TC     ",
    "Front Brake Normal",
    "4WD  Enabled      ",
    "Main Setup Menu   ",
    "Params Setup Menu ",
    "Enable HAS        ",
    "Toggle QV Valve   ",
    "Save Log to File  ",
    "Reset Statistics  ",
    "Max Hold OFF      "
};

static void put18(char out[ARX_MENU_TEXT_LEN+1u],const char *s){
    memset(out,' ',ARX_MENU_TEXT_LEN);
    if(s){size_t n=strlen(s);if(n>ARX_MENU_TEXT_LEN)n=ARX_MENU_TEXT_LEN;memcpy(out,s,n);}
    out[ARX_MENU_TEXT_LEN]='\0';
}

static bool skipped(uint8_t p,const ArxMenuCapabilities *c){
    if(!c)return false;
    if(p==2u&&!c->read_faults_available)return true;
    if(p==3u&&!c->clear_faults_available)return true;
    if(p==5u&&!c->dyno_available)return true;
    if(p==6u&&!c->esc_tc_available)return true;
    if(p==7u&&!c->brake_available)return true;
    if(p==8u&&!c->awd_available)return true;
    if(p==11u&&!c->has_available)return true;
    if(p==12u&&!c->exhaust_available)return true;
    return false;
}

void arx_menu_init(ArxMenuState *m){
    if(!m) return;
    memset(m,0,sizeof(*m));
    m->commands_enabled=true;
}
void arx_menu_set_visible(ArxMenuState *m,bool v){if(m)m->visible=v;}

uint8_t arx_menu_next_main(ArxMenuState *m,const ArxMenuCapabilities *c,uint8_t step){
    if(!m) return 0u;
    if(step==0u) step=1u;
    for(uint8_t k=0u;k<step;k++){
        do{m->main_page=(uint8_t)((m->main_page+1u)%ARX_MAIN_MENU_COUNT);}while(skipped(m->main_page,c));
    }
    return m->main_page;
}
uint8_t arx_menu_prev_main(ArxMenuState *m,const ArxMenuCapabilities *c,uint8_t step){
    if(!m) return 0u;
    if(step==0u) step=1u;
    for(uint8_t k=0u;k<step;k++){
        do{m->main_page=(m->main_page==0u)?ARX_MAIN_MENU_COUNT-1u:(uint8_t)(m->main_page-1u);}while(skipped(m->main_page,c));
    }
    return m->main_page;
}
void arx_menu_next_setup(ArxMenuState *m,uint8_t step){if(!m)return;m->setup_page=(uint8_t)((m->setup_page+step)%ARX_SETUP_MENU_COUNT);}
void arx_menu_prev_setup(ArxMenuState *m,uint8_t step){
    if(!m) return;
    step=(uint8_t)(step%ARX_SETUP_MENU_COUNT);
    m->setup_page=(uint8_t)((m->setup_page+ARX_SETUP_MENU_COUNT-step)%ARX_SETUP_MENU_COUNT);
}

bool arx_menu_main_text(uint8_t page,char out[ARX_MENU_TEXT_LEN+1u]){
    if(!out||page>=ARX_MAIN_MENU_COUNT) return false;
    put18(out,main_text[page]);
    return true;
}

static char mark(bool on){return on?'X':'O';}
static const char *pedal_name(uint8_t m){
    static const char *n[]={"Disabled","Auto","Bypass","All Weather","Natural","Dynamic","Race","Hybrid","Kids Limit"};
    return m<9u?n[m]:"Disabled";
}
static const char *tri_name(uint8_t v){static const char *n[]={"OFF","1 Click","2 Click"};return v<3u?n[v]:"OFF";}
static const char *acc_name(uint8_t v){static const char *n[]={"OFF","RES","Gently +"};return v<3u?n[v]:"OFF";}

static void write_uint_right(char out[ARX_MENU_TEXT_LEN+1u],size_t pos,size_t width,unsigned value){
    if(!out||pos>=ARX_MENU_TEXT_LEN||width==0u)return;
    if(pos+width>ARX_MENU_TEXT_LEN)width=ARX_MENU_TEXT_LEN-pos;
    for(size_t i=0;i<width;i++)out[pos+i]=' ';
    size_t at=pos+width;
    do{
        if(at==pos)break;
        out[--at]=(char)('0'+(value%10u));
        value/=10u;
    }while(value);
}

static void marked_text(char out[ARX_MENU_TEXT_LEN+1u],bool enabled,const char *label){
    put18(out,"");
    out[0]=mark(enabled);
    out[1]=' ';out[2]=' ';
    if(label){
        size_t n=strlen(label);
        if(n>ARX_MENU_TEXT_LEN-3u)n=ARX_MENU_TEXT_LEN-3u;
        memcpy(&out[3],label,n);
    }
}

static void prefix_choice(char out[ARX_MENU_TEXT_LEN+1u],const char *prefix,const char *choice){
    put18(out,prefix);
    size_t p=strlen(prefix);
    if(p>=ARX_MENU_TEXT_LEN||!choice)return;
    size_t n=strlen(choice);
    if(n>ARX_MENU_TEXT_LEN-p)n=ARX_MENU_TEXT_LEN-p;
    memcpy(&out[p],choice,n);
}

bool arx_menu_setup_text(uint8_t p,const ArxRuntimeConfig *c,char out[ARX_MENU_TEXT_LEN+1u]){
    if(!c||!out||p>=ARX_SETUP_MENU_COUNT)return false;
    switch(p){
        case 0: put18(out,"SAVE&EXIT");break;
        case 1: marked_text(out,c->smart_start_stop_enabled,"Start&Stop");break;
        case 2:
            put18(out,"LaunchTorque    Nm");
            write_uint_right(out,13u,3u,c->launch_torque_threshold_nm);
            break;
        case 3: marked_text(out,c->led_strip_enabled,"Led Controller");break;
        case 4: marked_text(out,c->shift_indicator_enabled,"Shift Indicator");break;
        case 5:
            put18(out,"Shift RPM");
            write_uint_right(out,10u,4u,c->shift_threshold_rpm);
            break;
        case 6: marked_text(out,c->ipc_my23,"My23 IPC");break;
        case 7: marked_text(out,c->regeneration_alert_enabled,"Regen. Alert");break;
        case 8: marked_text(out,c->seatbelt_alarm_enabled,"Seatbelt Alarm");break;
        case 9: marked_text(out,c->route_messages_enabled,"Route Messages");break;
        case 10:marked_text(out,c->esc_tc_customizer_enabled,"ESC/TC Custom.");break;
        case 11:marked_text(out,c->dyno_enabled,"Dyno");break;
        case 12:marked_text(out,c->acc_virtual_pad_enabled,"ACC Virtual Pad");break;
        case 13:marked_text(out,c->front_brake_override_enabled,"Brakes Override");break;
        case 14:marked_text(out,c->awd_control_enabled,"4WD Disabler");break;
        case 15:marked_text(out,c->clear_faults_enabled,"Clear Faults");break;
        case 16:marked_text(out,c->read_faults_enabled,"Read Faults");break;
        case 17:marked_text(out,false,"Remote Start");break;
        case 18:marked_text(out,c->diesel_profile,"Diesel   Params");break;
        case 19:marked_text(out,c->odometer_blink_mask_enabled,"Odometer Blink");break;
        case 20:prefix_choice(out,"Pedal ",pedal_name(c->pedal_mode));break;
        case 21:{
            put18(out,"Pedal Power:");
            int v=c->pedal_power;
            unsigned mag=(unsigned)(v<0?-v:v);
            out[13]=' ';
            if(mag>=10u){out[13]=(v<0)?'-':'+';out[14]=(char)('0'+(mag/10u)%10u);out[15]=(char)('0'+mag%10u);}
            else{out[14]=(v<0)?'-':'+';out[15]=(char)('0'+mag);}
            break;
        }
        case 22:marked_text(out,c->park_mirror_enabled,"Park Mirror");break;
        case 23:prefix_choice(out,"ACC Start ",acc_name(c->acc_autostart_mode));break;
        case 24:prefix_choice(out,"Close Win ",tri_name(c->close_windows_mode));break;
        case 25:prefix_choice(out,"Open Win ",tri_name(c->open_windows_mode));break;
        case 26:marked_text(out,c->has_virtual_pad_enabled,"HAS Virtual Pad");break;
        case 27:marked_text(out,c->exhaust_flap_enabled,"QV Exhaust Flap");break;
        case 28:marked_text(out,c->front_park_mute_enabled,"Front Park Mute");break;
        case 29:marked_text(out,c->sniffer_enabled,"SNIFFER");break;
        case 30:marked_text(out,c->elm327_enabled,"ELM327");break;
        default:return false;
    }
    return true;
}

ArxMenuEffect arx_menu_activate_setup(ArxMenuState *m,ArxRuntimeConfig *c){
    if(!m||!c||m->setup_page>=ARX_SETUP_MENU_COUNT)return ARX_MENU_EFFECT_NONE;
    ArxMenuEffect e=ARX_MENU_EFFECT_NONE;
    switch(m->setup_page){
        case 0:
            e=(ArxMenuEffect)(ARX_MENU_EFFECT_SAVE_CONFIG|ARX_MENU_EFFECT_SYNC_CONFIG|
                              (m->usb_change_pending?ARX_MENU_EFFECT_USB_MODE_CHANGED:0u));
            m->usb_change_pending=false;
            m->level=ARX_MENU_LEVEL_MAIN;
            break;
        case 1:c->smart_start_stop_enabled=!c->smart_start_stop_enabled;break;
        case 2:c->launch_torque_threshold_nm=(uint16_t)(c->launch_torque_threshold_nm+25u);if(c->launch_torque_threshold_nm>600u)c->launch_torque_threshold_nm=25u;break;
        case 3:c->led_strip_enabled=!c->led_strip_enabled;break;
        case 4:c->shift_indicator_enabled=!c->shift_indicator_enabled;break;
        case 5:c->shift_threshold_rpm=(uint16_t)(c->shift_threshold_rpm+250u);if(c->shift_threshold_rpm>6000u)c->shift_threshold_rpm=1500u;break;
        case 6:c->ipc_my23=!c->ipc_my23;break;
        case 7:c->regeneration_alert_enabled=!c->regeneration_alert_enabled;break;
        case 8:c->seatbelt_alarm_enabled=!c->seatbelt_alarm_enabled;break;
        case 9:c->route_messages_enabled=!c->route_messages_enabled;break;
        case 10:c->esc_tc_customizer_enabled=!c->esc_tc_customizer_enabled;e=ARX_MENU_EFFECT_SYNC_CONFIG;break;
        case 11:c->dyno_enabled=!c->dyno_enabled;break;
        case 12:c->acc_virtual_pad_enabled=!c->acc_virtual_pad_enabled;break;
        case 13:c->front_brake_override_enabled=!c->front_brake_override_enabled;break;
        case 14:c->awd_control_enabled=!c->awd_control_enabled;break;
        case 15:c->clear_faults_enabled=!c->clear_faults_enabled;break;
        case 16:c->read_faults_enabled=!c->read_faults_enabled;break;
        case 17:break; /* reference feature is not operational; keep display-only */
        case 18:c->diesel_profile=!c->diesel_profile;break;
        case 19:c->odometer_blink_mask_enabled=!c->odometer_blink_mask_enabled;e=ARX_MENU_EFFECT_SYNC_CONFIG;break;
        case 20:c->pedal_mode=(uint8_t)((c->pedal_mode+1u)%9u);e=(ArxMenuEffect)(ARX_MENU_EFFECT_PEDAL_RESYNC|ARX_MENU_EFFECT_SYNC_CONFIG);break;
        case 21:c->pedal_power=(int8_t)(c->pedal_power+2);if(c->pedal_power>10)c->pedal_power=-10;e=ARX_MENU_EFFECT_PEDAL_RESYNC;break;
        case 22:c->park_mirror_enabled=!c->park_mirror_enabled;e=(ArxMenuEffect)(ARX_MENU_EFFECT_SYNC_CONFIG|(c->park_mirror_enabled?ARX_MENU_EFFECT_CAPTURE_PARK_MIRROR:0));break;
        case 23:c->acc_autostart_mode=(uint8_t)((c->acc_autostart_mode+1u)%3u);break;
        case 24:c->close_windows_mode=(uint8_t)((c->close_windows_mode+1u)%3u);break;
        case 25:c->open_windows_mode=(uint8_t)((c->open_windows_mode+1u)%3u);break;
        case 26:c->has_virtual_pad_enabled=!c->has_virtual_pad_enabled;e=ARX_MENU_EFFECT_SYNC_CONFIG;break;
        case 27:c->exhaust_flap_enabled=!c->exhaust_flap_enabled;break;
        case 28:c->front_park_mute_enabled=!c->front_park_mute_enabled;e=ARX_MENU_EFFECT_SYNC_CONFIG;break;
        case 29:
            c->sniffer_enabled=!c->sniffer_enabled;
            if(c->sniffer_enabled)c->elm327_enabled=false;
            m->usb_change_pending=true;
            break;
        case 30:
            c->elm327_enabled=!c->elm327_enabled;
            if(c->elm327_enabled)c->sniffer_enabled=false;
            m->usb_change_pending=true;
            break;
        default:break;
    }
    (void)arx_config_sanitize(c);
    return e;
}
