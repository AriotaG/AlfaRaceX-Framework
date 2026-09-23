#ifndef ARX_ACC_H
#define ARX_ACC_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_ACC_AUTOSTART_OFF = 0,
    ARX_ACC_AUTOSTART_RES = 1,
    ARX_ACC_AUTOSTART_PLUS = 2
} ArxAccAutostartMode;

typedef struct {
    bool virtual_pad_enabled;
    bool has_virtual_pad_enabled;
    ArxAccAutostartMode autostart_mode;

    uint8_t has_press_frames_remaining;
    uint8_t autostart_burst_count;
    uint32_t last_autostart_burst_ms;
} ArxAccControl;

void arx_acc_init(ArxAccControl *f);
void arx_acc_request_has_press(ArxAccControl *f);
bool arx_acc_transform_2fa(
    ArxAccControl *f,
    const ArxCanFrame *source,
    uint8_t acc_status,
    bool vehicle_stationary,
    bool acc_braking,
    uint32_t now_ms,
    ArxCanFrame *out
);

#endif
