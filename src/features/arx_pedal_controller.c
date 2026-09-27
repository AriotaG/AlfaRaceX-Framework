#include "arx/features/arx_pedal_controller.h"
#include "arx/arx_crc.h"
#include <string.h>

static uint8_t map_token(ArxPedalMap map) {
    switch (map) {
        case ARX_PEDAL_MAP_ALL_WEATHER: return 0x49u;
        case ARX_PEDAL_MAP_NATURAL:     return 0x92u;
        case ARX_PEDAL_MAP_DYNAMIC:     return 0xDBu;
        case ARX_PEDAL_MAP_RACE:        return 0x24u;
        case ARX_PEDAL_MAP_BYPASS:
        default:                        return 0x00u;
    }
}

static int8_t adapted_power(ArxPedalMap map, int8_t power) {
    if (power > 10) power = 10;
    if (power < -10) power = -10;

    float factor_pos = 1.0f;
    float factor_neg = 1.0f;

    switch (map) {
        case ARX_PEDAL_MAP_ALL_WEATHER:
            factor_pos = 1.6f;
            factor_neg = 0.6f;
            break;
        case ARX_PEDAL_MAP_NATURAL:
            factor_pos = 3.2f;
            factor_neg = 1.6f;
            break;
        case ARX_PEDAL_MAP_DYNAMIC:
            factor_pos = 2.0f;
            factor_neg = 3.2f;
            break;
        case ARX_PEDAL_MAP_RACE:
            factor_pos = 2.4f;
            factor_neg = 1.8f;
            break;
        default:
            return 0;
    }

    if (power >= 0) {
        return (int8_t)((float)power * factor_pos + 0.5f);
    }
    return (int8_t)((float)power * factor_neg - 0.5f);
}

void arx_pedal_init(ArxPedalController *f) {
    if (!f) return;
    memset(f, 0, sizeof(*f));
    f->mode = ARX_PEDAL_DISABLED;
    f->applied_map = ARX_PEDAL_MAP_UNKNOWN;
    f->retry_interval_ms = 400u;
}

bool arx_pedal_set_mode(ArxPedalController *f, ArxPedalMode mode) {
    if (!f || (unsigned)mode > ARX_PEDAL_KIDS_LIMITER) return false;
    if(f->mode!=mode){
        f->disable_pending=mode==ARX_PEDAL_DISABLED;
        f->waiting_reply=false;
        f->applied_map=ARX_PEDAL_MAP_UNKNOWN;
        f->communication=ARX_PEDAL_COMM_UNKNOWN;
    }
    f->mode = mode;
    return true;
}

bool arx_pedal_set_power(ArxPedalController *f, int8_t power) {
    if (!f || power < -10 || power > 10) return false;
    if(f->power!=power){
        f->applied_map=ARX_PEDAL_MAP_UNKNOWN;
        f->waiting_reply=false;
        f->communication=ARX_PEDAL_COMM_UNKNOWN;
    }
    f->power = power;
    return true;
}

ArxPedalMap arx_pedal_target_map(const ArxPedalController *f, ArxDnaMode dna_mode) {
    if (!f) return ARX_PEDAL_MAP_BYPASS;

    switch (f->mode) {
        case ARX_PEDAL_AUTO:
            switch (dna_mode) {
                case ARX_DNA_ALL_WEATHER: return ARX_PEDAL_MAP_ALL_WEATHER;
                case ARX_DNA_NATURAL:     return ARX_PEDAL_MAP_NATURAL;
                case ARX_DNA_DYNAMIC:     return ARX_PEDAL_MAP_DYNAMIC;
                case ARX_DNA_RACE:        return ARX_PEDAL_MAP_RACE;
                default:                  return ARX_PEDAL_MAP_BYPASS;
            }

        case ARX_PEDAL_BYPASS:
            return ARX_PEDAL_MAP_BYPASS;

        case ARX_PEDAL_ALL_WEATHER:
            return ARX_PEDAL_MAP_ALL_WEATHER;

        case ARX_PEDAL_NATURAL:
            return ARX_PEDAL_MAP_NATURAL;

        case ARX_PEDAL_DYNAMIC:
            return ARX_PEDAL_MAP_DYNAMIC;

        case ARX_PEDAL_RACE:
            return ARX_PEDAL_MAP_RACE;

        case ARX_PEDAL_HYBRID_ALIGN:
            return (dna_mode == ARX_DNA_RACE)
                ? ARX_PEDAL_MAP_RACE
                : ARX_PEDAL_MAP_NATURAL;

        case ARX_PEDAL_KIDS_LIMITER:
            return ARX_PEDAL_MAP_ALL_WEATHER;

        case ARX_PEDAL_DISABLED:
        default:
            return ARX_PEDAL_MAP_BYPASS;
    }
}

