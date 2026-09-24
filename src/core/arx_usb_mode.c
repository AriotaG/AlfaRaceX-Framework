#include "arx/arx_usb_mode.h"
#include <string.h>

void arx_usb_mode_init(ArxUsbModeManager *m) {
    if(!m)return;
    memset(m,0,sizeof(*m));
    m->activation_timeout_ms=10000u;
    m->diagnostic_idle_exit_ms=120000u;
}

bool arx_usb_mode_request(ArxUsbModeManager *m,ArxUsbMode mode,uint32_t now_ms) {
    if(!m)return false;

    if(mode==ARX_USB_MODE_NONE){
        m->queued_mode=ARX_USB_MODE_NONE;
        if(m->state==ARX_USB_DETACHED){
            m->mode=ARX_USB_MODE_NONE;
            return true;
        }
        m->state=ARX_USB_DETACH_REQUESTED;
        return true;
    }

    if(m->state!=ARX_USB_DETACHED &&
       m->mode!=ARX_USB_MODE_NONE &&
       m->mode!=mode){
        /* USB classes are mutually exclusive on this endpoint set. Detach the
           current class first, then attach the requested class on the next
           process cycle. */
        m->queued_mode=mode;
        m->state=ARX_USB_DETACH_REQUESTED;
        return true;
    }

    m->queued_mode=ARX_USB_MODE_NONE;
    m->mode=mode;
    m->activation_ms=now_ms;
    m->last_host_seen_ms=now_ms;
    m->last_command_ms=now_ms;
    m->state=ARX_USB_ATTACH_REQUESTED;
    return true;
}

void arx_usb_mode_note_configured(ArxUsbModeManager *m,uint32_t now_ms) {
    if(!m)return;
    m->state=ARX_USB_CONFIGURED;
    m->last_host_seen_ms=now_ms;
}

void arx_usb_mode_note_command(ArxUsbModeManager *m,uint32_t now_ms) {
    if(!m)return;
    m->last_command_ms=now_ms;
    m->last_host_seen_ms=now_ms;
}

bool arx_usb_mode_process(ArxUsbModeManager *m,uint32_t now_ms,const ArxUsbOps *ops) {
    if(!m)return false;

    if(m->state==ARX_USB_ATTACH_REQUESTED){
        if(!ops||!ops->attach||!ops->attach(m->mode,ops->user))return false;
        m->state=ARX_USB_WAIT_HOST;
        return true;
    }

    if(m->state==ARX_USB_WAIT_HOST &&
       now_ms-m->activation_ms>=m->activation_timeout_ms){
        m->state=ARX_USB_DETACH_REQUESTED;
    }

    if(m->state==ARX_USB_CONFIGURED &&
       m->mode==ARX_USB_MODE_DIAGNOSTIC &&
       now_ms-m->last_command_ms>=m->diagnostic_idle_exit_ms){
        m->state=ARX_USB_DETACH_REQUESTED;
    }

    if(m->state==ARX_USB_DETACH_REQUESTED){
        if(ops&&ops->detach&&!ops->detach(ops->user))return false;
        const ArxUsbMode next=m->queued_mode;
        m->queued_mode=ARX_USB_MODE_NONE;
        m->state=ARX_USB_DETACHED;
        m->mode=ARX_USB_MODE_NONE;

        if(next!=ARX_USB_MODE_NONE){
            m->mode=next;
            m->activation_ms=now_ms;
            m->last_host_seen_ms=now_ms;
            m->last_command_ms=now_ms;
            m->state=ARX_USB_ATTACH_REQUESTED;
        }
        return true;
    }

    return false;
}
