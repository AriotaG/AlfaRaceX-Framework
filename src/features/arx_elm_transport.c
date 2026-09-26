#include "arx/features/arx_elm_transport.h"
#include <string.h>

void arx_elm_router_init(ArxElmRouter *router) {
    if(router) memset(router,0,sizeof(*router));
}

uint16_t arx_elm_target_address(const ArxElm327 *cfg) {
    if(!cfg) return 0u;
    if(cfg->tx_extended) return (uint16_t)((cfg->tx_header>>8u)&0xFFu);
    return (uint16_t)(cfg->tx_header&0x7FFu);
}

static bool lookup(const ArxElmRouter *r,uint16_t address,ArxElmBus *bus) {
    if(!r) return false;
    for(uint8_t i=0;i<r->count;i++){
        if(r->entries[i].address==address){
            if(bus) *bus=r->entries[i].bus;
            return true;
        }
    }
    return false;
}

uint8_t arx_elm_router_candidates(
    const ArxElmRouter *router,
    const ArxElm327 *cfg,
    ArxElmBus out[3]
) {
    if(!cfg||!out) return 0u;
    ArxElmBus known;
    if(lookup(router,arx_elm_target_address(cfg),&known)){
        out[0]=known;
        return 1u;
    }

    if(cfg->bitrate_divisor>1u){
        out[0]=ARX_ELM_BUS_BH;
        out[1]=ARX_ELM_BUS_C2;
        out[2]=ARX_ELM_BUS_C1;
    }else{
        out[0]=ARX_ELM_BUS_C1;
        out[1]=ARX_ELM_BUS_C2;
        out[2]=ARX_ELM_BUS_BH;
    }
    return 3u;
}

bool arx_elm_response_filter(
    const ArxElm327 *cfg,
    uint32_t *value,
    uint32_t *mask,
    bool *extended_id
) {
    if(!cfg||!value||!mask||!extended_id) return false;

    if(cfg->filter_mask!=0u){
        *value=cfg->filter_value;
        *mask=cfg->filter_mask;
        *extended_id=(cfg->filter_mask>0x7FFu)||(cfg->filter_value>0x7FFu);
        return true;
    }

    if(cfg->tx_extended){
        /* ISO 15765 normal fixed addressing used by the Giorgio platform:
         * request 18 DA <target> F1 -> response 18 DA F1 <target>. */
        if((cfg->tx_header&0xFFFF00FFu)==0x18DA00F1u){
            const uint32_t target=(cfg->tx_header>>8u)&0xFFu;
            *value=0x18DAF100u|target;
            *mask=0x1FFFFFFFu;
            *extended_id=true;
            return true;
        }
        *value=0u;
        *mask=0u;
        *extended_id=true;
        return false;
    }

    if(cfg->tx_header==0x7DFu){
        *value=0x7E8u;
        *mask=0x7F8u;
        *extended_id=false;
        return true;
    }
    if(cfg->tx_header>=0x7E0u&&cfg->tx_header<=0x7E7u){
        *value=cfg->tx_header+8u;
        *mask=0x7FFu;
        *extended_id=false;
        return true;
    }

    *value=0u;
    *mask=0u;
    *extended_id=false;
    return false;
}

void arx_elm_router_remember(
    ArxElmRouter *router,
    const ArxElm327 *cfg,
    ArxElmBus bus
) {
    if(!router||!cfg) return;
    uint16_t address=arx_elm_target_address(cfg);

    for(uint8_t i=0;i<router->count;i++){
        if(router->entries[i].address==address){
            router->entries[i].bus=bus;
            return;
        }
    }

    if(router->count<ARX_ELM_ROUTE_CACHE_LEN){
        router->entries[router->count].address=address;
        router->entries[router->count].bus=bus;
        router->count++;
    }
}

void arx_elm_transaction_init(ArxElmTransaction *t) {
    if(!t) return;
    memset(t,0,sizeof(*t));
    arx_isotp_tx_init(&t->tx);
    arx_isotp_rx_init(&t->rx);
}

bool arx_elm_transaction_start(
    ArxElmTransaction *t,
    const ArxElm327 *cfg,
    ArxBus bus,
    const uint8_t *payload,
    uint16_t payload_length,
    uint8_t expected_responses,
    uint32_t now_ms,
    ArxCanFrame *first
) {
    if(!t) return false;
    arx_elm_transaction_init(t);
    if(!cfg||!payload||!payload_length||!first||
       payload_length>ARX_ISOTP_MAX_PAYLOAD||
       (!cfg->auto_format&&payload_length>8u)) return false;
    t->active=true;
    t->raw_mode=!cfg->auto_format;
    t->auto_flow_control=cfg->auto_flow_control;
    t->expected_responses=expected_responses;
    t->bus=bus;
    (void)arx_elm_response_filter(cfg,&t->response_value,&t->response_mask,&t->response_extended);
    /* C2/BH responses cross the 38.4 kbit/s inter-controller link. Limiting
     * ISO-TP bursts prevents a fast ECU from overrunning that transport. */
    t->rx_block_size=(bus==ARX_BUS_C1)?0u:4u;
    t->rx_st_min_ms=(bus==ARX_BUS_C1)?0u:8u;
    arx_isotp_rx_set_block_size(&t->rx,t->rx_block_size);

    if(t->raw_mode){
        if(payload_length>8u) return false;
        memset(first,0,sizeof(*first));
        first->bus=bus;
        first->id=cfg->tx_header;
        first->extended_id=cfg->tx_extended;
        first->dlc=(uint8_t)payload_length;
        first->timestamp_ms=now_ms;
        memcpy(first->data,payload,payload_length);
        return true;
    }

    return arx_isotp_tx_start(
        &t->tx,bus,cfg->tx_header,cfg->tx_extended,
        payload,payload_length,cfg->variable_dlc,now_ms,first
    );
}

