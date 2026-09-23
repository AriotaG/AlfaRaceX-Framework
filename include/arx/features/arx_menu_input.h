#ifndef ARX_MENU_INPUT_H
#define ARX_MENU_INPUT_H

#include "arx/features/arx_menu.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_MENU_INPUT_NONE=0,
    ARX_MENU_INPUT_RENDER,
    ARX_MENU_INPUT_ACTIVATE,
    ARX_MENU_INPUT_VISIBILITY_CHANGED
} ArxMenuInputEvent;

typedef struct {
    uint8_t last_button;
    uint16_t res_hold_frames;
} ArxMenuInput;

void arx_menu_input_init(ArxMenuInput *i);

ArxMenuInputEvent arx_menu_input_on_button(
    ArxMenuInput *input,
    ArxMenuState *menu,
    const ArxMenuCapabilities *caps,
    uint8_t button,
    uint8_t parameter_page_count,
    uint8_t parameter_setup_page_count
);

#endif
