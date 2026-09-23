#include "arx/features/arx_performance.h"
#include <string.h>

void arx_performance_init(ArxPerformanceStats *s) {
    if (!s) return;
    memset(s,0,sizeof(*s));
    s->zero_to_100_s=20.001f;
    s->hundred_to_200_s=40.001f;
    s->best_zero_to_100_s=20.001f;
    s->best_hundred_to_200_s=40.001f;
}

void arx_performance_set_best(ArxPerformanceStats *s,float a,float b) {
    if (!s) return;
    s->best_zero_to_100_s=a;
    s->best_hundred_to_200_s=b;
    s->best_dirty=false;
}

void arx_performance_reset_best(ArxPerformanceStats *s) {
    if (!s) return;
    s->best_zero_to_100_s=20.001f;
    s->best_hundred_to_200_s=40.001f;
    s->best_dirty=true;
}

static uint32_t compensated_start(uint32_t now_ms) {
    return now_ms>=10u ? now_ms-10u : 0u;
}

void arx_performance_update(ArxPerformanceStats *s,float speed,uint32_t now) {
    if (!s) return;

    if (s->previous_speed_kmh<0.0625f && speed>=0.0625f) {
        s->zero_to_100_start_ms=compensated_start(now);
        s->zero_to_100_state=ARX_RUN_ACTIVE;
    }

    if (s->zero_to_100_state==ARX_RUN_ACTIVE) {
        s->zero_to_100_s=(float)(now-s->zero_to_100_start_ms)/1000.0f;
        if (speed>=100.0f) {
            s->zero_to_100_state=ARX_RUN_COMPLETE;
            if (s->zero_to_100_s<s->best_zero_to_100_s) {
                s->best_zero_to_100_s=s->zero_to_100_s;
                s->best_dirty=true;
            }
        } else if (s->zero_to_100_s>20.0f) {
            s->zero_to_100_state=ARX_RUN_MISSED;
        }
    }

    if (s->previous_speed_kmh<=100.0f && speed>100.0f) {
        s->hundred_to_200_start_ms=compensated_start(now);
        s->hundred_to_200_state=ARX_RUN_ACTIVE;
    }

    if (s->hundred_to_200_state==ARX_RUN_ACTIVE) {
        s->hundred_to_200_s=(float)(now-s->hundred_to_200_start_ms)/1000.0f;
        if (speed>=200.0f) {
            s->hundred_to_200_state=ARX_RUN_COMPLETE;
            if (s->hundred_to_200_s<s->best_hundred_to_200_s) {
                s->best_hundred_to_200_s=s->hundred_to_200_s;
                s->best_dirty=true;
            }
        } else if (s->hundred_to_200_s>40.0f) {
            s->hundred_to_200_state=ARX_RUN_MISSED;
        }
    }

    s->previous_speed_kmh=speed;
}
