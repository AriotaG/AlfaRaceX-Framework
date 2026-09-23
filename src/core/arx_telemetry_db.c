#include "arx/arx_telemetry_db.h"
#include <string.h>

#define NATIVE(KEY,SCALE,OFF,UNIT,DIGITS) \
    {.key=(KEY),.source=ARX_SIGNAL_NATIVE,.scale=(SCALE),.offset=(OFF),.unit=(UNIT),.decimal_digits=(DIGITS)}

#define UDSX(KEY,REQ,RESP,DID,LEN,ROFF,RAWOFF,SCALE,ENGOFF,UNIT,DIGITS) \
    {.key=(KEY),.source=ARX_SIGNAL_UDS,.request_id=(REQ),.response_id=(RESP), \
     .request={0x03,0x22,(uint8_t)(((DID)>>8)&0xFFu),(uint8_t)((DID)&0xFFu)},.request_len=4, \
     .reply_len=(LEN),.reply_offset=(ROFF),.signed_value=false,.scale=(SCALE),.offset=(ENGOFF),.unit=(UNIT), \
     .raw_offset=(RAWOFF),.decimal_digits=(DIGITS)}

#define UDS1(KEY,REQ,RESP,DID,SCALE,OFF,UNIT,DIGITS) UDSX(KEY,REQ,RESP,DID,1,0,0,SCALE,OFF,UNIT,DIGITS)
#define UDS2(KEY,REQ,RESP,DID,SCALE,OFF,UNIT,DIGITS) UDSX(KEY,REQ,RESP,DID,2,0,0,SCALE,OFF,UNIT,DIGITS)
#define UDS3(KEY,REQ,RESP,DID,SCALE,OFF,UNIT,DIGITS) UDSX(KEY,REQ,RESP,DID,3,0,0,SCALE,OFF,UNIT,DIGITS)

/*
 * Diesel database used by the dashboard/diagnostic scheduler.
 * Native signals are decoded elsewhere; UDS definitions preserve the exact DID,
 * byte length and scaling used by the deployed vehicle profile.
 */
