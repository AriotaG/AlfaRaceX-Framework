#ifndef ARX_DRIVE_STYLE_H
#define ARX_DRIVE_STYLE_H

#include "arx/arx_can.h"
#include "arx/arx_vehicle_state.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool enabled;
    bool show_race_mask;
    bool inversion_active;
    ArxDnaMode actual_mode;

    uint8_t lane_clicks;
    uint32_t lane_press_started_ms;
    uint32_t first_lane_click_ms;

    bool c1_384_valid;
    ArxCanFrame c1_384_template;
    uint32_t last_periodic_384_ms;
} ArxDriveStyleControl;

void arx_drive_style_init(ArxDriveStyleControl *f);
void arx_drive_style_update_actual_mode(ArxDriveStyleControl *f, ArxDnaMode mode);

bool arx_drive_style_lane_event(
    ArxDriveStyleControl *f,
    bool button_pressed,
    bool dyno_active_or_busy,
    uint32_t now_ms,
    bool *has_double_tap
);

void arx_drive_style_observe_384_c1(
    ArxDriveStyleControl *f,
    const ArxCanFrame *src
);

bool arx_drive_style_periodic_384_c1(
    ArxDriveStyleControl *f,
    uint16_t rpm,
    uint32_t now_ms,
    ArxCanFrame *out
);

bool arx_drive_style_transform_384_c1(const ArxDriveStyleControl *f, const ArxCanFrame *src, ArxCanFrame *out);
bool arx_drive_style_transform_384_c2(const ArxDriveStyleControl *f, const ArxCanFrame *src, ArxCanFrame *out);
bool arx_drive_style_transform_46c(const ArxDriveStyleControl *f, const ArxCanFrame *src, ArxCanFrame *out);
bool arx_drive_style_transform_4af(const ArxDriveStyleControl *f, const ArxCanFrame *src, ArxCanFrame *out);
bool arx_drive_style_transform_5a8(const ArxDriveStyleControl *f, const ArxCanFrame *src, ArxCanFrame *out);
bool arx_drive_style_transform_25a(const ArxDriveStyleControl *f, const ArxCanFrame *src, ArxCanFrame *out);

#endif
