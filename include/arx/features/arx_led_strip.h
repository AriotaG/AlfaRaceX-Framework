#ifndef ARX_LED_STRIP_H
#define ARX_LED_STRIP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_LED_COUNT              46u
#define ARX_WS2812_BITS_PER_LED    24u
#define ARX_WS2812_RESET_SLOTS     50u
#define ARX_WS2812_PWM_WORDS       (ARX_LED_COUNT*ARX_WS2812_BITS_PER_LED+ARX_WS2812_RESET_SLOTS)
#define ARX_WS2812_ZERO_TICKS      17u
#define ARX_WS2812_ONE_TICKS       34u

typedef struct { uint8_t r,g,b; } ArxRgb;

typedef struct {
    bool enabled;
    float filtered_level;
    uint8_t gear;
    uint32_t last_render_ms;
    uint32_t last_pedal_message_ms;
    uint32_t shutdown_timeout_ms;
    uint32_t pattern_seed;
} ArxLedStrip;

void arx_led_strip_init(ArxLedStrip *f);
float arx_led_strip_scale_pedal_byte(uint8_t raw);
bool arx_led_strip_update_pedal(ArxLedStrip *f, uint8_t raw, uint32_t now_ms);
void arx_led_strip_set_gear(ArxLedStrip *f, uint8_t gear);
bool arx_led_strip_should_shutdown(const ArxLedStrip *f, uint32_t now_ms);

size_t arx_led_strip_render(
    ArxLedStrip *f,
    ArxRgb out[ARX_LED_COUNT],
    uint32_t now_ms
);

size_t arx_led_strip_encode_ws2812_brg(
    const ArxRgb rgb[ARX_LED_COUNT],
    uint16_t *pwm,
    size_t capacity
);

#endif
