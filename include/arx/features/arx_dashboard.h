#ifndef ARX_DASHBOARD_H
#define ARX_DASHBOARD_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_DASHBOARD_TEXT_LEN 18u
#define ARX_DASHBOARD_FRAME_COUNT (ARX_DASHBOARD_TEXT_LEN/3u)

typedef struct {
    const char *name;
    float value;
    const char *unit;
    bool visible;
} ArxDashboardParam;

typedef struct {
    bool menu_visible;
    size_t page;
    size_t selected_param;
    uint32_t last_activity_ms;

    uint8_t info_code;
    char text[ARX_DASHBOARD_TEXT_LEN];
    uint8_t frame_index;
    uint8_t repeats_remaining;
    uint32_t last_frame_ms;
} ArxDashboard;

void arx_dashboard_init(ArxDashboard *f);
void arx_dashboard_next(ArxDashboard *f, size_t count, uint32_t now_ms);
void arx_dashboard_prev(ArxDashboard *f, size_t count, uint32_t now_ms);

void arx_dashboard_set_text(ArxDashboard *f, const char *text, uint8_t repeats);
bool arx_dashboard_next_telematic_frame(ArxDashboard *f, uint32_t now_ms, ArxCanFrame *out);

#endif
