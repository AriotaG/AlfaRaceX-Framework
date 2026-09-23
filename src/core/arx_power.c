#include "arx/arx_power.h"
#include <string.h>

void arx_power_init(ArxPowerManager *p) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->state = ARX_POWER_AWAKE;

    /* Hardware-parity timing:
     * sleep after 3500 ms without CAN activity;
     * while asleep, CAN activity newer than 3400 ms requests wake-up.
     */
    p->sleep_after_ms = 3500u;
    p->wake_activity_window_ms = 3400u;
}

void arx_power_note_can_rx(ArxPowerManager *p, uint32_t now_ms) {
    if (p) p->last_can_rx_ms = now_ms;
}

bool arx_power_should_sleep(
    ArxPowerManager *p,
    ArxPowerBlocks b,
    uint32_t now_ms
) {
    if (!p || !p->enabled || p->state != ARX_POWER_AWAKE) return false;
    if (b.usb_slave_connected || b.sniffer_in_use || b.diagnostic_bridge_in_use) {
        return false;
    }
    if ((uint32_t)(now_ms - p->last_can_rx_ms) <= p->sleep_after_ms) return false;
    return true;
}

bool arx_power_should_wake(ArxPowerManager *p, uint32_t now_ms) {
    if (!p || p->state != ARX_POWER_LOW_CONSUME) return false;
    return (uint32_t)(now_ms - p->last_can_rx_ms) < p->wake_activity_window_ms;
}

bool arx_power_enter_low_consume(
    ArxPowerManager *p,
    ArxPowerBlocks blocks,
    uint32_t now_ms,
    const ArxPowerOps *ops
) {
    if (!arx_power_should_sleep(p, blocks, now_ms)) return false;

    if (ops) {
        if (ops->interchip_uart_pause) ops->interchip_uart_pause(ops->user);
        if (ops->slave_can_sleep_set) ops->slave_can_sleep_set(true, ops->user);
        if (ops->slave_reset_set) ops->slave_reset_set(true, ops->user);
    }

    p->state = ARX_POWER_LOW_CONSUME;
    p->sleep_count++;
    return true;
}

bool arx_power_wake_if_needed(
    ArxPowerManager *p,
    uint32_t now_ms,
    const ArxPowerOps *ops
) {
    if (!arx_power_should_wake(p, now_ms)) return false;

    if (ops) {
        if (ops->slave_can_sleep_set) ops->slave_can_sleep_set(false, ops->user);
        if (ops->slave_reset_set) ops->slave_reset_set(false, ops->user);
        if (ops->interchip_uart_resume) ops->interchip_uart_resume(ops->user);
        if (ops->on_wake_reconfigure) ops->on_wake_reconfigure(ops->user);
    }

    p->state = ARX_POWER_AWAKE;
    p->wake_count++;
    return true;
}
