#ifndef ARX_STM32F072_USB_H
#define ARX_STM32F072_USB_H

#include "arx/arx_usb_mode.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(ARX_TARGET_STM32F072)
bool arx_stm32f072_usb_attach(ArxUsbMode mode,bool shared_led_pin);
bool arx_stm32f072_usb_detach(void);
bool arx_stm32f072_usb_send(const uint8_t *data,size_t length);
bool arx_stm32f072_usb_is_configured(void);
void arx_stm32f072_usb_irq(void);

/* Implemented by the board application; invoked from the USB OUT callback. */
void arx_stm32f072_usb_rx(const uint8_t *data,size_t length);
#endif

#endif
