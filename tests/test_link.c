#include "arx/arx_link.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    ArxLinkFrame f;
    const uint8_t d[4]={0x03,0x22,0xF1,0x90};

    arx_link_build_can(&f,ARX_LINK_TO_C2,ARX_LINK_REQ,true,0x18DA10F1u,d,4,7);
    assert(arx_link_validate(&f));
    assert(f.raw[0]==ARX_LINK_TO_C2 && f.raw[1]==ARX_LINK_REQ);
    assert((f.raw[2]&ARX_LINK_FLAG_EXTID)!=0u);
    assert(arx_link_can_id(&f)==0x18DA10F1u);
    assert(arx_link_dlc(&f)==4u && arx_link_data(&f)[1]==0x22u);
    assert(f.raw[16]==7u);

    arx_link_build_config(&f,ARX_LINK_TO_BH,0x700u,0x7F0u,300u,true,8);
    assert(arx_link_validate(&f));
    assert(arx_link_can_id(&f)==0x700u);
    assert(f.raw[8]==0 && f.raw[10]==0x07 && f.raw[11]==0xF0);
    assert(f.raw[12]==0x01 && f.raw[13]==0x2C);
    assert((f.raw[14]&ARX_LINK_CFG_AUTOFC)!=0);
    assert((f.raw[2]&ARX_LINK_FLAG_EXTID)==0u);

    arx_link_build_config(&f,ARX_LINK_TO_C2,0x18DAF110u,0x1FFFFFFFu,200u,true,9);
    assert(arx_link_validate(&f));
    assert((f.raw[2]&ARX_LINK_FLAG_EXTID)!=0u);

    f.raw[5]^=1u;
    assert(!arx_link_validate(&f));

    puts("link tests: OK");
    return 0;
}
