#include "arx/arx_decoder.h"

static ArxDnaMode decode_c1_drive_style(uint8_t raw) {
    switch (raw & 0x7Cu) {
        case 0x00u: return ARX_DNA_NATURAL;
        case 0x08u: return ARX_DNA_DYNAMIC;
        case 0x10u: return ARX_DNA_ALL_WEATHER;
        case 0x30u: return ARX_DNA_RACE;
        default:    return ARX_DNA_UNKNOWN;
    }
}

static ArxDnaMode decode_bh_drive_style(uint8_t raw) {
    switch (raw & 0x1Fu) {
        case 0x00u: return ARX_DNA_NATURAL;
        case 0x02u: return ARX_DNA_DYNAMIC;
        case 0x04u: return ARX_DNA_ALL_WEATHER;
        case 0x0Cu: return ARX_DNA_RACE;
        default:    return ARX_DNA_UNKNOWN;
    }
}

static ArxAccState decode_acc(uint8_t raw) {
    switch (raw & 0x07u) {
        case 0: return ARX_ACC_OFF;
        case 1: return ARX_ACC_ENABLED;
        case 2: return ARX_ACC_ENGAGED;
        case 3: return ARX_ACC_BRAKE_ONLY;
        case 4: return ARX_ACC_OVERRIDE;
        case 5: return ARX_ACC_CANCEL;
        case 6: return ARX_ACC_SUGGESTION_ENGAGED;
        case 7: return ARX_ACC_SUGGESTION_OVERRIDE;
        default:return ARX_ACC_UNKNOWN;
    }
}

void arx_decode_frame(const ArxCanFrame *frame, ArxVehicleState *state) {
    if (!frame || !state) return;

    if (!frame->extended_id && frame->bus == ARX_BUS_C1 &&
        frame->id == 0x384u && frame->dlc >= 2u) {
        state->dna_mode = decode_c1_drive_style(frame->data[1]);
        state->valid_mask |= ARX_VS_DNA;
        return;
    }

    if (!frame->extended_id && frame->bus == ARX_BUS_BH &&
        frame->id == 0x46Cu && frame->dlc >= 8u) {
        state->dna_mode = decode_bh_drive_style(frame->data[7]);
        state->turn_indicator = (uint8_t)((frame->data[6] >> 1u) & 0x03u);
        state->valid_mask |= ARX_VS_DNA | ARX_VS_TURN;
        return;
    }

    if (!frame->extended_id && (frame->bus == ARX_BUS_C1 || frame->bus == ARX_BUS_C2) &&
        frame->id == 0x0FCu && frame->dlc >= 4u) {
        state->engine_rpm = (uint16_t)(
            ((uint16_t)frame->data[0] * 256u +
             (uint16_t)(frame->data[1] & 0xFCu)) / 4u
        );
        state->valid_mask |= ARX_VS_ENGINE_RPM;
        return;
    }

    if (!frame->extended_id && frame->bus == ARX_BUS_C1 &&
        frame->id == 0x0FBu && frame->dlc >= 4u) {
        int32_t raw = ((int32_t)(frame->data[2] & 0x7Fu) << 4) |
                      ((int32_t)frame->data[3] >> 4);
        state->engine_torque_nm = (int16_t)(raw - 500);
        state->valid_mask |= ARX_VS_TORQUE;
        return;
    }

    if (!frame->extended_id && frame->bus == ARX_BUS_C1 &&
        frame->id == 0x101u && frame->dlc >= 3u) {
        uint16_t raw = (uint16_t)(
            (((uint16_t)frame->data[0] << 11) & 0x1FFFu) |
            ((uint16_t)frame->data[1] << 3) |
            ((uint16_t)frame->data[2] >> 5)
        );
        state->vehicle_speed_kmh = (float)raw / 16.0f;
        state->acc_brake_intervention = ((frame->data[0] >> 5u) & 1u) != 0u;
        state->valid_mask |= ARX_VS_SPEED | ARX_VS_BRAKE_CTRL;
        return;
    }

    if (!frame->extended_id && frame->bus == ARX_BUS_C1 &&
        frame->id == 0x226u && frame->dlc >= 3u) {
        state->start_stop_vehicle_enabled =
            (((frame->data[2] >> 2u) & 0x03u) != 0x01u);
        state->valid_mask |= ARX_VS_START_STOP;
        return;
    }

    if (!frame->extended_id && frame->bus == ARX_BUS_C1 &&
        frame->id == 0x2EFu && frame->dlc >= 1u) {
        state->current_gear = (uint8_t)(frame->data[0] >> 4u);
        state->valid_mask |= ARX_VS_GEAR;
        return;
    }

    if (!frame->extended_id && frame->bus == ARX_BUS_BH &&
        frame->id == 0x3E8u && frame->dlc >= 4u) {
        state->current_gear = (uint8_t)(frame->data[3] & 0x0Fu);
        state->valid_mask |= ARX_VS_GEAR;
        return;
    }

    if (!frame->extended_id && frame->bus == ARX_BUS_C1 &&
        frame->id == 0x4B2u && frame->dlc >= 4u) {
        uint16_t oil_pressure_raw =
            (uint16_t)(((frame->data[0] & 0x01u) << 7u) |
                       ((frame->data[1] >> 1u) & 0x7Fu));
        uint16_t oil_temp_raw =
            (uint16_t)(((frame->data[2] & 0x3Fu) << 2u) |
                       ((frame->data[3] >> 6u) & 0x03u));
        state->oil_pressure_bar = (float)oil_pressure_raw * 0.1f;
        state->oil_temperature_c = (float)oil_temp_raw;
        state->valid_mask |= ARX_VS_OIL_PRESSURE | ARX_VS_OIL_TEMP;
        return;
    }

    if (!frame->extended_id && frame->bus == ARX_BUS_C1 &&
        frame->id == 0x41Au && frame->dlc >= 6u) {
        const uint16_t current_raw=(uint16_t)(((uint16_t)frame->data[4] << 4u) |
                                             ((uint16_t)frame->data[5] >> 4u));
        state->battery_soc_percent=(float)(frame->data[1] & 0x7Fu);
        state->battery_current_a=(float)current_raw * 0.1f - 250.0f;
        state->valid_mask |= ARX_VS_BATTERY_SOC | ARX_VS_BATTERY_CURR;
        return;
    }

    if (!frame->extended_id && frame->bus == ARX_BUS_C1 &&
        frame->id == 0x2EDu && frame->dlc >= 7u) {
        state->shift_urgency = (uint8_t)(frame->data[6] & 0x03u);
        state->valid_mask |= ARX_VS_SHIFT;
        return;
    }

    if (!frame->extended_id && frame->bus == ARX_BUS_C1 &&
        frame->id == 0x73Cu && frame->dlc >= 8u) {
        state->acc_state = decode_acc((uint8_t)((frame->data[7] >> 4u) & 0x07u));
        state->valid_mask |= ARX_VS_ACC;
        return;
    }
}
