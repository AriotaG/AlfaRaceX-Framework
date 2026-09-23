#ifndef ARX_WINDOWS_H
#define ARX_WINDOWS_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_WINDOWS_DISABLED = 0,
    ARX_WINDOWS_ONE_CLICK = 1,
    ARX_WINDOWS_MULTI_CLICK = 2
} ArxWindowMode;

typedef struct {
    ArxWindowMode close_mode;
    ArxWindowMode open_mode;

    uint8_t lock_clicks;
    uint8_t unlock_clicks;
    uint8_t requestor;
    uint8_t fob;

    uint32_t last_lock_ms;
    uint32_t last_unlock_ms;

    uint8_t close_phase;
    bool open_active;
} ArxWindows;

void arx_windows_init(ArxWindows *f);
void arx_windows_observe_rf(ArxWindows *f, const ArxCanFrame *frame, uint32_t now_ms);
bool arx_windows_build_action(ArxWindows *f, const ArxCanFrame *template_1ef, uint32_t now_ms, ArxCanFrame *out);

#endif