static const ArxTelemetryDefinition diesel[] = {
    NATIVE("oil_pressure",0.1f,0.0f,"bar",1),
    NATIVE("engine_power",1.0f,0.0f,"CV",0),
    NATIVE("engine_torque",1.0f,0.0f,"Nm",0),
    NATIVE("oil_temperature",1.0f,0.0f,"C",0),
    NATIVE("gear",1.0f,0.0f,"",0),
    NATIVE("speed",1.0f,0.0f,"km/h",2),
    NATIVE("dpf_regen_mode",1.0f,0.0f,"",0),
    NATIVE("battery_current",0.1f,-250.0f,"A",1),
    NATIVE("seatbelt_alarm",1.0f,0.0f,"",0),
    NATIVE("performance_0_100",1.0f,0.0f,"s",2),
    NATIVE("performance_100_200",1.0f,0.0f,"s",2),
    NATIVE("best_0_100",1.0f,0.0f,"s",2),
    NATIVE("best_100_200",1.0f,0.0f,"s",2),
    NATIVE("dna_mode",1.0f,0.0f,"",0),
    NATIVE("pedal_map",1.0f,0.0f,"",0),

    UDS2("dpf_load",0x18DA10F1u,0x18DAF110u,0x18E4u,0.015259022f,0.0f,"%",1),
    UDS2("dpf_temperature",0x18DA10F1u,0x18DAF110u,0x18DEu,0.02f,-40.0f,"C",1),
    UDS2("dpf_regen_progress",0x18DA10F1u,0x18DAF110u,0x380Bu,0.001525902f,0.0f,"%",1),
    UDS3("distance_last_regen",0x18DA10F1u,0x18DAF110u,0x3807u,0.1f,0.0f,"km",0),
    UDS2("regen_count",0x18DA10F1u,0x18DAF110u,0x18A4u,1.0f,0.0f,"",0),
    UDS2("mean_regen_distance",0x18DA10F1u,0x18DAF110u,0x3809u,1.0f,0.0f,"km",0),
    UDS2("mean_regen_duration",0x18DA10F1u,0x18DAF110u,0x380Au,0.01666666666f,0.0f,"min",0),
    UDS2("battery_voltage",0x18DA10F1u,0x18DAF110u,0x1955u,0.0005f,0.0f,"V",3),
    UDS1("battery_soc",0x18DA10F1u,0x18DAF110u,0x19BDu,1.0f,0.0f,"%",1),
    UDS2("oil_quality",0x18DA10F1u,0x18DAF110u,0x3813u,0.0015259022f,0.0f,"%",1),
    UDS2("oil_level",0x18DA10F1u,0x18DAF110u,0x194Eu,0.1f,0.0f,"mm",1),
    UDS2("adblue_liters",0x18DA01F1u,0x18DAF101u,0xD930u,0.00097676774f,0.0f,"L",2),
    UDS1("adblue_percent",0x18DA01F1u,0x18DAF101u,0xD97Cu,0.390625f,0.0f,"%",2),
    UDS2("egt_turbo_in",0x18DA10F1u,0x18DAF110u,0x3836u,0.02f,-40.0f,"C",1),
    UDS2("coolant_temperature",0x18DA10F1u,0x18DAF110u,0x1003u,0.02f,-40.0f,"C",1),
    UDS1("gearbox_temperature",0x18DA18F1u,0x18DAF118u,0x04FEu,1.0f,-40.0f,"C",0),

    UDSX("tire_temp_front_left", 0x18DAC7F1u,0x18DAF1C7u,0x40B1u,1,4,-50,1.0f,0.0f,"C",0),
    UDSX("tire_temp_front_right",0x18DAC7F1u,0x18DAF1C7u,0x40B2u,1,4,-50,1.0f,0.0f,"C",0),
    UDSX("tire_temp_rear_left",  0x18DAC7F1u,0x18DAF1C7u,0x40B3u,1,4,-50,1.0f,0.0f,"C",0),
    UDSX("tire_temp_rear_right", 0x18DAC7F1u,0x18DAF1C7u,0x40B4u,1,4,-50,1.0f,0.0f,"C",0),

    UDSX("egr_command",0x18DA10F1u,0x18DAF110u,0x189Bu,2,0,-32767,0.00305185095f,0.0f,"%",1),
    UDS1("egr_status",0x18DA10F1u,0x18DAF110u,0x189Au,0.1953125f,0.0f,"%",1),
    UDSX("egr_measured",0x18DA10F1u,0x18DAF110u,0x189Cu,2,0,-32767,0.00305185095f,0.0f,"%",1),
    UDS2("turbo_request_pressure",0x18DA10F1u,0x18DAF110u,0x1942u,0.000030517578f,0.0f,"bar",1),
    UDS2("turbo_request_percent",0x18DA10F1u,0x18DAF110u,0x189Fu,0.00152590219f,0.0f,"%",1),
    UDS2("turbo_temperature",0x18DA10F1u,0x18DAF110u,0x1935u,0.02f,-40.0f,"C",1),
    UDSX("turbo_pressure",0x18DA10F1u,0x18DAF110u,0x195Au,2,0,-32768,0.001f,-1.0f,"bar",2),
    UDS2("turbo_percent",0x18DA10F1u,0x18DAF110u,0x18A0u,0.00152590219f,0.0f,"%",1),
    UDSX("boost_request",0x18DA10F1u,0x18DAF110u,0x1959u,2,0,-32768,0.001f,-1.0f,"bar",1),
    UDS2("boost_sensor_voltage",0x18DA10F1u,0x18DAF110u,0x195Bu,0.0001f,0.0f,"V",2),
    UDS2("rail_pressure",0x18DA10F1u,0x18DAF110u,0x1947u,0.05f,0.0f,"bar",1),
    UDS2("diesel_temperature",0x18DA10F1u,0x18DAF110u,0x1900u,0.02f,-40.0f,"C",1),
    UDS3("odometer_last",0x18DA10F1u,0x18DAF110u,0x2002u,0.1f,0.0f,"km",0),
    UDS2("ac_pressure",0x18DA10F1u,0x18DAF110u,0x192Fu,0.01f,0.0f,"bar",1),
    UDS2("fuel_consumption",0x18DA10F1u,0x18DAF110u,0x1942u,0.0004f,0.0f,"L/h",3),
    UDS2("maf_temperature",0x18DA10F1u,0x18DAF110u,0x193Fu,0.02f,-40.0f,"C",1)
};


