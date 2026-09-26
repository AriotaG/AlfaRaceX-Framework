#include "arx/arx_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static ArxCanFrame last;
static ArxStatus send_frame(const ArxCanFrame *f,void *user){(void)user;last=*f;return ARX_STATUS_OK;}
int main(void){
    ArxRuntime rt;ArxRuntimeOps ops={.can_send=send_frame};
    arx_runtime_init(&rt,ARX_RUNTIME_C1,&ops);
    ArxRuntimeConfig cfg;arx_config_defaults(&cfg);cfg.elm327_enabled=true;
    arx_runtime_apply_config(&rt,&cfg,10);
    rt.elm.tx_header=0x7E0u;
    const uint8_t request[]="220102030405060708090A0B0C0D0E0F10111213\r";
    arx_runtime_usb_rx(&rt,request,sizeof(request)-1,100);
    CHECK(rt.elm_request_active);
    CHECK(arx_runtime_drain_can(&rt,100,1)==1);
    CHECK(last.data[0]==0x10 && last.data[1]==20);
    ArxCanFrame filler={.bus=ARX_BUS_C1,.id=0x321,.dlc=1};
    for(unsigned i=0;i<ARX_CAN_QUEUE_CAPACITY;i++)
        CHECK(arx_can_enqueue(&rt.can_tx,&filler,ARX_PRIORITY_HIGH,0,500)==ARX_STATUS_OK);
    ArxCanFrame fc={.bus=ARX_BUS_C1,.id=0x7E8,.dlc=3,.data={0x30,0,0}};
    arx_runtime_on_can(&rt,&fc,101);
    CHECK(rt.elm_transaction.tx.state==ARX_ISOTP_TX_SEND_CF);
    rt.menu.visible=true;rt.menu.level=ARX_MENU_LEVEL_SUB;
    rt.menu.main_page=1;rt.menu.param_page=5;
    arx_runtime_tick(&rt,102);
    CHECK(rt.telemetry_last_poll_ms==0); /* Automatic UDS must not overlap the ELM exchange. */
    CHECK(rt.elm_transaction.tx.offset==6 && rt.elm_transaction.tx.sequence==1);
    CHECK(arx_runtime_drain_can(&rt,102,ARX_CAN_QUEUE_CAPACITY)==ARX_CAN_QUEUE_CAPACITY);
    arx_runtime_tick(&rt,103);
    CHECK(rt.elm_transaction.tx.offset==13 && rt.elm_transaction.tx.sequence==2);
    CHECK(arx_runtime_drain_can(&rt,103,1)==1);
    CHECK(last.id==0x7E0 && last.data[0]==0x21 && last.data[1]==6);
    const uint8_t mutate[]="ATSH7E1\r";
    arx_runtime_usb_rx(&rt,mutate,sizeof(mutate)-1,103);
    CHECK(rt.elm.tx_header==0x7E0u && rt.elm_request_active);
    arx_runtime_tick(&rt,104);
    CHECK(arx_runtime_drain_can(&rt,104,1)==1);
    CHECK(last.id==0x7E0 && last.data[0]==0x22 && last.data[1]==13);
    CHECK(rt.elm_transaction.tx.state==ARX_ISOTP_TX_COMPLETE);
    puts("ELM full TX queue retains ISO-TP offset; active transaction rejects config mutation: PASS");
    return 0;
}
