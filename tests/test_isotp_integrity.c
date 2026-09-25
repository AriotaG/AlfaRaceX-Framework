#include "arx/arx_isotp.h"
#include <assert.h>
#include <stdio.h>

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
    cf.bus=ARX_BUS_C1; cf.id=0x7E9;
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_NONE);
    cf.id=0x7E8; cf.extended_id=true;
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_NONE);
    cf.extended_id=false; cf.dlc=3;
    assert(arx_isotp_rx_feed(&rx,&cf)==ARX_ISOTP_RX_EVENT_ERROR);
    arx_isotp_rx_init(&rx); f.data[0]=0; f.dlc=8;
    assert(arx_isotp_rx_feed(&rx,&f)==ARX_ISOTP_RX_EVENT_ERROR);
    f.dlc=9; f.data[0]=1;
    assert(arx_isotp_rx_feed(&rx,&f)==ARX_ISOTP_RX_EVENT_ERROR);
    puts("ISO-TP malformed and interleaved traffic: OK");
    return 0;
}
