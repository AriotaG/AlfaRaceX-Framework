#include "arx/arx_isotp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void round_trip(uint16_t length, uint8_t block_size) {
    uint8_t payload[ARX_ISOTP_MAX_PAYLOAD];
    for (uint16_t i=0; i<length; ++i) payload[i]=(uint8_t)i;
    ArxIsoTpTx tx;
    ArxIsoTpRx rx;
    ArxCanFrame frame;
    arx_isotp_rx_init(&rx);
    arx_isotp_rx_set_block_size(&rx,block_size);
    assert(arx_isotp_tx_start(&tx,ARX_BUS_C1,0x7E8,false,payload,length,true,0,&frame));
    for (;;) {
        ArxIsoTpRxEvent event=arx_isotp_rx_feed(&rx,&frame);
        if (event==ARX_ISOTP_RX_EVENT_COMPLETE) break;
        assert(event!=ARX_ISOTP_RX_EVENT_ERROR);
        if (event==ARX_ISOTP_RX_EVENT_NEED_FLOW_CONTROL) {
            ArxCanFrame fc={.bus=ARX_BUS_C1,.id=0x7E0,.dlc=3,.data={0x30,block_size,0}};
            assert(arx_isotp_tx_on_flow_control(&tx,&fc,0));
        }
        assert(arx_isotp_tx_next(&tx,0,&frame));
    }
    assert(rx.received==length);
    assert(memcmp(rx.payload,payload,length)==0);
    assert(tx.state==ARX_ISOTP_TX_COMPLETE);
}

int main(void) {
    ArxIsoTpRx rx;
    arx_isotp_rx_init(&rx);
    ArxCanFrame f = {.bus=ARX_BUS_C1,.id=0x7E8,.dlc=8,.data={0x11,0,1,2,3,4,5,6}};
    assert(arx_isotp_rx_feed(&rx,&f)==ARX_ISOTP_RX_EVENT_ERROR);
    arx_isotp_rx_init(&rx);
    f.data[0]=0x10; f.data[1]=10;
    assert(arx_isotp_rx_feed(&rx,&f)==ARX_ISOTP_RX_EVENT_NEED_FLOW_CONTROL);
    ArxCanFrame cf={.bus=ARX_BUS_C2,.id=0x7E8,.dlc=8,.data={0x21,7,8,9,10}};
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_NONE);
    assert(rx.received==6);
    cf.dlc=0;
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_NONE);
    assert(rx.state==ARX_ISOTP_RX_RECEIVING);
    cf.dlc=8;
    cf.bus=ARX_BUS_C1; cf.id=0x7E9;
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_NONE);
    cf.id=0x7E8; cf.extended_id=true;
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_NONE);
    cf.extended_id=false; cf.dlc=3;
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_ERROR);
    arx_isotp_rx_init(&rx); f.data[0]=0; f.dlc=8;
    assert(arx_isotp_rx_feed(&rx,&f)==ARX_ISOTP_RX_EVENT_ERROR);
    for (uint16_t length=1; length<=ARX_ISOTP_MAX_PAYLOAD; ++length) {
        round_trip(length,0);
        round_trip(length,1);
        round_trip(length,8);
    }
    arx_isotp_rx_init(&rx);
    f.dlc=8; f.data[0]=0x10; f.data[1]=10;
    assert(arx_isotp_rx_feed(&rx,&f)==ARX_ISOTP_RX_EVENT_NEED_FLOW_CONTROL);
    cf.dlc=5; cf.data[0]=0x22;
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_ERROR);
    arx_isotp_rx_init(&rx);
    assert(arx_isotp_rx_feed(&rx,&f)==ARX_ISOTP_RX_EVENT_NEED_FLOW_CONTROL);
    cf.data[0]=0x21;
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_COMPLETE);
    assert(rx.received==10 && rx.payload[9]==10);
    ArxIsoTpTx tx;
    uint8_t payload[8]={0};
    ArxCanFrame out;
    assert(arx_isotp_tx_start(&tx,ARX_BUS_C1,0x7E0,false,payload,8,false,0,&out));
    ArxCanFrame fc={.bus=ARX_BUS_C2,.id=0x7E8,.dlc=3,.data={0x30,0,0}};
    assert(!arx_isotp_tx_on_flow_control(&tx,&fc,0));
    assert(tx.state==ARX_ISOTP_TX_WAIT_FC);
    fc.bus=ARX_BUS_C1; fc.dlc=9;
    assert(!arx_isotp_tx_on_flow_control(&tx,&fc,0));
    fc.dlc=3;
    assert(arx_isotp_tx_on_flow_control(&tx,&fc,0));
    f.dlc=9; f.data[0]=1;
    assert(arx_isotp_rx_feed(&rx,&f)==ARX_ISOTP_RX_EVENT_ERROR);
    puts("ISO-TP malformed and interleaved traffic: OK");
    return 0;
}
