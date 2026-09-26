#ifndef ARX_RUNTIME_H
#define ARX_RUNTIME_H

#include "arx/arx_can.h"
#include "arx/arx_config.h"
#include "arx/arx_interchip.h"
#include "arx/arx_link.h"
#include "arx/arx_power.h"
#include "arx/arx_storage.h"
#include "arx/arx_usb_mode.h"
#include "arx/arx_vehicle_state.h"

#include "arx/features/arx_acc.h"
#include "arx/features/arx_awd.h"
#include "arx/features/arx_brake_override.h"
#include "arx/features/arx_dashboard.h"
#include "arx/features/arx_dpf_alert.h"
#include "arx/features/arx_drive_style.h"
#include "arx/features/arx_dynamic_shift.h"
#include "arx/features/arx_dyno.h"
#include "arx/features/arx_elm327.h"
#include "arx/features/arx_elm_transport.h"
#include "arx/features/arx_faults.h"
#include "arx/features/arx_immobilizer.h"
#include "arx/features/arx_led_strip.h"
#include "arx/features/arx_max_hold.h"
#include "arx/features/arx_menu.h"
#include "arx/features/arx_menu_input.h"
#include "arx/features/arx_performance.h"
#include "arx/features/arx_odometer.h"
#include "arx/features/arx_park_mirror.h"
#include "arx/features/arx_park_mute.h"
#include "arx/features/arx_pedal_controller.h"
#include "arx/features/arx_qv_exhaust.h"
#include "arx/features/arx_route_service.h"
#include "arx/features/arx_seatbelt.h"
#include "arx/features/arx_sniffer.h"
#include "arx/features/arx_start_stop.h"
#include "arx/features/arx_windows.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    ARX_RUNTIME_C1 = 0,
    ARX_RUNTIME_C2,
    ARX_RUNTIME_BH
} ArxRuntimeRole;

typedef struct {
    ArxStatus (*can_send)(const ArxCanFrame *frame, void *user);
    bool (*interchip_send)(const uint8_t frame[ARX_INTERCHIP_FRAME_SIZE], void *user);
    bool (*pedal_send)(const uint8_t packet[ARX_PEDAL_PACKET_SIZE], void *user);
    bool (*usb_attach)(ArxUsbMode mode, void *user);
    bool (*usb_detach)(void *user);
    bool (*usb_send)(const uint8_t *data, size_t length, void *user);
    bool (*led_submit)(const ArxRgb rgb[ARX_LED_COUNT], void *user);
    bool (*persist_config)(const ArxRuntimeConfig *config, void *user);
    bool (*persist_performance)(float best_0_100_s, float best_100_200_s, void *user);
    bool (*persist_visibility)(const uint8_t *visible, uint16_t count, void *user);
    bool (*persist_mirror)(const ArxMirrorStorage *mirror, void *user);
    bool (*save_log)(void *user);
    void *user;
} ArxRuntimeOps;

#define ARX_RUNTIME_TELEMETRY_CACHE_MAX 64u