ArxPedalMap arx_pedal_parse_reply(uint8_t reply_byte) {
    switch (reply_byte) {
        case 0x10u: return ARX_PEDAL_MAP_BYPASS;
        case 0x59u: return ARX_PEDAL_MAP_ALL_WEATHER;
        case 0xA2u: return ARX_PEDAL_MAP_NATURAL;
        case 0xEBu: return ARX_PEDAL_MAP_DYNAMIC;
        case 0x34u: return ARX_PEDAL_MAP_RACE;
        default:    return ARX_PEDAL_MAP_UNKNOWN;
    }
}

void arx_pedal_on_reply(ArxPedalController *f, uint8_t reply_byte) {
    if (!f) return;
    f->applied_map = arx_pedal_parse_reply(reply_byte);
    f->reply_count++;
}

void arx_pedal_on_reply_at(ArxPedalController *f,uint8_t reply,uint32_t now_ms) {
    if(!f)return;
    const bool timely=f->waiting_reply && now_ms-f->last_tx_ms<=ARX_PEDAL_REPLY_TIMEOUT_MS;
    arx_pedal_on_reply(f,reply);
    f->last_reply_ms=now_ms;
    if(!timely){
        f->applied_map=ARX_PEDAL_MAP_UNKNOWN;
        f->communication=f->waiting_reply?ARX_PEDAL_COMM_TIMEOUT:ARX_PEDAL_COMM_UNKNOWN;
        f->waiting_reply=false;f->error_count++;
        return;
    }
    f->waiting_reply=false;
    if(f->applied_map==ARX_PEDAL_MAP_UNKNOWN){
        f->communication=ARX_PEDAL_COMM_INVALID_REPLY;f->error_count++;
    }else if(f->requested_map!=ARX_PEDAL_MAP_UNKNOWN && f->applied_map!=f->requested_map){
        f->communication=ARX_PEDAL_COMM_MAP_MISMATCH;f->error_count++;
    }else{
        f->communication=ARX_PEDAL_COMM_MAP_CONFIRMED;
        if(f->disable_pending&&f->applied_map==ARX_PEDAL_MAP_BYPASS)f->disable_pending=false;
    }
}

void arx_pedal_note_send(ArxPedalController *f,bool success,uint32_t now_ms) {
    if(!f)return;
    f->last_tx_ms=now_ms;f->attempted=true;
    f->waiting_reply=success;
    f->communication=success?ARX_PEDAL_COMM_WAITING:ARX_PEDAL_COMM_TX_ERROR;
    if(success)f->tx_count++;else f->error_count++;
}

void arx_pedal_tick(ArxPedalController *f,bool engine_running,uint32_t now_ms) {
    if(!f)return;
    if(f->waiting_reply&&now_ms-f->last_tx_ms>ARX_PEDAL_REPLY_TIMEOUT_MS){
        f->waiting_reply=false;f->communication=ARX_PEDAL_COMM_TIMEOUT;f->error_count++;
        f->applied_map=ARX_PEDAL_MAP_UNKNOWN;
    }
    if(!engine_running&&f->mode!=ARX_PEDAL_DISABLED)f->applied_map=ARX_PEDAL_MAP_UNKNOWN;
}

bool arx_pedal_needs_sync(
    const ArxPedalController *f,
    ArxDnaMode dna_mode,
    bool engine_running,
    uint32_t now_ms
) {
    if (!f || f->waiting_reply || (!engine_running&&!f->disable_pending) ||
        (f->mode == ARX_PEDAL_DISABLED&&!f->disable_pending)) return false;

    const ArxPedalMap target = arx_pedal_target_map(f, dna_mode);
    if (target == f->applied_map) return false;

    return !f->attempted || ((now_ms - f->last_tx_ms) > f->retry_interval_ms);
}

