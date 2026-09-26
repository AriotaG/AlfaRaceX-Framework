#include "arx/arx_ingress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);} } while(0)
int main(void){
    ArxIngressQueue q={0};ArxIngressEvent e={0},out={0};
    CHECK(!arx_ingress_pop(&q,&out));
    for(unsigned round=0;round<10000;round++){
        for(unsigned i=0;i<ARX_INGRESS_CAPACITY;i++){
            e.kind=(ArxIngressKind)(i%ARX_INGRESS_KIND_COUNT);e.timestamp_ms=round*16+i;
            e.length=64;memset(e.data.bytes,(int)i,64);
            CHECK(arx_ingress_push(&q,&e));
            e.data.bytes[0]=255; /* Hardware buffer reuse must not alter queued data. */
        }
        CHECK(!arx_ingress_push(&q,&e));
        CHECK(q.count==ARX_INGRESS_CAPACITY && q.rejected[e.kind]==round+1);
        for(unsigned i=0;i<ARX_INGRESS_CAPACITY;i++){
            CHECK(arx_ingress_pop(&q,&out));
            CHECK(out.timestamp_ms==round*16+i && out.kind==(ArxIngressKind)(i%4));
            CHECK(out.length==64 && out.data.bytes[0]==i && out.data.bytes[63]==i);
        }
    }
    e.length=65;CHECK(!arx_ingress_push(&q,&e));
    CHECK(q.count==0);
    printf("Ingress: 160000 mixed events, stable copies, FIFO, saturation PASS (%zu bytes)\n",sizeof(q));
    return 0;
}
