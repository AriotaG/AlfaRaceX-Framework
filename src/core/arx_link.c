#include "arx/arx_link.h"
#include <string.h>

uint8_t arx_link_checksum(const uint8_t raw[ARX_LINK_FRAME_SIZE]) {
    if (!raw) return 0u;
    uint8_t x=0u;
    for (uint8_t i=0u;i<17u;i++) x=(uint8_t)(x^raw[i]);
    return x;
}

bool arx_link_validate(const ArxLinkFrame *f) {
    if (!f) return false;
    const uint8_t d=f->raw[0];
    if (d!=ARX_LINK_TO_C2 && d!=ARX_LINK_TO_BH && d!=ARX_LINK_TO_MASTER) return false;
    if (f->raw[1]<ARX_LINK_REQ || f->raw[1]>ARX_LINK_ARM ||
        f->raw[7]>8u || f->raw[18]!=0x20u) return false;
    if (f->raw[1]==ARX_LINK_REQ || f->raw[1]==ARX_LINK_RSP || f->raw[1]==ARX_LINK_FCCFG) {
        const uint32_t max_id=(f->raw[2]&ARX_LINK_FLAG_EXTID)?0x1FFFFFFFu:0x7FFu;
        if (arx_link_can_id(f)>max_id) return false;
    }
    return arx_link_checksum(f->raw)==f->raw[17];
}

static void base(
    ArxLinkFrame *f,uint8_t dest,ArxLinkType type,uint8_t flags,
    uint32_t id,const uint8_t *data,uint8_t dlc,uint8_t seq
) {
    if (!f) return;
    memset(f->raw,0x20,ARX_LINK_FRAME_SIZE);
    f->raw[0]=dest; f->raw[1]=(uint8_t)type; f->raw[2]=flags;
    f->raw[3]=(uint8_t)(id>>24u); f->raw[4]=(uint8_t)(id>>16u);
    f->raw[5]=(uint8_t)(id>>8u);  f->raw[6]=(uint8_t)id;
    if (dlc>8u) dlc=8u;
    f->raw[7]=dlc;
    memset(&f->raw[8],0,8u);
    if (data && dlc) memcpy(&f->raw[8],data,dlc);
    f->raw[16]=seq;
    f->raw[17]=arx_link_checksum(f->raw);
    f->raw[18]=0x20u;
}

void arx_link_build_can(
    ArxLinkFrame *f,uint8_t dest,ArxLinkType type,bool ext,
    uint32_t id,const uint8_t *data,uint8_t dlc,uint8_t seq
) {
    base(f,dest,type,ext?ARX_LINK_FLAG_EXTID:0u,id,data,dlc,seq);
}

void arx_link_build_config(
    ArxLinkFrame *f,uint8_t dest,uint32_t value,uint32_t mask,
    uint16_t timeout_ms,bool auto_fc,uint8_t seq
) {
    const bool extended=(value>0x7FFu)||(mask>0x7FFu);
    base(f,dest,ARX_LINK_CFG,extended?ARX_LINK_FLAG_EXTID:0u,value,NULL,0u,seq);
    f->raw[8]=(uint8_t)(mask>>24u); f->raw[9]=(uint8_t)(mask>>16u);
    f->raw[10]=(uint8_t)(mask>>8u); f->raw[11]=(uint8_t)mask;
    f->raw[12]=(uint8_t)(timeout_ms>>8u); f->raw[13]=(uint8_t)timeout_ms;
    f->raw[14]=auto_fc?ARX_LINK_CFG_AUTOFC:0u;
    f->raw[17]=arx_link_checksum(f->raw);
}

void arx_link_build_flow_control_config(
    ArxLinkFrame *f,uint8_t dest,uint32_t id,bool ext,
    const uint8_t *data,uint8_t len,uint8_t seq
) {
    base(f,dest,ARX_LINK_FCCFG,ext?ARX_LINK_FLAG_EXTID:0u,id,data,len,seq);
}

void arx_link_build_arm(ArxLinkFrame *f,uint8_t dest,bool enabled,uint8_t seq) {
    base(f,dest,ARX_LINK_ARM,enabled?ARX_LINK_FLAG_ARM_ON:0u,0u,NULL,0u,seq);
}

void arx_link_build_end(ArxLinkFrame *f,bool no_data,uint8_t seq) {
    base(f,ARX_LINK_TO_MASTER,ARX_LINK_END,no_data?ARX_LINK_FLAG_NODATA:0u,0u,NULL,0u,seq);
}

uint32_t arx_link_can_id(const ArxLinkFrame *f) {
    if (!f) return 0u;
    return ((uint32_t)f->raw[3]<<24u)|((uint32_t)f->raw[4]<<16u)|
           ((uint32_t)f->raw[5]<<8u)|(uint32_t)f->raw[6];
}
uint8_t arx_link_dlc(const ArxLinkFrame *f) {
    return f?(f->raw[7]>8u?8u:f->raw[7]):0u;
}
const uint8_t *arx_link_data(const ArxLinkFrame *f) {
    return f?&f->raw[8]:NULL;
}
