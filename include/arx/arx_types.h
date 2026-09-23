#ifndef ARX_TYPES_H
#define ARX_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    ARX_BUS_C1 = 0,
    ARX_BUS_C2,
    ARX_BUS_BH,
    ARX_BUS_COUNT
} ArxBus;

typedef enum {
    ARX_PRIORITY_HIGH = 0,
    ARX_PRIORITY_NORMAL,
    ARX_PRIORITY_LOW,
    ARX_PRIORITY_COUNT
} ArxPriority;

typedef struct {
    ArxBus bus;
    uint32_t id;
    bool extended_id;
    uint8_t dlc;
    uint8_t data[8];
    uint32_t timestamp_ms;
} ArxCanFrame;

typedef enum {
    ARX_STATUS_OK = 0,
    ARX_STATUS_EMPTY,
    ARX_STATUS_FULL,
    ARX_STATUS_INVALID,
    ARX_STATUS_EXPIRED,
    ARX_STATUS_IO_ERROR
} ArxStatus;

#endif
