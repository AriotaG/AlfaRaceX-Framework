#ifndef ARX_CRC_H
#define ARX_CRC_H

#include <stddef.h>
#include <stdint.h>

uint8_t arx_crc8_sae_j1850(const uint8_t *data, size_t length_without_crc);

#endif