static const ArxTelemetryPage diesel_pages[ARX_DIESEL_DASHBOARD_PAGE_COUNT] = {
    {"PWR & TORQUE",       "engine_power",            "engine_torque"},
    {"OIL BAR / WATER",    "oil_pressure",            "coolant_temperature"},
    {"OIL BAR / OIL TEMP", "oil_pressure",            "oil_temperature"},
    {"OIL / WATER TEMP",   "oil_temperature",         "coolant_temperature"},
    {"OIL LEVEL / QUALITY","oil_level",               "oil_quality"},
    {"BAT SOC / CURRENT",  "battery_soc",             "battery_current"},
    {"BAT VOLT / CURRENT", "battery_voltage",         "battery_current"},
    {"DPF LOAD / TEMP",    "dpf_load",                "dpf_temperature"},
    {"REGEN / DPF TEMP",   "dpf_regen_progress",      "dpf_temperature"},
    {"POWER",              "engine_power",            "engine_power"},
    {"TORQUE",             "engine_torque",           "engine_torque"},
    {"DPF LOAD",           "dpf_load",                "dpf_load"},
    {"DPF TEMP",           "dpf_temperature",         "dpf_temperature"},
    {"DPF REGEN",          "dpf_regen_progress",      "dpf_regen_progress"},
    {"REGEN TYPE",         "dpf_regen_mode",          "dpf_regen_mode"},
    {"LAST REGEN",         "distance_last_regen",     "distance_last_regen"},
    {"TOTAL REGEN",        "regen_count",             "regen_count"},
    {"MEAN REGEN KM",      "mean_regen_distance",     "mean_regen_distance"},
    {"MEAN REGEN MIN",     "mean_regen_duration",     "mean_regen_duration"},
    {"BATTERY V",          "battery_voltage",         "battery_voltage"},
    {"BATTERY SOC",        "battery_soc",             "battery_soc"},
    {"BATTERY A",          "battery_current",         "battery_current"},
    {"OIL QUALITY",        "oil_quality",             "oil_quality"},
    {"OIL TEMP",           "oil_temperature",         "oil_temperature"},
    {"OIL PRESSURE",       "oil_pressure",            "oil_pressure"},
    {"OIL LEVEL",          "oil_level",               "oil_level"},
    {"ADBLUE L",           "adblue_liters",           "adblue_liters"},
    {"ADBLUE %",           "adblue_percent",          "adblue_percent"},
    {"GEARBOX TEMP",       "gearbox_temperature",     "gearbox_temperature"},
    {"EXHAUST GAS",        "egt_turbo_in",            "egt_turbo_in"},
    {"CURRENT GEAR",       "gear",                    "gear"},
    {"WATER TEMP",         "coolant_temperature",     "coolant_temperature"},
    {"EGR COMMAND",        "egr_command",             "egr_command"},
    {"EGR STATUS",         "egr_status",              "egr_status"},
    {"TURBO REQ BAR",      "turbo_request_pressure",  "turbo_request_pressure"},
    {"TURBO REQ %",        "turbo_request_percent",   "turbo_request_percent"},
    {"TURBO TEMP",         "turbo_temperature",       "turbo_temperature"},
    {"TURBO BAR",          "turbo_pressure",          "turbo_pressure"},
    {"TURBO %",            "turbo_percent",           "turbo_percent"},
    {"BOOST REQUEST",      "boost_request",           "boost_request"},
    {"BOOST SENSOR V",     "boost_sensor_voltage",    "boost_sensor_voltage"},
    {"RAIL PRESSURE",      "rail_pressure",           "rail_pressure"},
    {"DIESEL TEMP",        "diesel_temperature",      "diesel_temperature"},
    {"ODOMETER LAST",      "odometer_last",           "odometer_last"},
    {"AIR COND PRESS",     "ac_pressure",             "ac_pressure"},
    {"FUEL CONS",          "fuel_consumption",        "fuel_consumption"},
    {"DEBIMETER TEMP",     "maf_temperature",         "maf_temperature"},
    {"SPEED",              "speed",                   "speed"},
    {"SEATBELT ALARM",     "seatbelt_alarm",          "seatbelt_alarm"},
    {"0-100 KM/H",         "performance_0_100",       "performance_0_100"},
    {"100-200 KM/H",       "performance_100_200",     "performance_100_200"},
    {"BEST 0-100",         "best_0_100",              "best_0_100"},
    {"BEST 100-200",       "best_100_200",            "best_100_200"},
    {"DRIVE STYLE",        "dna_mode",                "dna_mode"},
    {"PEDAL MAP",          "pedal_map",               "pedal_map"}
};

