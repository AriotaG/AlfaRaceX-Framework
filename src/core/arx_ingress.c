#include "arx/arx_ingress.h"
bool arx_ingress_push(ArxIngressQueue *q,const ArxIngressEvent *e){
    if(!q||!e||(unsigned)e->kind>=ARX_INGRESS_KIND_COUNT||e->length>ARX_INGRESS_USB_SIZE)return false;
    if(e->kind!=ARX_INGRESS_USB&&e->length>ARX_INGRESS_INLINE_SIZE)return false;
    if(q->count==ARX_INGRESS_CAPACITY){q->rejected[e->kind]++;return false;}
    q->events[q->tail]=*e;
    q->tail=(uint8_t)((q->tail+1u)%ARX_INGRESS_CAPACITY);
    q->count++;
    return true;
}
bool arx_ingress_pop(ArxIngressQueue *q,ArxIngressEvent *e){
    if(!q||!e||q->count==0u)return false;
    *e=q->events[q->head];
    q->head=(uint8_t)((q->head+1u)%ARX_INGRESS_CAPACITY);
    q->count--;
    return true;
}
