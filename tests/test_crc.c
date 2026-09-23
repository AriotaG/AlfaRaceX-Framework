#include "arx/arx_crc.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const unsigned char data[] = {0x10,0x20,0x30,0x40};
    unsigned char a = arx_crc8_sae_j1850(data, sizeof(data));
    unsigned char b = arx_crc8_sae_j1850(data, sizeof(data));
    assert(a == b);
    puts("crc tests: OK");
    return 0;
}
