#ifndef ARX_VEHICLE_STATE_H
#define ARX_VEHICLE_STATE_H

#include "arx/arx_types.h"

typedef enum {
    ARX_DNA_UNKNOWN = 0,
    ARX_DNA_NATURAL,
    ARX_DNA_DYNAMIC,
    ARX_DNA_ALL_WEATHER,
    ARX_DNA_RACE
} ArxDnaMode;

typedef enum {
    ARX_ACC_UNKNOWN = 0,
    ARX_ACC_OFF,
    ARX_ACC_ENABLED,
    ARX_ACC_ENGAGED,
    ARX_ACC_BRAKE_ONLY,
    ARX_ACC_OVERRIDE,
    ARX_ACC_CANCEL,
    ARX_ACC_SUGGESTION_ENGAGED,
    ARX_ACC_SUGGESTION_OVERRIDE
} ArxAccState;

enum {
    ARX_VS_OIL_PRESSURE = 1u << 0,
    ARX_VS_OIL_TEMP     = 1u << 1,
    ARX_VS_COOLANT_TEMP = 1u << 2,
    ARX_VS_ENGINE_RPM   = 1u << 3,
    ARX_VS_TORQUE       = 1u << 4,
    ARX_VS_GEAR         = 1u << 5,
    ARX_VS_DNA          = 1u << 6,
    ARX_VS_SHIFT        = 1u << 7,
    ARX_VS_SPEED        = 1u << 8,
    ARX_VS_ACC          = 1u << 9,
    ARX_VS_START_STOP   = 1u << 10,
    ARX_VS_BRAKE_CTRL   = 1u << 11,
    ARX_VS_TURN         = 1u << 12
};

typedef struct {
    uint32_t valid_mask;

    float oil_pressure_bar;
    float oil_temperature_c;
    float coolant_temperature_c;
    float gearbox_temperature_c;
    float vehicle_speed_kmh;

    int16_t engine_torque_nm;
    uint16_t engine_rpm;

    uint8_t current_gear;
    uint8_t shift_urgency;
    uint8_t turn_indicator; /* 0=center, 1=right, 2=left */

    bool start_stop_vehicle_enabled;
    bool acc_brake_intervention;

    float dpf_load_percent;
    float dpf_temperature_c;
    float dpf_regeneration_percent;

    ArxDnaMode dna_mode;
    ArxAccState acc_state;
} ArxVehicleState;

void arx_vehicle_state_init(ArxVehicleState *state);
bool arx_vehicle_engine_running(const ArxVehicleState *state);

#endif
