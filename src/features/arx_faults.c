#include "arx/features/arx_faults.h"
#include <string.h>

static void parse_dtcs(ArxFaultManager *f) {
    f->dtc_count=0u;
    uint16_t off=3u;
    while(off+4u<=f->received && f->dtc_count<ARX_FAULT_MAX_DTC){
        ArxDtcRecord *d=&f->dtc[f->dtc_count++];
        d->bytes[0]=f->rx[off];
        d->bytes[1]=f->rx[off+1u];
        d->bytes[2]=f->rx[off+2u];
        d->status=f->rx[off+3u];
        off+=4u;
    }
}

void arx_faults_init(ArxFaultManager *f, uint32_t request_id, uint32_t response_id) {
    if(!f) return;
    memset(f,0,sizeof(*f));
    f->request_id=request_id;
    f->response_id=response_id;
    f->state=ARX_FAULTS_IDLE;
    f->timeout_ms=2000u;
}

bool arx_faults_start_read(ArxFaultManager *f, uint32_t now_ms, ArxCanFrame *out) {
    if(!f || !out || f->state!=ARX_FAULTS_IDLE) return false;
    memset(out,0,sizeof(*out));
    out->bus=ARX_BUS_C1; out->id=f->request_id; out->extended_id=true; out->dlc=3;
    out->data[0]=0x02; out->data[1]=0x10; out->data[2]=0x03; out->timestamp_ms=now_ms;
    f->state=ARX_FAULTS_WAIT_SESSION; f->state_since_ms=now_ms;
    f->dtc_count=0u; f->received=0u; f->expected=0u; f->next_sn=1u;
    return true;
}

bool arx_faults_on_response(ArxFaultManager *f, const ArxCanFrame *fr, uint32_t now_ms, ArxCanFrame *next) {
    if(!f || !fr || !next || !fr->extended_id || fr->id!=f->response_id) return false;

    if(fr->dlc>=3u && fr->data[1]==0x7Fu){
        f->state=ARX_FAULTS_ERROR;
        return false;
    }

    if(f->state==ARX_FAULTS_WAIT_SESSION &&
       fr->dlc>=3u && fr->data[1]==0x50u && fr->data[2]==0x03u){
        memset(next,0,sizeof(*next));
        next->bus=ARX_BUS_C1; next->id=f->request_id; next->extended_id=true; next->dlc=4;
        next->data[0]=0x03; next->data[1]=0x19; next->data[2]=0x02; next->data[3]=0xFF;
        next->timestamp_ms=now_ms;
        f->state=ARX_FAULTS_WAIT_READ; f->state_since_ms=now_ms;
        return true;
    }

    const uint8_t type=(uint8_t)(fr->data[0]>>4u);

    if(f->state==ARX_FAULTS_WAIT_READ && type==0u){
        uint8_t len=(uint8_t)(fr->data[0]&0x0Fu);
        if(len>=3u && fr->dlc>=4u && fr->data[1]==0x59u && fr->data[2]==0x02u){
            if(len>ARX_FAULT_RX_BUFFER) len=ARX_FAULT_RX_BUFFER;
            f->received=0u;
            for(uint8_t i=0u;i<len && (uint8_t)(i+1u)<fr->dlc;i++){
                f->rx[f->received++]=fr->data[i+1u];
            }
            parse_dtcs(f);
            f->state=ARX_FAULTS_COMPLETE;
        }
        return false;
    }

    if(f->state==ARX_FAULTS_WAIT_READ && type==1u){
        uint16_t total=(uint16_t)(((uint16_t)(fr->data[0]&0x0Fu)<<8u)|fr->data[1]);
        if(total>ARX_FAULT_RX_BUFFER) total=ARX_FAULT_RX_BUFFER;
        f->expected=total; f->received=0u; f->next_sn=1u;

        uint8_t n=(total<6u)?(uint8_t)total:6u;
        for(uint8_t i=0u;i<n;i++) f->rx[f->received++]=fr->data[2u+i];

        memset(next,0,sizeof(*next));
        next->bus=ARX_BUS_C1; next->id=f->request_id; next->extended_id=true; next->dlc=3;
        next->data[0]=0x30; next->data[1]=0x00; next->data[2]=0x00; next->timestamp_ms=now_ms;
        f->state=ARX_FAULTS_WAIT_CF; f->state_since_ms=now_ms;
        return true;
    }

    if(f->state==ARX_FAULTS_WAIT_CF && type==2u){
        const uint8_t sn=(uint8_t)(fr->data[0]&0x0Fu);
        if(sn!=f->next_sn){
            f->state=ARX_FAULTS_ERROR;
            return false;
        }
        f->next_sn=(uint8_t)((f->next_sn+1u)&0x0Fu);
        for(uint8_t i=1u;i<fr->dlc && f->received<f->expected;i++){
            f->rx[f->received++]=fr->data[i];
        }
        f->state_since_ms=now_ms;
        if(f->received>=f->expected){
            parse_dtcs(f);
            f->state=ARX_FAULTS_COMPLETE;
        }
    }
    return false;
}

bool arx_faults_build_clear(uint8_t ecu_address, ArxCanFrame *out) {
    if(!out) return false;
    memset(out,0,sizeof(*out));
    out->bus=ARX_BUS_C1;
    out->id=0x18DA00F1u | ((uint32_t)ecu_address<<8u);
    out->extended_id=true;
    out->dlc=5;
    out->data[0]=0x04; out->data[1]=0x14; out->data[2]=0xFF; out->data[3]=0xFF; out->data[4]=0xFF;
    return true;
}

void arx_faults_tick(ArxFaultManager *f, uint32_t now_ms) {
    if(!f) return;
    if((f->state==ARX_FAULTS_WAIT_SESSION ||
        f->state==ARX_FAULTS_WAIT_READ ||
        f->state==ARX_FAULTS_WAIT_CF) &&
       now_ms-f->state_since_ms > f->timeout_ms){
        f->state=ARX_FAULTS_ERROR;
    }
}
