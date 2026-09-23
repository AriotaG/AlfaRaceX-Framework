#include "arx/features/arx_sniffer.h"
#include "arx/features/arx_dashboard.h"
#include "arx/features/arx_elm327.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    ArxSniffer s;
    arx_sniffer_init(&s); s.enabled=true; arx_sniffer_start(&s,0);
    ArxCanFrame f={.bus=ARX_BUS_C1,.id=0x123,.dlc=3,.timestamp_ms=0x123456,
        .data={0xAA,0xBB,0xCC}};
    assert(arx_sniffer_push(&s,&f));
    uint8_t buf[64];
    assert(arx_sniffer_peek_chunk(&s,19,buf,sizeof(buf))==0u);
    size_t n=arx_sniffer_peek_chunk(&s,20,buf,sizeof(buf));
    assert(n==16u);
    assert(buf[0]==0xA3u && buf[1]==0x56u && buf[2]==0x34u && buf[3]==0x12u);
    assert(buf[4]==0x23u && buf[5]==0x01u);
    assert(buf[8]==0xAAu && buf[9]==0xBBu && buf[10]==0xCCu);
    arx_sniffer_commit_chunk(&s,n,20);
    assert(s.count==0u);

    ArxDashboard d;
    arx_dashboard_init(&d);
    arx_dashboard_set_text(&d,"ABCDEFGHIJKLMNOPQR",1);
    ArxCanFrame out;
    assert(arx_dashboard_next_telematic_frame(&d,51,&out));
    assert(out.id==0x090u && out.dlc==8u);
    assert((out.data[0]&0xF8u)==(5u<<3u));
    assert(out.data[3]=='A' && out.data[5]=='B' && out.data[7]=='C');

    ArxElm327 e;
    arx_elm327_init(&e); e.enabled=true;
    char reply[128];
    arx_elm327_command(&e,"ATH1",reply,sizeof(reply));
    assert(e.headers);
    arx_elm327_command(&e,"ATCAF0",reply,sizeof(reply));
    assert(!e.auto_format);
    arx_elm327_command(&e,"ATSH18DA10F1",reply,sizeof(reply));
    assert(e.tx_extended && e.tx_header==0x18DA10F1u);
    arx_elm327_command(&e,"ATSP7",reply,sizeof(reply));
    assert(e.protocol==ARX_ELM_PROTOCOL_29_500);

    ArxElmRequest req;
    assert(arx_elm327_prepare_request(&e,"0322F190",&req));
    assert(req.can_id==0x18DA10F1u && req.length==4u && req.data[1]==0x22u);

    puts("infrastructure tests: OK");
    return 0;
}