typedef struct {
    ArxRuntimeRole role;
    ArxRuntimeOps ops;

    ArxRuntimeConfig config;
    ArxVehicleState vehicle;
    ArxCanTransport can_tx;
    ArxInterchip interchip;
    ArxPowerManager power;
    ArxUsbModeManager usb_mode;

    ArxStartStop start_stop;
    ArxDynamicShift shift;
    ArxDyno dyno;
    ArxDriveStyleControl drive_style;
    ArxDpfAlert dpf;
    ArxAccControl acc;
    ArxAwdControl awd;
    ArxBrakeOverride brake;
    ArxQvExhaust exhaust;
    ArxSeatbelt seatbelt;
    ArxFaultManager faults;
    ArxImmobilizer immobilizer;
    ArxParkMute park_mute;
    ArxParkMirror park_mirror;
    ArxWindows windows;
    ArxPedalController pedal;
    ArxLedStrip leds;
    ArxSniffer sniffer;
    ArxRouteService route;
    ArxDashboard dashboard;
    ArxMenuState menu;
    ArxMenuInput menu_input;
    ArxMenuCapabilities menu_caps;
    ArxPerformanceStats performance;
    ArxMaxHold max_hold;

#if !defined(ARX_BUILD_C2) && !defined(ARX_BUILD_BH)
    /* C1 owns the host-facing ELM session. Host builds keep these fields so
       integration tests can exercise the complete three-controller topology. */
    ArxElm327 elm;
    ArxElmRouter elm_router;
    ArxElmTransaction elm_transaction;
    ArxElmRequest elm_request;
    ArxElmBus elm_candidates[3];
    uint8_t elm_candidate_count;
    uint8_t elm_candidate_index;
    uint8_t elm_link_sequence;
    bool elm_request_active;
    bool elm_saw_response;
    bool elm_output_overflow;
    uint32_t elm_deadline_ms;
    char elm_line[96];
    uint8_t elm_line_len;
    uint8_t elm_usb_tx[1024];
    uint16_t elm_usb_tx_len;
    uint16_t elm_usb_tx_off;
#endif

    /* Slave-side diagnostic relay state used by C2/BH. */
    bool diag_link_armed;
    bool diag_link_extended;
    uint32_t diag_filter_value;
    uint32_t diag_filter_mask;
    uint16_t diag_timeout_ms;
    uint32_t diag_deadline_ms;
    uint8_t diag_link_sequence;

    bool template_4b1_valid;
    ArxCanFrame template_4b1;

    bool template_1ef_valid;
    ArxCanFrame template_1ef;

    bool template_5ac_valid;
    ArxCanFrame template_5ac;
    bool bh_chime_requested;

    bool engine_running;
    uint32_t engine_running_since_ms;

    bool remote_dyno_active;
    bool remote_brake_forced;
    bool remote_usb_c2_active;
    bool remote_usb_bh_active;

    bool cruise_control_disabled;
    bool menu_was_visible_before_engine_off;
    uint32_t menu_shutdown_requested_ms;
    uint8_t visible_params[60];

    float telemetry_values[ARX_RUNTIME_TELEMETRY_CACHE_MAX];
    uint8_t telemetry_valid[ARX_RUNTIME_TELEMETRY_CACHE_MAX];
    uint8_t telemetry_poll_slot;
    uint32_t telemetry_last_poll_ms;

    uint32_t last_c2_status_request_ms;
    uint32_t last_bh_status_request_ms;

    uint32_t rx_frames;
    uint32_t generated_frames;
    uint32_t pedal_packets;
    uint32_t interchip_tx_frames;
    uint32_t usb_bytes;
} ArxRuntime;

void arx_runtime_init(
    ArxRuntime *rt,
    ArxRuntimeRole role,
    const ArxRuntimeOps *ops
);

void arx_runtime_apply_config(
    ArxRuntime *rt,
    const ArxRuntimeConfig *config,
    uint32_t now_ms
);

void arx_runtime_on_can(
    ArxRuntime *rt,
    const ArxCanFrame *frame,
    uint32_t now_ms
);

void arx_runtime_on_pedal_reply(
    ArxRuntime *rt,
    uint8_t reply_byte
);

void arx_runtime_usb_configured(
    ArxRuntime *rt,
    uint32_t now_ms
);

void arx_runtime_usb_command(
    ArxRuntime *rt,
    uint32_t now_ms
);

void arx_runtime_usb_rx(
    ArxRuntime *rt,
    const uint8_t *data,
    size_t length,
    uint32_t now_ms
);

void arx_runtime_on_interchip(
    ArxRuntime *rt,
    const uint8_t raw[ARX_INTERCHIP_FRAME_SIZE],
    uint32_t now_ms
);

void arx_runtime_queue_config_sync(
    ArxRuntime *rt
);

void arx_runtime_tick(
    ArxRuntime *rt,
    uint32_t now_ms
);

size_t arx_runtime_drain_can(
    ArxRuntime *rt,
    uint32_t now_ms,
    size_t budget
);

size_t arx_runtime_drain_interchip(
    ArxRuntime *rt,
    uint32_t now_ms,
    size_t budget
);

#endif
