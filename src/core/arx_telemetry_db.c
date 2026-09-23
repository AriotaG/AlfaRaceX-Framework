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
    {"PWR & TORQUE",       "PWR $3.0fCV $3.0fNm",        "engine_power",            "engine_torque"},
    {"OIL BAR / WATER",    "OIL $1.1fbar W.$3.0fC",      "oil_pressure",            "coolant_temperature"},
    {"OIL BAR / OIL TEMP", "OIL $1.1fbar O.$3.0fC",      "oil_pressure",            "oil_temperature"},
    {"OIL / WATER TEMP",   "OIL $3.0fC W.$3.0fC",        "oil_temperature",         "coolant_temperature"},
    {"OIL LEVEL / QUALITY","OIL $2.1fmm Qu.$3.0f%",      "oil_level",               "oil_quality"},
    {"BAT SOC / CURRENT",  "BAT $3.0f% $3.1fA",          "battery_soc",             "battery_current"},
    {"BAT VOLT / CURRENT", "BAT $2.2fV $3.1fA",          "battery_voltage",         "battery_current"},
    {"DPF LOAD / TEMP",    "DPF $2.2f% $2.2fC",          "dpf_load",                "dpf_temperature"},
    {"REGEN / DPF TEMP",   "REGEN $2.1f% $3.0fC",        "dpf_regen_progress",      "dpf_temperature"},
    {"POWER",              "PWR: $3.2fCV",               "engine_power",            "engine_power"},
    {"TORQUE",             "TORQUE: $3.2fNm",            "engine_torque",           "engine_torque"},
    {"DPF LOAD",           "DPF: $3.2f%",                "dpf_load",                "dpf_load"},
    {"DPF TEMP",           "DPF: $3.2fC",                "dpf_temperature",         "dpf_temperature"},
    {"DPF REGEN",          "DPF REGEN: $3.2f%",          "dpf_regen_progress",      "dpf_regen_progress"},
    {"REGEN TYPE",         "REGEN: $enum",               "dpf_regen_mode",          "dpf_regen_mode"},
    {"LAST REGEN",         "LAST REGEN:$5.0fkm",         "distance_last_regen",     "distance_last_regen"},
    {"TOTAL REGEN",        "TOT REGEN: $5.0f",           "regen_count",             "regen_count"},
    {"MEAN REGEN KM",      "MEAN REGEN:$5.0fkm",         "mean_regen_distance",     "mean_regen_distance"},
    {"MEAN REGEN MIN",     "MEAN REGEN:$3.0fmin",        "mean_regen_duration",     "mean_regen_duration"},
    {"BATTERY V",          "BAT $2.2fV",                 "battery_voltage",         "battery_voltage"},
    {"BATTERY SOC",        "BAT $3.0f%",                 "battery_soc",             "battery_soc"},
    {"BATTERY A",          "BAT $3.1fA",                 "battery_current",         "battery_current"},
    {"OIL QUALITY",        "OIL QUALY: $3.0f%",          "oil_quality",             "oil_quality"},
    {"OIL TEMP",           "OIL: $3.0fC",                "oil_temperature",         "oil_temperature"},
    {"OIL PRESSURE",       "OIL: $2.2fbar",              "oil_pressure",            "oil_pressure"},
    {"OIL LEVEL",          "OIL: $3.2fmm",               "oil_level",               "oil_level"},
    {"ADBLUE L",           "ADBLUE: $3.2fL",             "adblue_liters",           "adblue_liters"},
    {"ADBLUE %",           "ADBLUE: $3.2f%",             "adblue_percent",          "adblue_percent"},
    {"GEARBOX TEMP",       "GEARBOX: $3.2fC",            "gearbox_temperature",     "gearbox_temperature"},
    {"EXHAUST GAS",        "EXHAUST GAS:$4.0fC",         "egt_turbo_in",            "egt_turbo_in"},
    {"CURRENT GEAR",       "CUR. GEAR: $enum",           "gear",                    "gear"},
    {"WATER TEMP",         "WATER: $3.0fC",              "coolant_temperature",     "coolant_temperature"},
    {"EGR COMMAND",        "EGR CMD:$2.2f%",             "egr_command",             "egr_command"},
    {"EGR STATUS",         "EGR: $2.2f%",                "egr_status",              "egr_status"},
    {"TURBO REQ BAR",      "TURBO REQ: $2.1fbar",        "turbo_request_pressure",  "turbo_request_pressure"},
    {"TURBO REQ %",        "TURBO REQ: $2.2f%",          "turbo_request_percent",   "turbo_request_percent"},
    {"TURBO TEMP",         "TURBO: $2.2fC",              "turbo_temperature",       "turbo_temperature"},
    {"TURBO BAR",          "TURBO: $2.2fbar",            "turbo_pressure",          "turbo_pressure"},
    {"TURBO %",            "TURBO: $2.2f%",              "turbo_percent",           "turbo_percent"},
    {"BOOST REQUEST",      "BOOST REQ.:$2.1fbar",        "boost_request",           "boost_request"},
    {"BOOST SENSOR V",     "BOOST: $1.2fV",              "boost_sensor_voltage",    "boost_sensor_voltage"},
    {"RAIL PRESSURE",      "RAIL: $5.2fbar",             "rail_pressure",           "rail_pressure"},
    {"DIESEL TEMP",        "DIESEL: $2.2fC",             "diesel_temperature",      "diesel_temperature"},
    {"ODOMETER LAST",      "ODOM.LAST: $5.0fkm",         "odometer_last",           "odometer_last"},
    {"AIR COND PRESS",     "AIR COND.:$2.2fbar",         "ac_pressure",             "ac_pressure"},
    {"FUEL CONS",          "FUEL CONS.:$1.2fL/h",        "fuel_consumption",        "fuel_consumption"},
    {"DEBIMETER TEMP",     "DEBIMETER:$3.2fC",           "maf_temperature",         "maf_temperature"},
    {"SPEED",              "SPEED:$3.2fkm/h",            "speed",                   "speed"},
    {"SEATBELT ALARM",     "Seatbelt Alarm:$enum",       "seatbelt_alarm",          "seatbelt_alarm"},
    {"0-100 KM/H",         "0-100Km/h: $2.2fs",          "performance_0_100",       "performance_0_100"},
    {"100-200 KM/H",       "100-200Km/h:$2.2fs",         "performance_100_200",     "performance_100_200"},
    {"BEST 0-100",         "Best 0-100:$2.2fs",          "best_0_100",              "best_0_100"},
    {"BEST 100-200",       "Best100-200:$2.2fs",         "best_100_200",            "best_100_200"},
    {"DRIVE STYLE",        "DRIVE STYLE: $enum",         "dna_mode",                "dna_mode"},
    {"PEDAL MAP",          "Pedal Map: $enum",           "pedal_map",               "pedal_map"}
};

