#include "arx/arx_runtime.h"
#include "arx/features/arx_menu_input.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t frames[16][ARX_INTERCHIP_FRAME_SIZE];
    size_t count;
} IcSink;

static bool ic_send(const uint8_t frame[ARX_INTERCHIP_FRAME_SIZE], void *user) {
    IcSink *s=(IcSink*)user;
    if(s->count>=16u) return false;
    memcpy(s->frames[s->count++],frame,ARX_INTERCHIP_FRAME_SIZE);
    return true;
}

static void menu_unit(void) {
    ArxMenuState m;
    ArxMenuInput i;
    ArxMenuCapabilities caps={
        .read_faults_available=true,.clear_faults_available=true,.dyno_available=true,
        .esc_tc_available=true,.brake_available=true,.awd_available=true,
        .has_available=true,.exhaust_available=true
    };
    arx_menu_init(&m);
    arx_menu_input_init(&i);

    for(unsigned n=0;n<50u;n++)
        assert(arx_menu_input_on_button(&i,&m,&caps,0x90u,55u,55u)==ARX_MENU_INPUT_NONE);
    assert(!m.visible);
    assert(arx_menu_input_on_button(&i,&m,&caps,0x90u,55u,55u)==ARX_MENU_INPUT_VISIBILITY_CHANGED);
    assert(m.visible);
    assert(arx_menu_input_on_button(&i,&m,&caps,0x10u,55u,55u)==ARX_MENU_INPUT_NONE);

    assert(arx_menu_input_on_button(&i,&m,&caps,0x18u,55u,55u)==ARX_MENU_INPUT_RENDER);
    assert(m.main_page==1u);
    (void)arx_menu_input_on_button(&i,&m,&caps,0x10u,55u,55u);

    assert(arx_menu_input_on_button(&i,&m,&caps,0x90u,55u,55u)==ARX_MENU_INPUT_NONE);
    assert(arx_menu_input_on_button(&i,&m,&caps,0x10u,55u,55u)==ARX_MENU_INPUT_ACTIVATE);

    m.level=ARX_MENU_LEVEL_SUB;
    m.main_page=9u;
    m.setup_page=1u;
    i.last_button=0x10u;
    assert(arx_menu_input_on_button(&i,&m,&caps,0x18u,55u,55u)==ARX_MENU_INPUT_RENDER);
    assert(m.setup_page==2u);
    assert(arx_menu_input_on_button(&i,&m,&caps,0x20u,55u,55u)==ARX_MENU_INPUT_RENDER);
    assert(m.setup_page==12u);
}

static void deferred_usb_unit(void) {
    ArxMenuState m;
    ArxRuntimeConfig c;
    arx_menu_init(&m);
    arx_config_defaults(&c);
    m.level=ARX_MENU_LEVEL_SUB;
    m.main_page=9u;

    m.setup_page=29u;
    ArxMenuEffect e=arx_menu_activate_setup(&m,&c);
    assert(e==ARX_MENU_EFFECT_NONE);
    assert(c.sniffer_enabled && !c.elm327_enabled && m.usb_change_pending);

    m.setup_page=30u;
    e=arx_menu_activate_setup(&m,&c);
    assert(e==ARX_MENU_EFFECT_NONE);
    assert(!c.sniffer_enabled && c.elm327_enabled);

    /* Test the save gate explicitly with a pending USB preference. */
    m.usb_change_pending=true;
    m.setup_page=0u;
    e=arx_menu_activate_setup(&m,&c);
    assert((e&ARX_MENU_EFFECT_SAVE_CONFIG)!=0u);
    assert((e&ARX_MENU_EFFECT_SYNC_CONFIG)!=0u);
    assert((e&ARX_MENU_EFFECT_USB_MODE_CHANGED)!=0u);
    assert(!m.usb_change_pending);
}

static void runtime_text_path(void) {
    IcSink sink={0};
    ArxRuntimeOps ops={.interchip_send=ic_send,.user=&sink};
    ArxRuntime c1;
    arx_runtime_init(&c1,ARX_RUNTIME_C1,&ops);

    ArxCanFrame b={.bus=ARX_BUS_C1,.id=0x2FAu,.dlc=3,.data={0x90u,0,0}};
    for(unsigned n=0;n<51u;n++) arx_runtime_on_can(&c1,&b,1000u+n);
    assert(c1.menu.visible);
    assert(c1.interchip.tx.count>=1u);

    assert(arx_runtime_drain_interchip(&c1,2001u,1u)==1u);
    assert(sink.count==1u);
    assert(sink.frames[0][0]==ARX_IC_TO_BH_PARAM_TEXT);

    ArxRuntime bh;
    arx_runtime_init(&bh,ARX_RUNTIME_BH,0);
    arx_runtime_on_interchip(&bh,sink.frames[0],2002u);
    assert(bh.dashboard.repeats_remaining==1u);
    assert(bh.dashboard.text[0]==' '); /* main page zero is intentionally blank */

    /* Release after long press, then navigate to page 1. */
    b.data[0]=0x10u; arx_runtime_on_can(&c1,&b,2100u);
    b.data[0]=0x18u; arx_runtime_on_can(&c1,&b,2101u);
    assert(c1.menu.main_page==1u);
    assert(c1.interchip.tx.count>=1u);
}

int main(void) {
    menu_unit();
    deferred_usb_unit();
    runtime_text_path();
    puts("menu input/runtime tests: OK");
    return 0;
}
