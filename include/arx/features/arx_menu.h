#ifndef ARX_MENU_H
#define ARX_MENU_H

#include "arx/arx_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_MENU_TEXT_LEN 18u
#define ARX_MAIN_MENU_COUNT 16u
#define ARX_SETUP_MENU_COUNT 31u

typedef enum {
    ARX_MENU_LEVEL_MAIN=0,
    ARX_MENU_LEVEL_SUB=1
} ArxMenuLevel;

typedef enum {
    ARX_MENU_EFFECT_NONE=0,
    ARX_MENU_EFFECT_SAVE_CONFIG=1u<<0,
    ARX_MENU_EFFECT_SYNC_CONFIG=1u<<1,
    ARX_MENU_EFFECT_PEDAL_RESYNC=1u<<2,
    ARX_MENU_EFFECT_CAPTURE_PARK_MIRROR=1u<<3,
    ARX_MENU_EFFECT_USB_MODE_CHANGED=1u<<4,
    ARX_MENU_EFFECT_RESET_STATS=1u<<5,
    ARX_MENU_EFFECT_SAVE_LOG=1u<<6
} ArxMenuEffect;

typedef struct {
    ArxMenuLevel level;
    uint8_t main_page;
    uint8_t setup_page;
    uint8_t param_page;
    bool visible;
    bool commands_enabled;
    bool max_hold;
    bool usb_change_pending;
} ArxMenuState;

typedef struct {
    bool read_faults_available;
    bool clear_faults_available;
    bool dyno_available;
    bool esc_tc_available;
    bool brake_available;
    bool awd_available;
    bool has_available;
    bool exhaust_available;
} ArxMenuCapabilities;

void arx_menu_init(ArxMenuState *m);
void arx_menu_set_visible(ArxMenuState *m,bool visible);

uint8_t arx_menu_next_main(
    ArxMenuState *m,
    const ArxMenuCapabilities *caps,
    uint8_t step
);
uint8_t arx_menu_prev_main(
    ArxMenuState *m,
    const ArxMenuCapabilities *caps,
    uint8_t step
);
void arx_menu_next_setup(ArxMenuState *m,uint8_t step);
void arx_menu_prev_setup(ArxMenuState *m,uint8_t step);

bool arx_menu_main_text(
    uint8_t page,
    char out[ARX_MENU_TEXT_LEN+1u]
);

bool arx_menu_setup_text(
    uint8_t page,
    const ArxRuntimeConfig *cfg,
    char out[ARX_MENU_TEXT_LEN+1u]
);

ArxMenuEffect arx_menu_activate_setup(
    ArxMenuState *m,
    ArxRuntimeConfig *cfg
);

#endif
