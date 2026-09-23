#include "arx/features/arx_start_stop.h"
#include "arx/features/arx_dpf_alert.h"
#include "arx/features/arx_route_service.h"
#include "arx/features/arx_dyno.h"
#include "arx/features/arx_awd.h"
#include "arx/features/arx_brake_override.h"
#include "arx/features/arx_qv_exhaust.h"
#include "arx/features/arx_faults.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    ArxCanFrame fr={0}, out={0}, next={0};

    /* Start/Stop */
    ArxStartStop ss;
    arx_start_stop_init(&ss);
    ss.enabled=true;
    arx_start_stop_on_engine_rpm(&ss,800u,11000u);
    assert(!arx_start_stop_should_toggle(&ss,15000u));
    assert(arx_start_stop_should_toggle(&ss,16000u));

    fr=(ArxCanFrame){.bus=ARX_BUS_C1,.id=0x226,.dlc=3,.data={0,0,0x04}};
    assert(arx_start_stop_observe_status_226(&ss,&fr));
    assert(!ss.vehicle_start_stop_enabled);

    /* DPF starts only on mode 2 */
    ArxDpfAlert dpf;
    arx_dpf_alert_init(&dpf); dpf.enabled=true;
    bool started=false,ended=false;
    fr=(ArxCanFrame){.bus=ARX_BUS_C1,.id=0x5AE,.dlc=8,.data={0,0,0,0,0,0x04,0,0}};
    assert(arx_dpf_alert_on_5ae(&dpf,&fr,&started,&ended));
    assert(!started && !dpf.regeneration_active);
    fr.data[5]=0x08;
    assert(arx_dpf_alert_on_5ae(&dpf,&fr,&started,&ended));
    assert(started && dpf.regeneration_active);
    fr.data[5]=0x00;
    for(int i=0;i<10;i++) { started=false; ended=false; arx_dpf_alert_on_5ae(&dpf,&fr,&started,&ended); assert(!ended); }
    arx_dpf_alert_on_5ae(&dpf,&fr,&started,&ended);
    assert(ended && !dpf.regeneration_active);

    /* Route service */
    ArxRouteService rs;
    arx_route_service_init(&rs); rs.enabled=true;
    fr=(ArxCanFrame){.bus=ARX_BUS_C1,.id=0x18DABAF1,.extended_id=true,.dlc=7,
        .data={0,0,0x01,0,0,0x01,0x23}};
    assert(arx_route_service_on_request(&rs,&fr));
    fr=(ArxCanFrame){.bus=ARX_BUS_C1,.id=0x123,.extended_id=false,.dlc=8,
        .data={1,2,3,4,5,6,7,8}};
    assert(arx_route_service_capture(&rs,&fr,&out));
    assert(out.id==0x18DAF1BAu && out.data[3]==2 && out.data[7]==6);

    /* Dyno read-before-toggle */
    ArxDyno dyno;
    arx_dyno_init(&dyno);
    assert(arx_dyno_toggle(&dyno,100,&out));
    assert(out.data[1]==0x10);
    fr=(ArxCanFrame){.bus=ARX_BUS_C2,.id=0x18DAF128,.extended_id=true,.dlc=7,.data={6,0x50,0x03,0,0,0,0}};
    assert(arx_dyno_on_response(&dyno,&fr,110,&next));
    assert(next.data[1]==0x22);
    fr=(ArxCanFrame){.bus=ARX_BUS_C2,.id=0x18DAF128,.extended_id=true,.dlc=5,.data={5,0x62,0x30,0x02,0x00}};
    assert(arx_dyno_on_response(&dyno,&fr,120,&next));
    assert(next.data[1]==0x2E && next.data[4]==0xFF);

    /* AWD sequence */
    ArxAwdControl awd;
    arx_awd_init(&awd); awd.enabled=true;
    assert(arx_awd_request_disable(&awd,true));
    assert(arx_awd_tick(&awd,100,&out) && out.data[1]==0x10);
    assert(arx_awd_tick(&awd,201,&out) && out.data[1]==0x3E);
    assert(arx_awd_tick(&awd,302,&out) && out.data[1]==0x2F);
    assert(arx_awd_tick(&awd,333,&out) && out.data[1]==0x11);

    /* Brake exact cycle */
    ArxBrakeOverride br;
    arx_brake_override_init(&br); br.enabled=true;
    assert(arx_brake_request_force(&br,true,true));
    assert(arx_brake_tick(&br,100,&out) && out.data[1]==0x10);
    assert(arx_brake_tick(&br,601,&out) && out.data[1]==0x3E);
    assert(arx_brake_tick(&br,1102,&out) && out.data[1]==0x2F);
    assert(arx_brake_tick(&br,1603,&out) && out.data[1]==0x3E);

    /* Exhaust exact cycle */
    ArxQvExhaust ex;
    arx_qv_exhaust_init(&ex); ex.enabled=true; arx_qv_exhaust_toggle(&ex);
    assert(arx_qv_exhaust_tick(&ex,100,&out) && out.data[1]==0x10);
    assert(arx_qv_exhaust_tick(&ex,601,&out) && out.data[1]==0x3E);
    assert(arx_qv_exhaust_tick(&ex,1102,&out) && out.data[1]==0x2F);
    assert(arx_qv_exhaust_tick(&ex,1603,&out) && out.data[1]==0x3E);

    /* DTC request and flow control */
    ArxFaultManager fm;
    arx_faults_init(&fm,0x18DA40F1u,0x18DAF140u);
    assert(arx_faults_start_read(&fm,1,&out) && out.data[1]==0x10);
    fr=(ArxCanFrame){.bus=ARX_BUS_C1,.id=0x18DAF140u,.extended_id=true,.dlc=7,.data={6,0x50,0x03,0,0,0,0}};
    assert(arx_faults_on_response(&fm,&fr,2,&next) && next.data[1]==0x19);

    puts("parity core tests: OK");
    return 0;
}
