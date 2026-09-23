#include "arx/features/arx_park_mirror.h"
#include <string.h>

void arx_park_mirror_init(ArxParkMirror *f) {
    if (!f) return;
    memset(f,0,sizeof(*f));
    f->restore_delay_ms=10000u;
    f->command_period_ms=900u;
    f->inter_command_pause_ms=1800u;
    f->neutral_transient_ms=1200u;
}


void arx_park_mirror_request_capture_park(ArxParkMirror *f) {
    if (!f) return;
    f->capture_park=true;
}

static ArxMirrorPosition decode_pos(const ArxCanFrame *fr) {
    ArxMirrorPosition p={0};
    p.left_h=fr->data[0];
    p.left_v=fr->data[1];
    p.right_h=fr->data[2];
    p.right_v=(uint16_t)(((uint16_t)fr->data[3]<<4u) | (fr->data[4]>>4u));
    return p;
}

bool arx_park_mirror_observe_position(ArxParkMirror *f, const ArxCanFrame *fr) {
    if (!f || !fr || fr->extended_id || fr->bus!=ARX_BUS_BH ||
        fr->id!=0x5A6u || fr->dlc<5u) return false;

    f->source_steady=((fr->data[3]&0x40u)==0u) && ((fr->data[3]&0x10u)==0u);

    if (f->capture_normal && f->source_steady) {
        f->normal=decode_pos(fr);
        f->calibrated_normal=true;
        f->capture_normal=false;
    }

    if (f->capture_park && f->source_steady) {
        f->park=decode_pos(fr);
        f->calibrated_park=true;
        f->capture_park=false;
    }

    if (f->request_restore && f->source_steady && f->calibrated_normal) {
        ArxMirrorPosition p=decode_pos(fr);
        const bool l0=(p.left_h+1u>=f->normal.left_h && p.left_h<=f->normal.left_h+1u);
        const bool l1=(p.left_v+1u>=f->normal.left_v && p.left_v<=f->normal.left_v+1u);
        const bool r0=(p.right_h+1u>=f->normal.right_h && p.right_h<=f->normal.right_h+1u);
        const bool r1=(p.right_v+1u>=f->normal.right_v && p.right_v<=f->normal.right_v+1u);
        if(l0&&l1&&r0&&r1) f->request_restore=false;
    }
    return true;
}

void arx_park_mirror_update(
    ArxParkMirror *f,
    uint8_t gear,
    uint8_t turn_indicator,
    uint16_t rpm,
    uint32_t now_ms
) {
    if (!f || !f->enabled || !f->calibrated_park) return;

    if (gear==0x0Eu) { /* reverse */
        f->exit_reverse_ms=0u;
        f->neutral_entry_ms=0u;

        if (turn_indicator==0x02u) {
            if(!f->request_left_park && !f->request_right_park) {
                if(f->request_restore) f->request_restore=false;
                else f->capture_normal=true;
            }
            if(!f->request_left_park){
                f->request_left_park=true;
                f->left_request_ms=now_ms;
            }
        } else if (turn_indicator==0x01u) {
            if(!f->request_left_park && !f->request_right_park) {
                if(f->request_restore) f->request_restore=false;
                else f->capture_normal=true;
            }
            if(!f->request_right_park){
                f->request_right_park=true;
                f->right_request_ms=now_ms;
            }
        }
        return;
    }

    if (gear==0x00u) { /* neutral transient */
        if(!f->neutral_entry_ms) f->neutral_entry_ms=now_ms;
        if(now_ms-f->neutral_entry_ms < f->neutral_transient_ms) f->exit_reverse_ms=0u;
        return;
    }

    f->neutral_entry_ms=0u;

    if ((f->request_left_park || f->request_right_park) && f->exit_reverse_ms==0u) {
        f->exit_reverse_ms=now_ms;

        /* P or stopped engine: request immediate restore without unsigned-underflow tricks. */
        if(gear==0x0Du || rpm<=400u) {
            f->exit_reverse_ms = (now_ms > f->restore_delay_ms)
                ? now_ms-f->restore_delay_ms-1u : 1u;
        }
    }

    if (f->exit_reverse_ms &&
        now_ms-f->exit_reverse_ms > f->restore_delay_ms) {
        if(f->request_left_park || f->request_right_park) {
            f->request_restore=true;
            f->restore_request_ms=now_ms;
        }
        f->request_left_park=false;
        f->request_right_park=false;
        f->exit_reverse_ms=0u;
    }
}

bool arx_park_mirror_build_command(ArxParkMirror *f, uint32_t now_ms, ArxCanFrame *out) {
    if(!f || !out || !f->enabled || !f->calibrated_normal) return false;
    if(!(f->request_left_park || f->request_right_park || f->request_restore)) return false;
    if(f->capture_normal || !f->source_steady) return false;
    if(f->last_command_ms && now_ms-f->last_command_ms <= f->command_period_ms) return false;

    if(f->request_restore &&
       now_ms-f->restore_request_ms < f->inter_command_pause_ms) return false;

    if((f->request_left_park &&
        now_ms-f->left_request_ms < f->inter_command_pause_ms) ||
       (f->request_right_park &&
        now_ms-f->right_request_ms < f->inter_command_pause_ms)) return false;

    ArxMirrorPosition p=f->normal;
    if(f->request_left_park){ p.left_h=f->park.left_h; p.left_v=f->park.left_v; }
    if(f->request_right_park){ p.right_h=f->park.right_h; p.right_v=f->park.right_v; }

    memset(out,0,sizeof(*out));
    out->bus=ARX_BUS_BH;
    out->id=0x5A8u;
    out->extended_id=false;
    out->dlc=8u;
    out->timestamp_ms=now_ms;

    out->data[0]=p.left_h;
    out->data[1]=p.left_v;
    out->data[2]=p.right_h;
    out->data[3]=(uint8_t)(p.right_v>>4u);
    out->data[4]=(uint8_t)((p.right_v&0x0Fu)<<4u);
    out->data[4] |= 0x80u;

    f->last_command_ms=now_ms;
    return true;
}
