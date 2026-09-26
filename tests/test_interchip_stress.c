#include "arx/arx_interchip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
int main(void) {
    ArxInterchip master,slave;
    arx_interchip_init(&master,ARX_IC_ROLE_C1);
    CHECK(!arx_interchip_tx_allowed(&master,1999));
    CHECK(arx_interchip_tx_allowed(&master,2000));
    master.last_tx_ms=UINT32_MAX-100u;
    CHECK(!arx_interchip_tx_allowed(&master,149u));
    CHECK(arx_interchip_tx_allowed(&master,150u));
    arx_interchip_init(&slave,ARX_IC_ROLE_BH);
    CHECK(!arx_interchip_tx_allowed(&slave,3000));
    arx_interchip_note_master_request(&slave,UINT32_MAX-99u);
    CHECK(arx_interchip_tx_allowed(&slave,99u));
    CHECK(!arx_interchip_tx_allowed(&slave,100u));
    CHECK(!arx_interchip_tx_allowed(&slave,UINT32_MAX-90u));
    arx_interchip_note_master_request(&slave,3000u);
    CHECK(arx_interchip_tx_allowed(&slave,3100u));
    arx_interchip_init(&slave,ARX_IC_ROLE_BH); /* Reset cannot inherit a response slot. */
    CHECK(!arx_interchip_tx_allowed(&slave,3100u));
    ArxInterchipQueue queue={0};
    for(unsigned cycle=0;cycle<10000;cycle++) {
        for(unsigned n=0;n<ARX_INTERCHIP_QUEUE_SIZE;n++) {
            ArxInterchipFrame frame; uint8_t data[]={ARX_IC_TO_C2,(uint8_t)n};
            arx_interchip_frame_build(&frame,data,sizeof(data));
            CHECK(arx_interchip_queue_push(&queue,&frame));
            CHECK(frame.bytes[18]==ARX_INTERCHIP_PAD);
        }
        ArxInterchipFrame extra={{0}};
        CHECK(!arx_interchip_queue_push(&queue,&extra));
        CHECK(queue.count==ARX_INTERCHIP_QUEUE_SIZE && queue.dropped==cycle+1);
        for(unsigned n=0;n<ARX_INTERCHIP_QUEUE_SIZE;n++) {
            const ArxInterchipFrame *frame,*again;
            CHECK(arx_interchip_queue_peek(&queue,&frame));
            CHECK(arx_interchip_queue_peek(&queue,&again)); /* Failed send retains head. */
            CHECK(frame==again && frame->bytes[1]==n);
            arx_interchip_queue_commit(&queue);
        }
        CHECK(queue.count==0);
    }
    puts("Interchip: rollover, reset, expired reply window, 100000 FIFO deliveries PASS");
    return 0;
}
