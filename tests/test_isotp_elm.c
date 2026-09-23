#include "arx/arx_isotp.h"
#include "arx/features/arx_elm327.h"
#include "arx/features/arx_elm_transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    uint8_t payload[20];
    for(unsigned i=0;i<20;i++)payload[i]=(uint8_t)i;

    ArxIsoTpTx tx;
    ArxCanFrame f;
    assert(arx_isotp_tx_start(&tx,ARX_BUS_C1,0x7E0,false,payload,20,false,0,&f));
    assert(f.data[0]==0x10 && f.data[1]==20 && tx.state==ARX_ISOTP_TX_WAIT_FC);

    ArxCanFrame fc={.bus=ARX_BUS_C1,.id=0x7E8,.dlc=8,.data={0x30,0,0}};
    assert(arx_isotp_tx_on_flow_control(&tx,&fc,1));
    assert(arx_isotp_tx_next(&tx,1,&f));
    assert((f.data[0]&0xF0u)==0x20u && (f.data[0]&0x0Fu)==1u);
    assert(arx_isotp_tx_next(&tx,1,&f));
    assert(tx.state==ARX_ISOTP_TX_COMPLETE);

    ArxIsoTpRx rx;
    arx_isotp_rx_init(&rx);
    ArxCanFrame ff={.bus=ARX_BUS_C1,.id=0x7E8,.dlc=8,.data={0x10,10,1,2,3,4,5,6}};
    assert(arx_isotp_rx_feed(&rx,&ff)==ARX_ISOTP_RX_EVENT_NEED_FLOW_CONTROL);
    ArxCanFrame cf={.bus=ARX_BUS_C1,.id=0x7E8,.dlc=8,.data={0x21,7,8,9,10,0,0,0}};
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_COMPLETE);
    assert(rx.received==10 && rx.payload[0]==1 && rx.payload[9]==10);

    ArxElm327 e;
    arx_elm327_init(&e); e.enabled=true;
    char reply[160];

    arx_elm327_command(&e,"ATI",reply,sizeof(reply));
    assert(strstr(reply,"ELM327 v1.4"));
    arx_elm327_command(&e,"AT@1",reply,sizeof(reply));
    assert(strstr(reply,"OBDII to RS232 Interpreter"));
    arx_elm327_command(&e,"ATDPN",reply,sizeof(reply));
    assert(strstr(reply,"A6"));
    arx_elm327_command(&e,"ATCRA18DAXXF1",reply,sizeof(reply));
    assert(e.filter_mask!=0u);
    arx_elm327_command(&e,"ATPP2DSV04",reply,sizeof(reply));
    assert(e.pp2d==4u);
    arx_elm327_command(&e,"ATSPB",reply,sizeof(reply));
    assert(e.protocol==ARX_ELM_PROTOCOL_USER1 && e.bitrate_divisor==4u);
    assert(arx_elm327_effective_bitrate_kbps(&e)==125u);
    arx_elm327_command(&e,"ATST00",reply,sizeof(reply));
    assert(e.timeout_ms==ARX_ELM_DEFAULT_TIMEOUT);

    /* Request parser hardening: malformed/truncated input must never be decoded. */
    ArxElmRequest raw_req;
    assert(arx_elm327_prepare_request(&e,"03 22 F1 90",&raw_req));
    assert(raw_req.length==4u && raw_req.data[0]==0x03u && raw_req.data[3]==0x90u);
    assert(!arx_elm327_prepare_request(&e,"",&raw_req));
    assert(!arx_elm327_prepare_request(&e,"0",&raw_req));
    assert(!arx_elm327_prepare_request(&e,"0G",&raw_req));
    /* With automatic formatting enabled, payloads longer than one CAN frame are
       valid ISO-TP requests. In raw mode they must fit in a single 8-byte frame. */
    assert(arx_elm327_prepare_request(&e,"001122334455667788",&raw_req));
    assert(raw_req.length==9u);
    arx_elm327_command(&e,"ATCAF0",reply,sizeof(reply));
    assert(!arx_elm327_prepare_request(&e,"001122334455667788",&raw_req));
    arx_elm327_command(&e,"ATCAF1",reply,sizeof(reply));
    char oversized_hex[96];
    memset(oversized_hex,'A',sizeof(oversized_hex)-1u);
    oversized_hex[sizeof(oversized_hex)-1u]='\0';
    assert(!arx_elm327_prepare_request(&e,oversized_hex,&raw_req));

    /* The wildcard-filter parser was tested above; clear it for the transport test. */
    arx_elm327_command(&e,"ATCRA",reply,sizeof(reply));
    assert(e.filter_mask==0u);

    ArxElmRouter router;
    arx_elm_router_init(&router);
    ArxElmBus order[3];
    assert(arx_elm_router_candidates(&router,&e,order)==3u);
    assert(order[0]==ARX_ELM_BUS_BH);
    arx_elm_router_remember(&router,&e,ARX_ELM_BUS_C2);
    assert(arx_elm_router_candidates(&router,&e,order)==1u && order[0]==ARX_ELM_BUS_C2);

    /* Automatic formatting transaction: request becomes ISO-TP SF. */
    ArxElmTransaction tr;
    const uint8_t req[]={0x22,0xF1,0x90};
    assert(arx_elm_transaction_start(&tr,&e,ARX_BUS_C1,req,3,0,100,&f));
    assert(f.data[0]==3u && f.data[1]==0x22u);

    ArxElmRxEvent ev;
    ArxCanFrame rsp={.bus=ARX_BUS_C1,.id=0x18DAF110,.extended_id=true,.dlc=8,
                     .data={3,0x62,0xF1,0x90,0,0,0,0}};
    assert(arx_elm_transaction_on_rx(&tr,&e,&rsp,&ev)==ARX_ELM_RX_PAYLOAD);
    assert(ev.length==3 && ev.data[0]==0x62);

    puts("isotp/elm tests: OK");
    return 0;
}