const ArxTelemetryDefinition *arx_telemetry_diesel(size_t *count) {
    if (count) *count=sizeof(diesel)/sizeof(diesel[0]);
    return diesel;
}

const ArxTelemetryPage *arx_telemetry_diesel_pages(size_t *count) {
    if (count) *count=sizeof(diesel_pages)/sizeof(diesel_pages[0]);
    return diesel_pages;
}

const ArxTelemetryDefinition *arx_telemetry_find(
    const ArxTelemetryDefinition *db,size_t count,const char *key
) {
    if (!db || !key) return NULL;
    for (size_t i=0;i<count;i++) if (!strcmp(db[i].key,key)) return &db[i];
    return NULL;
}

bool arx_telemetry_build_request(
    const ArxTelemetryDefinition *def,
    ArxBus bus,
    uint32_t now_ms,
    ArxCanFrame *out
) {
    if(!def||!out||def->source!=ARX_SIGNAL_UDS||
       def->request_len==0u||def->request_len>8u)return false;
    memset(out,0,sizeof(*out));
    out->bus=bus;
    out->id=def->request_id;
    out->extended_id=def->request_id>0x7FFu;
    out->dlc=def->request_len;
    out->timestamp_ms=now_ms;
    memcpy(out->data,def->request,def->request_len);
    return true;
}

bool arx_telemetry_decode_response(
    const ArxTelemetryDefinition *def,
    const ArxCanFrame *frame,
    float *value
) {
    if(!def||!frame||!value||def->source!=ARX_SIGNAL_UDS)return false;
    if(frame->id!=def->response_id||frame->dlc<4u)return false;
    if(frame->data[1]!=0x62u)return false;
    if(frame->data[2]!=def->request[2]||frame->data[3]!=def->request[3])return false;
    if(def->reply_len==0u||def->reply_len>4u)return false;

    const uint8_t start=(uint8_t)(4u+def->reply_offset);
    if((uint16_t)start+def->reply_len>frame->dlc)return false;

    uint32_t raw=0u;
    for(uint8_t i=0u;i<def->reply_len;i++)
        raw=(raw<<8u)|frame->data[start+i];

    /* The compatibility database uses arithmetic raw offsets, including values
       like -32768, rather than signed reinterpretation. */
    const int64_t adjusted=(int64_t)raw+(int64_t)def->raw_offset;
    *value=(float)adjusted*def->scale+def->offset;
    return true;
}
