#include "arx_stm32f072_target.h"
#include "arx_stm32f072_usb.h"

#if defined(ARX_TARGET_STM32F072)
#include "stm32f0xx_hal.h"

extern CAN_HandleTypeDef *arx_stm32_can_handle(void);
extern UART_HandleTypeDef *arx_stm32_interchip_uart_handle(void);
extern UART_HandleTypeDef *arx_stm32_pedal_uart_handle(void);

void Error_Handler(void) {
    __disable_irq();
    for(;;) { }
}

void SysTick_Handler(void) {
    HAL_IncTick();
    HAL_SYSTICK_IRQHandler();
}

void CEC_CAN_IRQHandler(void) {
    HAL_CAN_IRQHandler(arx_stm32_can_handle());
}

void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(arx_stm32_pedal_uart_handle());
}

void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(arx_stm32_interchip_uart_handle());
}

void DMA1_Channel4_5_6_7_IRQHandler(void) {
    arx_stm32f072_led_dma_irq();
}

void USB_IRQHandler(void) {
    arx_stm32f072_usb_irq();
}
#endif