static size_t append_text(char out[19],size_t pos,const char *s) {
    while(s&&*s&&pos<18u) out[pos++]=*s++;
    return pos;
}

static uint32_t pow10_u8(uint8_t n) {
    uint32_t v=1u;
    while(n--) v*=10u;
    return v;
}

static size_t append_fixed(
    char out[19],size_t pos,float value,bool valid,uint8_t integer_width,uint8_t decimals
) {
    const size_t field=(size_t)integer_width+(decimals?1u+(size_t)decimals:0u);
    if(!valid){
        for(size_t i=0u;i<field&&pos<18u;i++) out[pos++]=(i<3u)?'-':' ';
        return pos;
    }

    const bool neg=value<0.0f;
    const float a=neg?-value:value;
    const uint32_t factor=pow10_u8(decimals);
    const uint32_t scaled=(uint32_t)(a*(float)factor+0.5f);
    const uint32_t whole=factor?scaled/factor:scaled;
    const uint32_t frac=factor?scaled%factor:0u;

    char digits[12];
    size_t nd=0u;
    uint32_t q=whole;
    do{digits[nd++]=(char)('0'+q%10u);q/=10u;}while(q&&nd<sizeof(digits));

    size_t needed=nd+(neg?1u:0u)+(decimals?1u+(size_t)decimals:0u);
    if(needed>field){
        for(size_t i=0u;i<field&&pos<18u;i++)out[pos++]='#';
        return pos;
    }
    for(size_t i=needed;i<field&&pos<18u;i++)out[pos++]=' ';
    if(neg&&pos<18u)out[pos++]='-';
    while(nd&&pos<18u)out[pos++]=digits[--nd];

    if(decimals&&pos<18u){
        out[pos++]='.';
        uint32_t div=factor/10u;
        for(uint8_t i=0u;i<decimals&&pos<18u;i++){
            out[pos++]=(char)('0'+((frac/div)%10u));
            if(div>1u)div/=10u;
        }
    }
    return pos;
}

static const char *enum_text(const char *key,float value,bool valid,char scratch[2]) {
    if(!valid)return "?";
    unsigned v=value<0.0f?0u:(unsigned)(value+0.5f);
    if(!strcmp(key,"gear")){
        static const char gear[]={'N','1','2','3','4','5','6','R','7','8','9'};
        scratch[0]=v<sizeof(gear)?gear[v]:'-';scratch[1]='\0';return scratch;
    }
    if(!strcmp(key,"dpf_regen_mode")){
        static const char *m[]={"NONE","DPF LO","DPF HI","NSC De-NOx","NSC De-SOx","SCR HeatUp","NONE.","?"};
        return m[v<7u?v:7u];
    }
    if(!strcmp(key,"seatbelt_alarm"))return v==0u?"ON":v==1u?"OFF":"?";
    if(!strcmp(key,"dna_mode")){
        static const char *m[]={"?","N","D","A","R"};
        return m[v<5u?v:0u];
    }
    if(!strcmp(key,"pedal_map")){
        static const char *m[]={"?","Bypass","A","N","D","R"};
        return m[v<6u?v:0u];
    }
    return "?";
}

bool arx_telemetry_format_page(
    const ArxTelemetryPage *page,
    float primary,
    bool primary_valid,
    float secondary,
    bool secondary_valid,
    char out[19]
) {
    if(!page||!page->format||!out)return false;
    memset(out,' ',18u);out[18]='\0';
    const float values[2]={primary,secondary};
    const bool valid[2]={primary_valid,secondary_valid};
    const char *keys[2]={page->primary_key,page->secondary_key};
    unsigned which=0u;
    size_t pos=0u;

    for(const char *p=page->format;*p&&pos<18u;){
        if(*p!='$'){out[pos++]=*p++;continue;}
        if(!strncmp(p,"$enum",5u)){
            char one[2];
            pos=append_text(out,pos,enum_text(keys[which<2u?which:1u],values[which<2u?which:1u],valid[which<2u?which:1u],one));
            if(which<1u)which++;
            p+=5;
            continue;
        }
        if(p[1]>='0'&&p[1]<='9'&&p[2]=='.'&&p[3]>='0'&&p[3]<='9'&&p[4]=='f'){
            const uint8_t iw=(uint8_t)(p[1]-'0');
            const uint8_t dec=(uint8_t)(p[3]-'0');
            const unsigned idx=which<2u?which:1u;
            pos=append_fixed(out,pos,values[idx],valid[idx],iw,dec);
            if(which<1u)which++;
            p+=5;
            continue;
        }
        out[pos++]=*p++;
    }
    return true;
}

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
