#include "arx/arx_feature_catalog.h"

static const ArxFeatureDescriptor catalog[] = {
    {"sniffer","CAN Sniffer",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,false},
    {"start_stop","Smart Start/Stop",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"immobilizer","Diagnostic Intrusion Guard",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"led_strip","LED Strip Controller",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"shift","Shift Indicator",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"ipc_profile","IPC Compatibility",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,false},
    {"dashboard_params","Dashboard Parameters",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"route","CAN Message Route Service",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"esc_tc","ESC/TC Drive-Style Control",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"dyno","Dyno Mode",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"acc_virtual_pad","ACC Virtual Pad",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"clear_faults","Clear DTC",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"regen_alert","DPF Regeneration Alert",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"awd","Q4/AWD Control",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"brake_override","Front Brake Override",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"read_faults","Read DTC",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,false},
    {"odometer","Odometer Blink Mask",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"seatbelt","Seat Belt Alarm Configuration",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"pedal_controller","Pedal Controller",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"setup","Runtime Setup",ARX_FEATURE_STABLE_CORE,false},
    {"params_setup","Parameter Visibility Setup",ARX_FEATURE_STABLE_CORE,false},
    {"race_mask","Race Display Mask",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"park_mirror","Park Mirror",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"acc_autostart","ACC Autostart",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"close_windows","Close Windows",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"open_windows","Open Windows",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"has_virtual_pad","HAS Virtual Pad",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"qv_exhaust","Exhaust Flap Control",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"front_park_mute","Front Park Mute",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"elm327","ELM-Compatible Diagnostics",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,false},
    {"low_consume","Low Consumption",ARX_FEATURE_NEEDS_VEHICLE_VALIDATION,true},
    {"remote_start","Remote Start",ARX_FEATURE_EXPERIMENTAL_DISABLED,true}
};

const ArxFeatureDescriptor *arx_feature_catalog(size_t *count) {
    if (count) *count=sizeof(catalog)/sizeof(catalog[0]);
    return catalog;
}
