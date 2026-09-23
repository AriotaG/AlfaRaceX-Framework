#include "arx/arx_crc.h"

uint8_t arx_crc8_sae_j1850(const uint8_t *data, size_t length_without_crc) {
    if (!data || length_without_crc == 0u) {
        return 0u;
    }

    uint8_t crc = 0xFFu;
    for (size_t i = 0; i < length_without_crc; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1u) ^ 0x1Du)
                                : (uint8_t)(crc << 1u);
        }
    }
    return (uint8_t)(crc ^ 0xFFu);
}
