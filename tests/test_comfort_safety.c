#include "arx/features/arx_windows.h"
#include "arx/features/arx_park_mirror.h"
#include "arx/arx_crc.h"
#include "arx/arx_runtime.h"
#include <assert.h>
#include <stdio.h>

static void windows(void) {
    ArxWindows w;ArxCanFrame out;
    ArxCanFrame rf={.bus=ARX_BUS_C1,.id=0x1EF,.dlc=8,.data={0,2,0x16}};
    arx_windows_init(&w);
    arx_windows_observe_rf(&w,&rf,10000);
    arx_windows_observe_rf(&w,&rf,11000);
    assert(!arx_windows_build_action(&w,&rf,15001,&out)); /* Disabled must stay passive. */
    w.close_mode=ARX_WINDOWS_ONE_CLICK;
    arx_windows_observe_rf(&w,&rf,20000);
    assert(!arx_windows_build_action(&w,&rf,24000,&out));
    assert(arx_windows_build_action(&w,&rf,24001,&out));
    assert(out.data[1]==3 && out.data[2]==6 && out.data[7]==arx_crc8_sae_j1850(out.data,7));
    w.close_mode=ARX_WINDOWS_DISABLED;
    assert(!arx_windows_build_action(&w,&rf,24002,&out));
    assert(w.close_phase==0 && w.lock_clicks==0);
    w.close_mode=ARX_WINDOWS_MULTI_CLICK;
    arx_windows_observe_rf(&w,&rf,30000);
    assert(w.close_phase==0);
    arx_windows_observe_rf(&w,&rf,31000);
    assert(w.close_phase==1);
    rf.data[2]=0x34; /* Passive entry unlock still cancels closing. */
    arx_windows_observe_rf(&w,&rf,32000);
    assert(!arx_windows_build_action(&w,&rf,36001,&out));
    assert(w.close_phase==0 && w.lock_clicks==0);
    w.open_mode=ARX_WINDOWS_ONE_CLICK;
    rf.data[2]=0x36;
    arx_windows_observe_rf(&w,&rf,40000);
    assert(!arx_windows_build_action(&w,&rf,43500,&out));
    assert(arx_windows_build_action(&w,&rf,43501,&out));
    assert(out.data[1]==2 && out.data[2]==0xB6);
    w.open_mode=ARX_WINDOWS_DISABLED;
    assert(!arx_windows_build_action(&w,&rf,43502,&out));
}

static unsigned window_frames;
static ArxStatus can_send(const ArxCanFrame *frame,void *user) {
    (void)user;if(frame->id==0x1EFu)window_frames++;return ARX_STATUS_OK;
}
static void tick_can(ArxRuntime *r,uint32_t now) {
    arx_runtime_tick(r,now);(void)arx_runtime_drain_can(r,now,32);
}
static void runtime_windows(void) {
    ArxRuntime r;ArxRuntimeOps ops={.can_send=can_send};
    ArxCanFrame rf={.bus=ARX_BUS_C1,.id=0x1EF,.dlc=8,.data={0,2,0x16}};
    arx_runtime_init(&r,ARX_RUNTIME_C1,&ops);
    r.config.close_windows_mode=ARX_WINDOWS_ONE_CLICK;
    arx_runtime_apply_config(&r,&r.config,0);
    arx_runtime_on_can(&r,&rf,10000);
    ArxCanFrame heartbeat={.bus=ARX_BUS_C1,.id=0x101,.dlc=3};
    arx_runtime_on_can(&r,&heartbeat,14000); /* Keep the runtime awake without another RF frame. */
    tick_can(&r,14001);
    assert(window_frames==0); /* No periodic replay of a cached RF command. */
    rf.data[2]=0; /* Next live RF status, as in upstream processingMessage0x1EF. */
    arx_runtime_on_can(&r,&rf,14002);tick_can(&r,14002);
    assert(window_frames==1);
    tick_can(&r,14003);tick_can(&r,14004);
    assert(window_frames==1);
    r.config.close_windows_mode=ARX_WINDOWS_DISABLED;
    arx_runtime_apply_config(&r,&r.config,14005);
    arx_runtime_on_can(&r,&rf,14006);tick_can(&r,14006);
    assert(window_frames==1);
}

static void mirrors(void) {
    ArxParkMirror m;ArxCanFrame out;
    arx_park_mirror_init(&m);m.enabled=true;m.calibrated_park=true;
    assert(m.inter_command_pause_ms==2500 && m.neutral_transient_ms==500);
    arx_park_mirror_update(&m,0x0E,2,800,100);
    arx_park_mirror_update(&m,0x0D,0,800,200);
    assert(m.request_restore && !m.request_left_park && m.restore_request_ms==200);
    m.calibrated_normal=true;m.capture_normal=false;m.source_steady=true;
    assert(!arx_park_mirror_build_command(&m,2699,&out));
    assert(arx_park_mirror_build_command(&m,2700,&out));
    /* Upstream stops restoration after 15 s plus its inter-command pause. */
    assert(!arx_park_mirror_build_command(&m,17701,&out));
    assert(!m.request_restore);
    arx_park_mirror_init(&m);m.enabled=true;m.calibrated_park=true;
    arx_park_mirror_update(&m,0x0E,1,800,100);
    arx_park_mirror_update(&m,0x04,0,800,200);
    arx_park_mirror_update(&m,0x04,0,800,10200);
    assert(!m.request_restore);
    arx_park_mirror_update(&m,0x04,0,800,10201);
    assert(m.request_restore && !m.request_right_park);
    /* No zero-timestamp sentinel or rollover underflow in the delayed path. */
    arx_park_mirror_init(&m);m.enabled=true;m.calibrated_park=true;
    arx_park_mirror_update(&m,0x0E,2,800,UINT32_MAX-100);
    arx_park_mirror_update(&m,0x04,0,800,0);
    arx_park_mirror_update(&m,0x04,0,800,10000);
    assert(!m.request_restore);
    arx_park_mirror_update(&m,0x04,0,800,10001);
    assert(m.request_restore);
    arx_park_mirror_init(&m);m.enabled=true;m.calibrated_park=true;
    m.calibrated_normal=true;m.source_steady=true;
    arx_park_mirror_update(&m,0x0E,2,0,100);
    m.capture_normal=false;
    assert(!arx_park_mirror_build_command(&m,3000,&out));
}

int main(void) { windows();mirrors();runtime_windows();puts("Comfort controls: disabled state, cancellation, live RF cadence and restore bounds PASS");return 0; }
