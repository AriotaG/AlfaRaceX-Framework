#include "arx/arx_runtime.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned sent;
static uint8_t packet[ARX_PEDAL_PACKET_SIZE];
static bool pedal_send(const uint8_t *p,void *u){(void)u;memcpy(packet,p,sizeof(packet));sent++;return true;}
static void command(ArxRuntime *r,const char *s,uint32_t now){
    r->elm_usb_tx_len=0;r->elm_usb_tx_off=0;
    arx_runtime_usb_rx(r,(const uint8_t*)s,strlen(s),now);
    r->elm_usb_tx[r->elm_usb_tx_len]=0;
}
int main(void){
    ArxRuntime r;ArxRuntimeOps ops={.pedal_send=pedal_send};
    arx_runtime_init(&r,ARX_RUNTIME_C1,&ops);
    r.usb_mode.mode=ARX_USB_MODE_DIAGNOSTIC;
    command(&r,"AT@PRX?\r",1);
    assert(strstr((char*)r.elm_usb_tx,"ARXPRX1:C1:000A01000000"));
    command(&r,"AT@PRX=05,14\r",2);
    assert(strstr((char*)r.elm_usb_tx,"ERR:VEHICLE_STATE") && r.pedal.mode==ARX_PEDAL_DISABLED);
    ArxCanFrame stopped={.bus=ARX_BUS_C1,.id=0x101,.dlc=3};
    arx_runtime_on_can(&r,&stopped,10);
    command(&r,"AT@PRX=09,14\r",11);
    assert(strstr((char*)r.elm_usb_tx,"ERR:RANGE"));
    command(&r,"AT@PRX=05,14junk\r",12);
    assert(strstr((char*)r.elm_usb_tx,"ERR:FORMAT"));
    command(&r,"AT@PRX=05,14\r",13);
    assert(r.config.pedal_mode==5 && r.pedal.power==10 && sent==0);
    r.engine_running=true;r.vehicle.engine_rpm=800;
    arx_runtime_tick(&r,20);
    assert(sent==1 && packet[2]==0xDB && packet[3]==212);
    arx_runtime_on_pedal_reply(&r,0xEB,30);
    command(&r,"AT@PRX?\r",31);
    assert(strstr((char*)r.elm_usb_tx,"ARXPRX1:C1:051404040203"));
    command(&r,"AT@PRX=04,0A\r",1011);
    assert(strstr((char*)r.elm_usb_tx,"ERR:VEHICLE_STATE") && r.pedal.mode==ARX_PEDAL_DYNAMIC);
    r.elm_request_active=true;
    command(&r,"AT@PRX=04,0A\r",1012);
    assert(strstr((char*)r.elm_usb_tx,"BUS BUSY") && r.pedal.mode==ARX_PEDAL_DYNAMIC);
    arx_runtime_init(&r,ARX_RUNTIME_C2,&ops);r.usb_mode.mode=ARX_USB_MODE_DIAGNOSTIC;
    command(&r,"AT@PRX=05,14\r",1);
    assert(r.elm_usb_tx_len==0);
    puts("PedalRaceX C1 management to external packet/reply, gates and stale state: PASS");
    return 0;
}