static bool transaction_accepts(const ArxElmTransaction *t,const ArxCanFrame *frame) {
    return t&&frame&&t->active&&frame->bus==t->bus&&
        frame->extended_id==t->response_extended&&
        (frame->id&t->response_mask)==(t->response_value&t->response_mask);
}

bool arx_elm_transaction_on_flow_control(
    ArxElmTransaction *t,
    const ArxCanFrame *frame,
    uint32_t now_ms
) {
    return transaction_accepts(t,frame)&&!t->raw_mode&&
        arx_isotp_tx_on_flow_control(&t->tx,frame,now_ms);
}

bool arx_elm_transaction_next_tx(
    ArxElmTransaction *t,
    uint32_t now_ms,
    ArxCanFrame *out
) {
    return t && t->active && arx_isotp_tx_next(&t->tx,now_ms,out);
}

static void event_clear(ArxElmRxEvent *e) {
    if(e) memset(e,0,sizeof(*e));
}

ArxElmRxEventType arx_elm_transaction_on_rx(
    ArxElmTransaction *t,
    const ArxElm327 *cfg,
    const ArxCanFrame *frame,
    ArxElmRxEvent *e
) {
    if(!t||!cfg||!frame||!e||!t->active) return ARX_ELM_RX_ERROR;
    event_clear(e);

    if(!transaction_accepts(t,frame)) return ARX_ELM_RX_NONE;
    if(frame->dlc==0u||frame->dlc>8u){
        t->active=false;
        e->type=ARX_ELM_RX_ERROR;
        return e->type;
    }

    e->can_id=frame->id;
    e->extended_id=frame->extended_id;

    if(t->raw_mode){
        e->type=ARX_ELM_RX_RAW_FRAME;
        e->length=frame->dlc>8u?8u:frame->dlc;
        memcpy(e->data,frame->data,e->length);

        if(frame->dlc>=4u &&
           (frame->data[0]&0xF0u)==0x00u &&
           frame->data[1]==0x7Fu &&
           frame->data[3]==0x78u){
            t->saw_response_pending=true;
            e->type=ARX_ELM_RX_PENDING;
            return e->type;
        }

        t->received_responses++;

        if(t->expected_responses &&
           t->received_responses>=t->expected_responses){
            t->active=false;
        }else if(t->saw_response_pending && e->type!=ARX_ELM_RX_PENDING){
            t->active=false;
        }
        return e->type;
    }

    const ArxIsoTpRxEvent r=arx_isotp_rx_feed(&t->rx,frame);
    if(r==ARX_ISOTP_RX_EVENT_ERROR){
        e->type=ARX_ELM_RX_ERROR;
        t->active=false;
        return e->type;
    }
    if(r==ARX_ISOTP_RX_EVENT_NEED_FLOW_CONTROL){
        e->type=ARX_ELM_RX_NEED_FLOW_CONTROL;
        return e->type;
    }
    if(r!=ARX_ISOTP_RX_EVENT_COMPLETE) return ARX_ELM_RX_NONE;

    e->length=t->rx.received;
    memcpy(e->data,t->rx.payload,e->length);

    if(e->length>=3u &&
       e->data[0]==0x7Fu &&
       e->data[2]==0x78u){
        t->saw_response_pending=true;
        e->type=ARX_ELM_RX_PENDING;
        arx_isotp_rx_init(&t->rx);
        arx_isotp_rx_set_block_size(&t->rx,t->rx_block_size);
        return e->type;
    }

    e->type=ARX_ELM_RX_PAYLOAD;
    t->received_responses++;
    t->active=false;
    return e->type;
}

bool arx_elm_transaction_build_flow_control(
    const ArxElmTransaction *t,
    const ArxElm327 *cfg,
    uint32_t now_ms,
    ArxCanFrame *out
) {
    if(!t||!cfg||!out||!cfg->auto_flow_control) return false;

    uint32_t id=cfg->tx_header;
    bool ext=cfg->tx_extended;
    const uint8_t *custom=NULL;
    uint8_t len=0u;

    uint8_t remote_default[3]={0x30u,t->rx_block_size,t->rx_st_min_ms};
    if(cfg->fc_mode && cfg->fc_length){
        custom=cfg->fc_data;
        len=cfg->fc_length;
        if(cfg->fc_header){
            id=cfg->fc_header;
            ext=cfg->fc_extended;
        }
    }else if(t->rx_block_size!=0u){
        custom=remote_default;
        len=3u;
    }

    return arx_isotp_build_flow_control(
        t->bus,id,ext,custom,len,now_ms,out
    );
}
