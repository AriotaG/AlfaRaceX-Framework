#include "arx/features/arx_drive_style.h"
#include "arx/features/arx_led_strip.h"
#include "arx/arx_usb_mode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {int attach;int detach;ArxUsbMode last_usb_mode;} UsbCalls;
static bool ua(ArxUsbMode mode,void *u){UsbCalls *c=(UsbCalls*)u;c->attach++;c->last_usb_mode=mode;return true;}
static bool ud(void *u){((UsbCalls*)u)->detach++;return true;}

int main(void) {
    ArxDriveStyleControl d;
    arx_drive_style_init(&d);
    d.enabled=true;
    d.inversion_active=true;
    d.actual_mode=ARX_DNA_DYNAMIC;

    ArxCanFrame src={.bus=ARX_BUS_C1,.id=0x384,.dlc=8,.data={0,0x08,0,0,0,0,0,0}};
    ArxCanFrame out;
    arx_drive_style_observe_384_c1(&d,&src);
    assert(arx_drive_style_transform_384_c1(&d,&src,&out));
    assert((out.data[1]&0x7Cu)==0x30u);

    /* Cosmetic transforms are controlled by each node's synchronized inversion state,
       not directly by the local preference flag. */
    d.show_race_mask=false;
    src=(ArxCanFrame){.bus=ARX_BUS_BH,.id=0x46C,.dlc=8,.data={0,0,0,0,0,0,0,0x02}};
    assert(arx_drive_style_transform_46c(&d,&src,&out));
    assert((out.data[7]&0x1Fu)==0x0Cu);

    src=(ArxCanFrame){.bus=ARX_BUS_C1,.id=0x5A8,.dlc=8};
    assert(arx_drive_style_transform_5a8(&d,&src,&out));
    assert((out.data[4]&0x78u)==0x30u);

    src=(ArxCanFrame){.bus=ARX_BUS_C1,.id=0x384,.dlc=8,.data={0,0x08,0,0,0,0,0,0}};
    arx_drive_style_observe_384_c1(&d,&src);
    assert(arx_drive_style_periodic_384_c1(&d,800u,51u,&out));
    assert((out.data[1]&0x7Cu)==0x30u);

    ArxLedStrip l;
    arx_led_strip_init(&l); l.enabled=true;
    assert(arx_led_strip_scale_pedal_byte(180u)>99.9f);
    assert(arx_led_strip_update_pedal(&l,51u,100u));
    /* 51 is already ~28%, clipped to 24 before filtering, not zero. */
    assert(l.filtered_level>2.39f && l.filtered_level<2.41f);
    l.filtered_level=24.0f;
    ArxRgb rgb[ARX_LED_COUNT];
    assert(arx_led_strip_render(&l,rgb,200u)==ARX_LED_COUNT);
    uint16_t pwm[ARX_WS2812_PWM_WORDS];
    assert(arx_led_strip_encode_ws2812_brg(rgb,pwm,ARX_WS2812_PWM_WORDS)==ARX_WS2812_PWM_WORDS);
    for(size_t i=ARX_WS2812_PWM_WORDS-ARX_WS2812_RESET_SLOTS;i<ARX_WS2812_PWM_WORDS;i++)
        assert(pwm[i]==0u);

    ArxUsbModeManager um;
    arx_usb_mode_init(&um);
    UsbCalls calls={0};
    ArxUsbOps ops={ua,ud,&calls};
    assert(arx_usb_mode_request(&um,ARX_USB_MODE_DIAGNOSTIC,100u));
    assert(arx_usb_mode_process(&um,100u,&ops));
    assert(calls.attach==1 && um.state==ARX_USB_WAIT_HOST);
    arx_usb_mode_note_configured(&um,200u);
    arx_usb_mode_note_command(&um,200u);
    assert(!arx_usb_mode_process(&um,1000u,&ops));
    assert(!arx_usb_mode_process(&um,11000u,&ops)); /* configured session is not the attach timeout */
    assert(arx_usb_mode_process(&um,120201u,&ops));
    assert(calls.detach==1 && um.mode==ARX_USB_MODE_NONE);

    arx_usb_mode_init(&um);
    memset(&calls,0,sizeof(calls));
    assert(arx_usb_mode_request(&um,ARX_USB_MODE_SNIFFER,0u));
    assert(arx_usb_mode_process(&um,0u,&ops));
    arx_usb_mode_note_configured(&um,1u);
    assert(!arx_usb_mode_process(&um,600000u,&ops));
    assert(um.state==ARX_USB_CONFIGURED && calls.detach==0);

    /* RC5: switching from the deployed BH/C2 MSC class to CDC must detach
       first and then attach the requested class on a later process cycle. */
    arx_usb_mode_init(&um);
    memset(&calls,0,sizeof(calls));
    assert(arx_usb_mode_request(&um,ARX_USB_MODE_LEGACY_MSC,0u));
    assert(arx_usb_mode_process(&um,0u,&ops));
    assert(calls.attach==1 && calls.last_usb_mode==ARX_USB_MODE_LEGACY_MSC);
    arx_usb_mode_note_configured(&um,1u);
    assert(arx_usb_mode_request(&um,ARX_USB_MODE_SNIFFER,2u));
    assert(um.state==ARX_USB_DETACH_REQUESTED);
    assert(arx_usb_mode_process(&um,2u,&ops));
    assert(calls.detach==1 && um.state==ARX_USB_ATTACH_REQUESTED);
    assert(arx_usb_mode_process(&um,3u,&ops));
    assert(calls.attach==2 && calls.last_usb_mode==ARX_USB_MODE_SNIFFER);

    puts("drive/led/usb tests: OK");
    return 0;
}
