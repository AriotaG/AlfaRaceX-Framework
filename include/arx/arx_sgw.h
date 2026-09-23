#ifndef ARX_SGW_H
#define ARX_SGW_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_SGW_UNKNOWN = 0,
    ARX_SGW_NOT_PRESENT,
    ARX_SGW_PRESENT_LOCKED,
    ARX_SGW_PRESENT_AUTHORIZED,
    ARX_DIRECT_NETWORK_ACCESS
} ArxSgwState;

typedef struct {
    bool c1_rx;
    bool c1_tx;
    bool c2_rx;
    bool c2_tx;
    bool bh_rx;
    bool bh_tx;

    bool uds_read;
    bool uds_write;
    bool security_access;
    bool authentication;
    bool routine_control;
    bool dtc_management;
} ArxCapabilities;

typedef struct {
    ArxSgwState state;
    ArxCapabilities capabilities;
    uint32_t last_probe_ms;
    uint32_t successful_probes;
    uint32_t failed_probes;
} ArxSgwManager;

void arx_sgw_init(ArxSgwManager *manager);
void arx_sgw_set_state(ArxSgwManager *manager, ArxSgwState state);

#endif
