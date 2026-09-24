#ifndef ARX_USB_MODE_H
#define ARX_USB_MODE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_USB_MODE_NONE = 0,
    ARX_USB_MODE_LEGACY_MSC,
    ARX_USB_MODE_SNIFFER,
    ARX_USB_MODE_DIAGNOSTIC
} ArxUsbMode;

typedef enum {
    ARX_USB_DETACHED = 0,
    ARX_USB_ATTACH_REQUESTED,
    ARX_USB_WAIT_HOST,
    ARX_USB_CONFIGURED,
    ARX_USB_DETACH_REQUESTED
} ArxUsbState;

typedef struct {
    ArxUsbMode mode;
    ArxUsbMode queued_mode;
    ArxUsbState state;
    uint32_t activation_ms;
    uint32_t last_host_seen_ms;
    uint32_t last_command_ms;
    uint32_t activation_timeout_ms;
    uint32_t diagnostic_idle_exit_ms;
} ArxUsbModeManager;

typedef struct {
    bool (*attach)(ArxUsbMode mode, void *user);
    bool (*detach)(void *user);
    void *user;
} ArxUsbOps;

void arx_usb_mode_init(ArxUsbModeManager *m);
bool arx_usb_mode_request(ArxUsbModeManager *m, ArxUsbMode mode, uint32_t now_ms);
void arx_usb_mode_note_configured(ArxUsbModeManager *m, uint32_t now_ms);
void arx_usb_mode_note_command(ArxUsbModeManager *m, uint32_t now_ms);
bool arx_usb_mode_process(ArxUsbModeManager *m, uint32_t now_ms, const ArxUsbOps *ops);

#endif
