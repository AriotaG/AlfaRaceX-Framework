#ifndef ARX_FAULTS_H
#define ARX_FAULTS_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_FAULT_MAX_DTC 20u
#define ARX_FAULT_RX_BUFFER 90u

typedef enum {
    ARX_FAULTS_IDLE = 0,
    ARX_FAULTS_WAIT_SESSION,
    ARX_FAULTS_WAIT_READ,
    ARX_FAULTS_WAIT_CF,
    ARX_FAULTS_COMPLETE,
    ARX_FAULTS_ERROR
} ArxFaultState;

typedef struct {
    uint8_t bytes[3];
    uint8_t status;
} ArxDtcRecord;

typedef struct {
    ArxFaultState state;
    uint32_t request_id;
    uint32_t response_id;
    uint32_t state_since_ms;
    uint32_t timeout_ms;

    uint8_t rx[ARX_FAULT_RX_BUFFER];
    uint16_t expected;
    uint16_t received;
    uint8_t next_sn;

    ArxDtcRecord dtc[ARX_FAULT_MAX_DTC];
    uint8_t dtc_count;
} ArxFaultManager;

void arx_faults_init(ArxFaultManager *f, uint32_t request_id, uint32_t response_id);
bool arx_faults_start_read(ArxFaultManager *f, uint32_t now_ms, ArxCanFrame *out);
bool arx_faults_on_response(ArxFaultManager *f, const ArxCanFrame *frame, uint32_t now_ms, ArxCanFrame *next);
bool arx_faults_build_clear(uint8_t ecu_address, ArxCanFrame *out);
void arx_faults_tick(ArxFaultManager *f, uint32_t now_ms);

#endif
