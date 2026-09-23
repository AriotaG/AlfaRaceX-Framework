#include "arx/features/arx_led_strip.h"
#include <string.h>

static const float tan45[46]={
0.0f,0.017455065f,0.034920769f,0.052407779f,0.069926812f,0.087488664f,
0.105104235f,0.122784561f,0.140540835f,0.15838444f,0.176326981f,
0.194380309f,0.212556562f,0.230868191f,0.249328003f,0.267949192f,
0.286745386f,0.305730681f,0.324919696f,0.344327613f,0.363970234f,
0.383864035f,0.404026226f,0.424474816f,0.445228685f,0.466307658f,
0.487732589f,0.509525449f,0.531709432f,0.554309051f,0.577350269f,
0.600860619f,0.624869352f,0.649407593f,0.674508517f,0.700207538f,
0.726542528f,0.75355405f,0.781285627f,0.809784033f,0.839099631f,
0.869286738f,0.900404044f,0.932515086f,0.965688775f,1.0f
};

void arx_led_strip_init(ArxLedStrip *f) {
    if(!f) return;
    memset(f,0,sizeof(*f));
    f->shutdown_timeout_ms=10000u;
    f->pattern_seed=1u;
}

float arx_led_strip_scale_pedal_byte(uint8_t raw) {
    return ((float)raw*100.0f)/180.0f;
}

bool arx_led_strip_update_pedal(ArxLedStrip *f,uint8_t raw,uint32_t now_ms) {
    if(!f||!f->enabled||raw<51u) return false;

    float v=arx_led_strip_scale_pedal_byte(raw);
    if(v>24.0f) v=24.0f;
    f->filtered_level=(f->filtered_level*0.9f)+(v*0.1f);
    f->last_pedal_message_ms=now_ms;
    return true;
}

void arx_led_strip_set_gear(ArxLedStrip *f,uint8_t gear) {
    if(f) f->gear=gear;
}

bool arx_led_strip_should_shutdown(const ArxLedStrip *f,uint32_t now_ms) {
    if(!f||!f->enabled||!f->last_pedal_message_ms) return false;
    return (uint32_t)(now_ms-f->last_pedal_message_ms)>f->shutdown_timeout_ms;
}

static void italian(ArxRgb *o) {
    for(unsigned i=0;i<ARX_LED_COUNT;i++){
        if(i<8u||i>=38u) o[i]=(ArxRgb){255,0,0};
        else if((i>=8u&&i<14u)||(i>=32u&&i<38u)) o[i]=(ArxRgb){255,255,200};
        else o[i]=(ArxRgb){0,255,0};
    }
}

static void european(ArxRgb *o) {
    for(unsigned i=0;i<ARX_LED_COUNT;i++){
        o[i]=(i%4u==3u)?(ArxRgb){255,80,0}:(ArxRgb){0,51,153};
    }
}

static uint32_t lcg(uint32_t *s) {
    *s=*s*1103515245u+12345u;
    return *s;
}

static void crazy(ArxLedStrip *f,ArxRgb *o) {
    uint32_t s=f->pattern_seed;
    for(unsigned i=0;i<ARX_LED_COUNT;i++){
        uint32_t a=lcg(&s),b=lcg(&s),c=lcg(&s);
        o[i]=(ArxRgb){(uint8_t)(a%255u),(uint8_t)(b%255u),(uint8_t)(c%64u)};
    }
    f->pattern_seed=s;
}

static uint8_t abs8(int v) {
    return (uint8_t)(v<0?-v:v);
}

static uint8_t fade_channel(uint8_t value,uint8_t brightness) {
    if(brightness>45u) brightness=45u;
    float x=(float)value*tan45[brightness];
    if(x<0.0f) x=0.0f;
    if(x>255.0f) x=255.0f;
    return (uint8_t)x;
}

static void apply_brightness(ArxRgb *p,uint8_t brightness) {
    p->r=fade_channel(p->r,brightness);
    p->g=fade_channel(p->g,brightness);
    p->b=fade_channel(p->b,brightness);
}

static uint8_t volume15(const ArxLedStrip *f) {
    float v=f->filtered_level/1.60f;
    if(v>15.0f) v=15.0f;
    if(v<0.0f) v=0.0f;
    return (uint8_t)(v+0.5f);
}

size_t arx_led_strip_render(ArxLedStrip *f,ArxRgb out[ARX_LED_COUNT],uint32_t now_ms) {
    if(!f||!out||!f->enabled) return 0u;
    if(f->last_render_ms && (uint32_t)(now_ms-f->last_render_ms)<10u) return 0u;

    if(f->gear==2u||f->gear==5u) european(out);
    else if(f->gear==3u||f->gear==6u) crazy(f,out);
    else italian(out);

    const uint8_t volume=volume15(f);

    for(uint8_t i=0u;i<15u;i++){
        uint8_t d=abs8((int)(15u-volume)-(int)i);
        if(d==0u)d=22u;
        else if(d==1u)d=3u;
        else if(d==2u)d=2u;
        else if(d==3u)d=1u;
        else d=0u;

        uint8_t brightness=(i<(uint8_t)(15u-volume))
            ? d
            : (uint8_t)(45u-d);
        apply_brightness(&out[i],brightness);
    }

    /* 15..30 inclusive remain at full intensity. */
    for(uint8_t i=31u;i<46u;i++){
        uint8_t d=abs8((int)volume-(int)i-31);
        if(d==0u)d=22u;
        if(d==1u)d=3u;
        if(d==3u)d=1u;
        if(d!=15u && d!=2u && d!=1u) d=0u;

        uint8_t brightness=(i<(uint8_t)(31u+volume))
            ? (uint8_t)(45u-d)
            : d;
        apply_brightness(&out[i],brightness);
    }

    f->last_render_ms=now_ms;
    return ARX_LED_COUNT;
}

size_t arx_led_strip_encode_ws2812_brg(
    const ArxRgb rgb[ARX_LED_COUNT],
    uint16_t *pwm,
    size_t capacity
) {
    if(!rgb||!pwm||capacity<ARX_WS2812_PWM_WORDS) return 0u;

    size_t pos=0u;
    for(uint8_t i=0u;i<ARX_LED_COUNT;i++){
        /* Physical strip byte order: Blue, Red, Green. */
        uint32_t color=((uint32_t)rgb[i].b<<16u)|
                       ((uint32_t)rgb[i].r<<8u)|
                       (uint32_t)rgb[i].g;

        for(int bit=23;bit>=0;bit--){
            pwm[pos++]=(color&(1u<<(unsigned)bit))
                ? ARX_WS2812_ONE_TICKS
                : ARX_WS2812_ZERO_TICKS;
        }
    }

    for(uint8_t i=0u;i<ARX_WS2812_RESET_SLOTS;i++) pwm[pos++]=0u;
    return pos;
}
