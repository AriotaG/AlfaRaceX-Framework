#include "arx/features/arx_drive_style.h"
#include "arx/arx_crc.h"
#include <string.h>

void arx_drive_style_init(ArxDriveStyleControl *f) {
    if (!f) return;
    memset(f,0,sizeof(*f));
    f->actual_mode=ARX_DNA_UNKNOWN;
}

void arx_drive_style_update_actual_mode(ArxDriveStyleControl *f,ArxDnaMode mode) {
    if(!f) return;
    if(f->actual_mode!=ARX_DNA_UNKNOWN && f->actual_mode!=mode)
        f->inversion_active=false;
    f->actual_mode=mode;
}

bool arx_drive_style_lane_event(
    ArxDriveStyleControl *f,bool pressed,bool dyno_busy,
    uint32_t now_ms,bool *double_tap
) {
    if(!f) return false;
    if(double_tap)*double_tap=false;

    if(pressed){
        if(f->lane_press_started_ms==0u){
            f->lane_press_started_ms=now_ms;
            f->lane_clicks++;
            if(f->lane_clicks==1u)f->first_lane_click_ms=now_ms;
        }
        if(now_ms-f->lane_press_started_ms>2000u){
            if(f->enabled&&!dyno_busy)
                f->inversion_active=!f->inversion_active;
            f->lane_press_started_ms=0u;
            f->lane_clicks=0u;
            return true;
        }
        return false;
    }

    f->lane_press_started_ms=0u;
    if(f->lane_clicks && now_ms-f->first_lane_click_ms>1000u)
        f->lane_clicks=0u;

    if(f->lane_clicks>=2u){
        f->lane_clicks=0u;
        if(double_tap)*double_tap=true;
        return true;
    }
    return false;
}

void arx_drive_style_observe_384_c1(ArxDriveStyleControl *f,const ArxCanFrame *src) {
    if(!f||!src||src->extended_id||src->bus!=ARX_BUS_C1||
       src->id!=0x384u||src->dlc!=8u) return;
    f->c1_384_template=*src;
    f->c1_384_valid=true;
}

bool arx_drive_style_transform_384_c1(const ArxDriveStyleControl *f,const ArxCanFrame *src,ArxCanFrame *out) {
    if(!f||!src||!out||!f->inversion_active||
       src->extended_id||src->bus!=ARX_BUS_C1||src->id!=0x384u||src->dlc!=8u) return false;

    *out=*src;
    if(f->actual_mode!=ARX_DNA_RACE)
        out->data[1]=(uint8_t)((out->data[1]&~0x7Cu)|0x30u);
    out->data[7]=arx_crc8_sae_j1850(out->data,7u);
    return true;
}

bool arx_drive_style_periodic_384_c1(
    ArxDriveStyleControl *f,uint16_t rpm,uint32_t now_ms,ArxCanFrame *out
) {
    if(!f||!out||!f->inversion_active||!f->c1_384_valid) return false;
    if(rpm==0u){
        f->inversion_active=false;
        return false;
    }
    if(f->last_periodic_384_ms && now_ms-f->last_periodic_384_ms<=50u) return false;

    *out=f->c1_384_template;
    out->timestamp_ms=now_ms;
    out->data[1]=(uint8_t)((out->data[1]&~0x7Cu)|0x30u);
    out->data[7]=arx_crc8_sae_j1850(out->data,7u);
    f->last_periodic_384_ms=now_ms;
    return true;
}

bool arx_drive_style_transform_384_c2(const ArxDriveStyleControl *f,const ArxCanFrame *src,ArxCanFrame *out) {
    if(!f||!src||!out||!f->inversion_active||
       src->extended_id||src->bus!=ARX_BUS_C2||src->id!=0x384u||src->dlc<2u) return false;

    *out=*src;
    const uint8_t v=(f->actual_mode==ARX_DNA_RACE)?0x08u:0x30u;
    out->data[1]=(uint8_t)((out->data[1]&~0x7Cu)|v);
    return true;
}

bool arx_drive_style_transform_46c(const ArxDriveStyleControl *f,const ArxCanFrame *src,ArxCanFrame *out) {
    if(!f||!src||!out||!f->inversion_active||
       f->actual_mode==ARX_DNA_RACE||src->extended_id||
       src->bus!=ARX_BUS_BH||src->id!=0x46Cu||src->dlc<8u) return false;

    *out=*src;
    out->data[7]=(uint8_t)((out->data[7]&~0x1Fu)|0x0Cu);
    return true;
}

bool arx_drive_style_transform_4af(const ArxDriveStyleControl *f,const ArxCanFrame *src,ArxCanFrame *out) {
    if(!f||!src||!out||!f->inversion_active||
       f->actual_mode==ARX_DNA_RACE||src->extended_id||
       (src->bus!=ARX_BUS_C1&&src->bus!=ARX_BUS_C2)||
       src->id!=0x4AFu||src->dlc<2u) return false;

    if((src->data[0]&0x01u)&&(src->data[1]&0x04u))return false;
    *out=*src;
    out->data[0]|=0x01u;
    out->data[1]|=0x04u;
    return true;
}

bool arx_drive_style_transform_5a8(const ArxDriveStyleControl *f,const ArxCanFrame *src,ArxCanFrame *out) {
    if(!f||!src||!out||!f->inversion_active||
       src->extended_id||src->bus!=ARX_BUS_C1||src->id!=0x5A8u||src->dlc!=8u) return false;

    if((src->data[4]&0x78u)==0x30u)return false;

    *out=*src;
    out->data[4]=(uint8_t)((out->data[4]&~0x78u)|0x30u);
    uint8_t counter=(uint8_t)((out->data[6]&0x0Fu)+1u);
    if(counter>0x0Fu)counter=0u;
    out->data[6]=(uint8_t)((out->data[6]&0xF0u)|counter);
    out->data[7]=arx_crc8_sae_j1850(out->data,7u);
    return true;
}

bool arx_drive_style_transform_25a(const ArxDriveStyleControl *f,const ArxCanFrame *src,ArxCanFrame *out) {
    if(!f||!src||!out||!f->inversion_active||
       f->actual_mode==ARX_DNA_RACE||src->extended_id||
       src->bus!=ARX_BUS_BH||src->id!=0x25Au||src->dlc<1u) return false;

    if((src->data[0]&0x7Cu)==0x04u)return false;
    *out=*src;
    out->data[0]=(uint8_t)((out->data[0]&~0x7Cu)|0x04u);
    return true;
}
