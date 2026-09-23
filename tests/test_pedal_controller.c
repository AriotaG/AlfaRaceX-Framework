#include "arx/features/arx_pedal_controller.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    ArxPedalController p;
    arx_pedal_init(&p);

    assert(ARX_PEDAL_UART_BAUD == 9600u);
    assert(arx_pedal_set_mode(&p, ARX_PEDAL_AUTO));
    assert(arx_pedal_set_power(&p, 0));

    assert(arx_pedal_target_map(&p, ARX_DNA_NATURAL) == ARX_PEDAL_MAP_NATURAL);
    assert(arx_pedal_target_map(&p, ARX_DNA_DYNAMIC) == ARX_PEDAL_MAP_DYNAMIC);
    assert(arx_pedal_target_map(&p, ARX_DNA_ALL_WEATHER) == ARX_PEDAL_MAP_ALL_WEATHER);
    assert(arx_pedal_target_map(&p, ARX_DNA_RACE) == ARX_PEDAL_MAP_RACE);

    uint8_t packet[ARX_PEDAL_PACKET_SIZE];
    assert(arx_pedal_build_map_packet(&p, ARX_PEDAL_MAP_DYNAMIC, packet));
    assert(packet[0] == '#');
    assert(packet[1] == 0xB6u);
    assert(packet[2] == 0xDBu);
    assert(packet[3] == 192u);
    assert(packet[4] == 128u);
    assert(packet[5] == 154u);

    arx_pedal_on_reply(&p, 0xEBu);
    assert(p.applied_map == ARX_PEDAL_MAP_DYNAMIC);

    assert(arx_pedal_set_mode(&p, ARX_PEDAL_HYBRID_ALIGN));
    assert(arx_pedal_target_map(&p, ARX_DNA_DYNAMIC) == ARX_PEDAL_MAP_NATURAL);
    assert(arx_pedal_target_map(&p, ARX_DNA_RACE) == ARX_PEDAL_MAP_RACE);

    assert(arx_pedal_set_mode(&p, ARX_PEDAL_KIDS_LIMITER));
    assert(arx_pedal_kids_override_required(&p, true, 3100u, 50.0f));
    assert(arx_pedal_kids_override_required(&p, true, 2000u, 101.0f));
    assert(!arx_pedal_kids_override_required(&p, true, 2000u, 50.0f));
    assert(arx_pedal_build_zero_override(&p, packet));
    assert(packet[1] == 0xFFu && packet[2] == 0x00u);

    puts("pedal controller tests: OK");
    return 0;
}