bool arx_pedal_build_map_packet(
    const ArxPedalController *f,
    ArxPedalMap map,
    uint8_t out[ARX_PEDAL_PACKET_SIZE]
) {
    if (!f || !out || map<ARX_PEDAL_MAP_BYPASS || map>ARX_PEDAL_MAP_RACE) return false;

    memset(out, 0, ARX_PEDAL_PACKET_SIZE);
    out[0] = '#';
    out[1] = 0xB6u;
    out[2] = map_token(map);

    switch (map) {
        case ARX_PEDAL_MAP_ALL_WEATHER:
            out[3] = (uint8_t)(96 + adapted_power(map, f->power));
            out[4] = 128u;
            out[5] = 154u;
            break;

        case ARX_PEDAL_MAP_NATURAL:
            out[3] = (uint8_t)(128 + adapted_power(map, f->power));
            out[4] = 128u;
            out[5] = 154u;
            break;

        case ARX_PEDAL_MAP_DYNAMIC:
            out[3] = (uint8_t)(192 + adapted_power(map, f->power));
            out[4] = 128u;
            out[5] = 154u;
            break;

        case ARX_PEDAL_MAP_RACE:
            out[3] = (uint8_t)(230 + adapted_power(map, f->power));
            out[4] = 128u;
            out[5] = 154u;
            break;

        case ARX_PEDAL_MAP_BYPASS:
        default:
            break;
    }

    out[8] = arx_crc8_sae_j1850(out, 8u);
    return true;
}

bool arx_pedal_build_sync_packet(
    ArxPedalController *f,
    ArxDnaMode dna_mode,
    bool engine_running,
    uint32_t now_ms,
    uint8_t out[ARX_PEDAL_PACKET_SIZE]
) {
    if (!f || !arx_pedal_needs_sync(f, dna_mode, engine_running, now_ms)) {
        return false;
    }

    ArxPedalMap target = arx_pedal_target_map(f, dna_mode);

    ArxPedalController temp = *f;
    if (f->mode == ARX_PEDAL_KIDS_LIMITER) {
        temp.power = -10;
    }

    if (!arx_pedal_build_map_packet(&temp, target, out)) return false;

    f->requested_map=target;
    f->mismatch_count++;
    return true;
}

bool arx_pedal_prepare_packet(ArxPedalController *f,ArxDnaMode dna,
    bool engine_running,bool diesel,uint16_t rpm,float speed,uint32_t now_ms,
    uint8_t out[ARX_PEDAL_PACKET_SIZE]) {
    if(!f||!out)return false;
    arx_pedal_tick(f,engine_running,now_ms);
    if(f->waiting_reply || (f->attempted&&now_ms-f->last_tx_ms<=f->retry_interval_ms))return false;
    /* Upstream ensures the minimum-power A map first, independently of limiting. */
    if(arx_pedal_build_sync_packet(f,dna,engine_running,now_ms,out))return true;
    if(arx_pedal_kids_override_required(f,diesel,rpm,speed)){
        f->requested_map=ARX_PEDAL_MAP_ALL_WEATHER;
        return arx_pedal_build_zero_override(f,out);
    }
    return false;
}

bool arx_pedal_kids_override_required(
    const ArxPedalController *f,
    bool diesel,
    uint16_t rpm,
    float speed_kmh
) {
    if (!f || f->mode != ARX_PEDAL_KIDS_LIMITER || rpm <= 400u) return false;

    const uint16_t rpm_limit = diesel ? 3000u : 4000u;
    return rpm > rpm_limit || speed_kmh > 100.0f;
}

bool arx_pedal_build_zero_override(
    const ArxPedalController *f,
    uint8_t out[ARX_PEDAL_PACKET_SIZE]
) {
    if (!f || !out || f->mode != ARX_PEDAL_KIDS_LIMITER) return false;

    memset(out, 0, ARX_PEDAL_PACKET_SIZE);
    out[0] = '#';
    out[1] = 0xFFu;
    out[2] = 0x00u;
    out[8] = arx_crc8_sae_j1850(out, 8u);
    return true;
}
