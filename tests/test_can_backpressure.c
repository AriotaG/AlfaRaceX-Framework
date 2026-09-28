#include "arx/arx_can.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static ArxStatus result=ARX_STATUS_FULL;
static unsigned sent;
static ArxStatus send_frame(const ArxCanFrame *frame,void *user){
    (void)user;
    if(result==ARX_STATUS_OK){CHECK(frame->data[0]==sent);sent++;}
    return result;
}
int main(void){
    ArxCanTransport tx;arx_can_init(&tx);
    ArxCanFrame frame={.bus=ARX_BUS_C1,.id=0x123,.dlc=1};
    for(unsigned i=0;i<ARX_CAN_QUEUE_CAPACITY;i++){
        frame.data[0]=(uint8_t)i;
        CHECK(arx_can_enqueue(&tx,&frame,ARX_PRIORITY_LOW,2,100)==ARX_STATUS_OK);
    }
    CHECK(arx_can_enqueue(&tx,&frame,ARX_PRIORITY_LOW,2,100)==ARX_STATUS_FULL);
    for(unsigned i=0;i<1000;i++)CHECK(arx_can_process_one(&tx,UINT32_MAX-10,send_frame,NULL)==ARX_STATUS_FULL);
    CHECK(tx.tx_retry_events==0 && tx.tx_dropped==1 && tx.tx_expired==0);
    result=ARX_STATUS_OK;
    for(unsigned i=0;i<ARX_CAN_QUEUE_CAPACITY;i++)CHECK(arx_can_process_one(&tx,100,send_frame,NULL)==ARX_STATUS_OK);
    CHECK(sent==ARX_CAN_QUEUE_CAPACITY);
    CHECK(arx_can_process_one(&tx,100,send_frame,NULL)==ARX_STATUS_EMPTY);
    CHECK(arx_can_enqueue(&tx,&frame,ARX_PRIORITY_LOW,0,UINT32_MAX-10)==ARX_STATUS_OK);
    CHECK(arx_can_process_one(&tx,10,send_frame,NULL)==ARX_STATUS_EXPIRED);
    CHECK(tx.tx_expired==1);
    puts("CAN saturation, retained retries, FIFO and tick rollover: PASS");
    return 0;
}
