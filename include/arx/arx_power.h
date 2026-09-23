#ifndef ARX_POWER_H
#define ARX_POWER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ARX_POWER_AWAKE = 0,
    ARX_POWER_LOW_CONSUME
} ArxPowerState;

typedef struct {
    bool enabled;
    ArxPowerState state;
    uint32_t last_can_rx_ms;

    uint32_t sleep_after_ms;
    uint32_t wake_activity_window_ms;

    uint32_t sleep_count;
    uint32_t wake_count;
} ArxPowerManager;

typedef struct {
    bool usb_slave_connected;
    bool sniffer_in_use;
    bool diagnostic_bridge_in_use;
} ArxPowerBlocks;

typedef struct {
    void (*interchip_uart_pause)(void *user);
    void (*interchip_uart_resume)(void *user);
    void (*slave_reset_set)(bool asserted, void *user);
    void (*slave_can_sleep_set)(bool sleep, void *user);
    void (*on_wake_reconfigure)(void *user);
    void *user;
} ArxPowerOps;

void arx_power_init(ArxPowerManager *p);
void arx_power_note_can_rx(ArxPowerManager *p, uint32_t now_ms);

bool arx_power_should_sleep(
    ArxPowerManager *p,
    ArxPowerBlocks blocks,
    uint32_t now_ms
);

bool arx_power_should_wake(
    ArxPowerManager *p,
    uint32_t now_ms
);

bool arx_power_enter_low_consume(
    ArxPowerManager *p,
    ArxPowerBlocks blocks,
    uint32_t now_ms,
    const ArxPowerOps *ops
);

bool arx_power_wake_if_needed(
    ArxPowerManager *p,
    uint32_t now_ms,
    const ArxPowerOps *ops
);

#endif
