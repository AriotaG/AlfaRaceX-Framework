#include "arx/features/arx_dyno.h"
#include "arx/features/arx_faults.h"
#include "arx/features/arx_seatbelt.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    ArxDyno d;
    arx_dyno_init(&d);
    ArxCanFrame tx;

    assert(arx_dyno_toggle(&d,1,&tx));
    assert(tx.id==0x18DA28F1u && tx.data[1]==0x10);

    ArxCanFrame r={.bus=ARX_BUS_C2,.id=0x18DAF128u,.extended_id=true,.dlc=7,
        .data={0x06,0x50,0x03,0,0,0,0}};
    assert(arx_dyno_on_response(&d,&r,2,&tx) && tx.data[1]==0x22);

    r.dlc=5; r.data[0]=0x05; r.data[1]=0x62; r.data[2]=0x30; r.data[3]=0x02; r.data[4]=0;
    assert(arx_dyno_on_response(&d,&r,3,&tx) && tx.data[1]==0x2E && tx.data[4]==0xFF);

    r.dlc=4; r.data[0]=0x03; r.data[1]=0x6E; r.data[2]=0x30; r.data[3]=0x02;
    assert(!arx_dyno_on_response(&d,&r,4,&tx));
    assert(d.enabled && d.state==ARX_DYNO_IDLE);

    ArxFaultManager f;
    arx_faults_init(&f,0x18DA40F1u,0x18DAF140u);
    assert(arx_faults_start_read(&f,10,&tx) && tx.data[1]==0x10);

    ArxCanFrame fr={.bus=ARX_BUS_C1,.id=0x18DAF140u,.extended_id=true,.dlc=7,
        .data={0x06,0x50,0x03,0,0,0,0}};
    assert(arx_faults_on_response(&f,&fr,11,&tx) && tx.data[1]==0x19);

    fr.dlc=8; fr.data[0]=0x07; fr.data[1]=0x59; fr.data[2]=0x02; fr.data[3]=0xFF;
    fr.data[4]=0x12; fr.data[5]=0x34; fr.data[6]=0x56; fr.data[7]=0xAA;
    assert(!arx_faults_on_response(&f,&fr,12,&tx));
    assert(f.state==ARX_FAULTS_COMPLETE && f.dtc_count==1u);
    assert(f.dtc[0].bytes[0]==0x12 && f.dtc[0].status==0xAA);

    ArxSeatbelt s;
    arx_seatbelt_init(&s);
    s.feature_enabled=true;
    assert(arx_seatbelt_request(&s,false,100,&tx)&&tx.data[1]==0x10);

    ArxCanFrame sr={.bus=ARX_BUS_C1,.id=0x18DAF160u,.extended_id=true,.dlc=7,
        .data={0x06,0x50,0x03,0,0,0,0}};
    assert(arx_seatbelt_on_response(&s,&sr,101,&tx)&&tx.data[1]==0x2F&&tx.data[5]==0);

    sr.dlc=4; sr.data[0]=3; sr.data[1]=0x6F; sr.data[2]=0x55; sr.data[3]=0xA0;
    assert(!arx_seatbelt_on_response(&s,&sr,102,&tx));
    assert(s.state==ARX_SEATBELT_DISABLED);

    puts("dyno/fault tests: OK");
    return 0;
}
