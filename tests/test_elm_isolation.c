#include "arx/features/arx_elm_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} } while(0)

int main(void) {
    ArxElm327 cfg; arx_elm327_init(&cfg);
    cfg.tx_header=0x7E0u; cfg.tx_extended=false;
    ArxElmTransaction t; ArxCanFrame out; ArxElmRxEvent event;
    const uint8_t payload[10]={0x22,0xF1,0x90};
    CHECK(arx_elm_transaction_start(&t,&cfg,ARX_BUS_C1,payload,10,1,0,&out));
    ArxCanFrame fc={.bus=ARX_BUS_C1,.id=0x7E9,.dlc=3,.data={0x30,0,0}};
    CHECK(!arx_elm_transaction_on_flow_control(&t,&fc,1));
    CHECK(t.tx.state==ARX_ISOTP_TX_WAIT_FC);
    fc.id=0x7E8; fc.extended_id=true;
    CHECK(!arx_elm_transaction_on_flow_control(&t,&fc,1));
    fc.extended_id=false; fc.bus=ARX_BUS_C2;
    CHECK(!arx_elm_transaction_on_flow_control(&t,&fc,1));
    fc.bus=ARX_BUS_C1;
    CHECK(arx_elm_transaction_on_flow_control(&t,&fc,1));
    CHECK(arx_elm_transaction_next_tx(&t,1,&out));
    ArxCanFrame response={.bus=ARX_BUS_C2,.id=0x7E8,.dlc=4,.data={3,0x62,0xF1,0x90}};
    CHECK(arx_elm_transaction_on_rx(&t,&cfg,&response,&event)==ARX_ELM_RX_NONE);
    response.bus=ARX_BUS_C1;response.id=0x7E9;
    CHECK(arx_elm_transaction_on_rx(&t,&cfg,&response,&event)==ARX_ELM_RX_NONE);
    response.id=0x7E8;response.extended_id=true;
    CHECK(arx_elm_transaction_on_rx(&t,&cfg,&response,&event)==ARX_ELM_RX_NONE);
    CHECK(t.active && t.received_responses==0);
    response.extended_id=false;
    CHECK(arx_elm_transaction_on_rx(&t,&cfg,&response,&event)==ARX_ELM_RX_PAYLOAD);
    CHECK(!t.active && event.length==3 && event.data[0]==0x62);
    cfg.auto_format=false;
    CHECK(arx_elm_transaction_start(&t,&cfg,ARX_BUS_C1,payload,3,1,0,&out));
    response.data[1]=0x7F;response.data[2]=0x22;response.data[3]=0x78;
    CHECK(arx_elm_transaction_on_rx(&t,&cfg,&response,&event)==ARX_ELM_RX_PENDING);
    CHECK(t.active && t.received_responses==0);
    response.data[1]=0x62;
    CHECK(arx_elm_transaction_on_rx(&t,&cfg,&response,&event)==ARX_ELM_RX_RAW_FRAME);
    CHECK(!t.active && t.received_responses==1);
    CHECK(!arx_elm_transaction_start(&t,&cfg,ARX_BUS_C1,payload,10,1,0,&out));
    CHECK(!t.active);
    puts("ELM source isolation, pending response and rejected start: PASS");
    return 0;
}
