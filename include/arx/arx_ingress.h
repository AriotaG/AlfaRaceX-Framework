#ifndef ARX_INGRESS_H
#define ARX_INGRESS_H
#include "arx/arx_can.h"
#include <stdbool.h>
#define ARX_INGRESS_CAPACITY 16u
#define ARX_INGRESS_USB_SIZE 64u
typedef enum { ARX_INGRESS_CAN, ARX_INGRESS_INTERCHIP, ARX_INGRESS_PEDAL, ARX_INGRESS_USB, ARX_INGRESS_KIND_COUNT } ArxIngressKind;
typedef struct {
    ArxIngressKind kind;
    uint32_t timestamp_ms;
    uint8_t length;
    union { ArxCanFrame can; uint8_t bytes[ARX_INGRESS_USB_SIZE]; } data;
} ArxIngressEvent;
typedef struct {
    ArxIngressEvent events[ARX_INGRESS_CAPACITY];
    uint8_t head, tail, count;
    uint32_t rejected[ARX_INGRESS_KIND_COUNT];
} ArxIngressQueue;
/* Caller serializes push/pop across IRQ and main contexts. No runtime work inside the lock. */
bool arx_ingress_push(ArxIngressQueue *queue,const ArxIngressEvent *event);
bool arx_ingress_pop(ArxIngressQueue *queue,ArxIngressEvent *event);
#endif
