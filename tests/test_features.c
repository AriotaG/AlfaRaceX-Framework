#include "arx/features/arx_start_stop.h"
#include "arx/features/arx_dpf_alert.h"
#include "arx/features/arx_odometer.h"
#include "arx/features/arx_acc.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    ArxCanFrame src = {.bus=ARX_BUS_C1,.id=0x4B1,.dlc=8};
    ArxCanFrame out;
    assert(arx_start_stop_build_toggle(&src,&out));
    assert((out.data[5] & 0x38u) == 0x08u);

    ArxDpfAlert dpf;
    arx_dpf_alert_init(&dpf);
    dpf.enabled=true;
    ArxCanFrame regen = {.bus=ARX_BUS_C1,.id=0x5AE,.dlc=8,.data={0,0,0,0,0,8,0,0}};
    bool started=false, ended=false;
    assert(arx_dpf_alert_on_5ae(&dpf,&regen,&started,&ended));
    assert(started && !ended);
    assert(arx_dpf_alert_build_visual(&dpf,&regen,&out));
    assert((out.data[4]&0x04u)!=0u);

    ArxCanFrame odo = {.bus=ARX_BUS_BH,.id=0x356,.dlc=8,.data={0,0,0,0,0x04,0,0,0}};
    assert(arx_odometer_build_no_blink(&odo,&out));
    assert((out.data[4] & 0x04u) == 0u);

    ArxAccControl acc;
    arx_acc_init(&acc);
    acc.has_virtual_pad_enabled=true;
    arx_acc_request_has_press(&acc);
    ArxCanFrame pad = {.bus=ARX_BUS_C1,.id=0x2FA,.dlc=3,.data={0x10,0x00,0x00}};
    assert(arx_acc_transform_2fa(&acc,&pad,0,false,false,1000,&out));
    assert((out.data[1] & 0x10u) != 0u);

    puts("feature tests: OK");
    return 0;
}
