#ifndef ARX_PERFORMANCE_H
#define ARX_PERFORMANCE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_RUN_IDLE=0,
    ARX_RUN_ACTIVE,
    ARX_RUN_COMPLETE,
    ARX_RUN_MISSED
} ArxRunState;

typedef struct {
    ArxRunState zero_to_100_state;
    ArxRunState hundred_to_200_state;

    float previous_speed_kmh;
    float zero_to_100_s;
    float hundred_to_200_s;

    float best_zero_to_100_s;
    float best_hundred_to_200_s;

    uint32_t zero_to_100_start_ms;
    uint32_t hundred_to_200_start_ms;

    bool best_dirty;
} ArxPerformanceStats;

void arx_performance_init(ArxPerformanceStats *s);
void arx_performance_set_best(ArxPerformanceStats *s, float best_0_100, float best_100_200);
void arx_performance_reset_best(ArxPerformanceStats *s);
void arx_performance_update(ArxPerformanceStats *s, float speed_kmh, uint32_t now_ms);

#endif
