#include "arx_stm32f072_target.h"

#if defined(ARX_TARGET_STM32F072)

#include "stm32f0xx_hal.h"
#include <string.h>

static CAN_HandleTypeDef hcan_arx;
static UART_HandleTypeDef huart_link;
static UART_HandleTypeDef huart_pedal;

static void arx_clock_init(void) {
    HAL_Init();

    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI48;
    osc.HSI48State = RCC_HSI48_ON;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI48;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_1) != HAL_OK) {
        Error_Handler();
    }

    periph.PeriphClockSelection = RCC_PERIPHCLK_USB;
    periph.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        Error_Handler();
    }
}

static void arx_gpio_common_init(ArxTargetRole role) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    /* PA0 red LED, PA1 blue LED. */
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOA, &g);

    /* CAN: PB8 RX / PB9 TX, AF4. */
    g.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF4_CAN;
    HAL_GPIO_Init(GPIOB, &g);

    /* Inter-controller single-wire UART: PA14 / USART2 AF1, open drain + pull-up. */
    g.Pin = GPIO_PIN_14;
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF1_USART2;
    HAL_GPIO_Init(GPIOA, &g);

    if (role == ARX_TARGET_ROLE_C1) {
        /* Pedal controller single-wire UART: PB6 / USART1 AF0. */
        g.Pin = GPIO_PIN_6;
        g.Mode = GPIO_MODE_AF_OD;
        g.Pull = GPIO_PULLUP;
        g.Speed = GPIO_SPEED_FREQ_MEDIUM;
        g.Alternate = GPIO_AF0_USART1;
        HAL_GPIO_Init(GPIOB, &g);

        /* PA4: open-drain reset line for C2/BH. */
        g.Pin = GPIO_PIN_4;
        g.Mode = GPIO_MODE_OUTPUT_OD;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_MEDIUM;
        HAL_GPIO_Init(GPIOA, &g);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

        /* PA5: CAN-transceiver sleep line, active high. */
        g.Pin = GPIO_PIN_5;
        g.Mode = GPIO_MODE_OUTPUT_PP;
        g.Pull = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_MEDIUM;
        HAL_GPIO_Init(GPIOA, &g);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

        /* Optional auxiliary outputs. */
        g.Pin = GPIO_PIN_7 | GPIO_PIN_8;
        g.Mode = GPIO_MODE_OUTPUT_PP;
        g.Pull = GPIO_PULLDOWN;
        g.Speed = GPIO_SPEED_FREQ_MEDIUM;
        HAL_GPIO_Init(GPIOA, &g);
    }
}

static void arx_can_hw_init(const ArxStm32TargetProfile *profile) {
    __HAL_RCC_CAN1_CLK_ENABLE();

    hcan_arx.Instance = CAN;
    hcan_arx.Init.Prescaler = profile->can_prescaler;
    hcan_arx.Init.Mode = CAN_MODE_NORMAL;
    hcan_arx.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan_arx.Init.TimeSeg1 = CAN_BS1_4TQ;
    hcan_arx.Init.TimeSeg2 = CAN_BS2_3TQ;
    hcan_arx.Init.TimeTriggeredMode = DISABLE;
    hcan_arx.Init.AutoBusOff = ENABLE;
    hcan_arx.Init.AutoWakeUp = DISABLE;
    hcan_arx.Init.AutoRetransmission = ENABLE;
    hcan_arx.Init.ReceiveFifoLocked = DISABLE;
    hcan_arx.Init.TransmitFifoPriority = ENABLE;

    if (HAL_CAN_Init(&hcan_arx) != HAL_OK) {
        Error_Handler();
    }

    CAN_FilterTypeDef filter = {0};
    filter.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterActivation = ENABLE;

    if (HAL_CAN_ConfigFilter(&hcan_arx, &filter) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_CAN_Start(&hcan_arx) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_CAN_ActivateNotification(&hcan_arx,CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(CEC_CAN_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(CEC_CAN_IRQn);
}

static void arx_uart_hw_init(ArxTargetRole role) {
    __HAL_RCC_USART2_CLK_ENABLE();

    huart_link.Instance = USART2;
    huart_link.Init.BaudRate = ARX_STM32_INTERCHIP_BAUD;
    huart_link.Init.WordLength = UART_WORDLENGTH_8B;
    huart_link.Init.StopBits = UART_STOPBITS_1;
    huart_link.Init.Parity = UART_PARITY_NONE;
    huart_link.Init.Mode = UART_MODE_TX_RX;
    huart_link.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart_link.Init.OverSampling = UART_OVERSAMPLING_16;
    huart_link.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_HalfDuplex_Init(&huart_link) != HAL_OK) {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);

    if (role == ARX_TARGET_ROLE_C1) {
        __HAL_RCC_USART1_CLK_ENABLE();

        huart_pedal.Instance = USART1;
        huart_pedal.Init.BaudRate = ARX_STM32_PEDAL_BAUD;
        huart_pedal.Init.WordLength = UART_WORDLENGTH_8B;
        huart_pedal.Init.StopBits = UART_STOPBITS_1;
        huart_pedal.Init.Parity = UART_PARITY_NONE;
        huart_pedal.Init.Mode = UART_MODE_TX_RX;
        huart_pedal.Init.HwFlowCtl = UART_HWCONTROL_NONE;
        huart_pedal.Init.OverSampling = UART_OVERSAMPLING_16;
        huart_pedal.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

        if (HAL_HalfDuplex_Init(&huart_pedal) != HAL_OK) {
            Error_Handler();
        }

        HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
}

void arx_stm32f072_platform_init(ArxTargetRole role) {
    const ArxStm32TargetProfile *profile = arx_stm32_profile(role);
    if (!profile) Error_Handler();

    arx_clock_init();
    arx_gpio_common_init(role);
    arx_can_hw_init(profile);
    arx_uart_hw_init(role);
    if(role==ARX_TARGET_ROLE_C1) arx_stm32f072_led_init();
}

CAN_HandleTypeDef *arx_stm32_can_handle(void) {
    return &hcan_arx;
}

UART_HandleTypeDef *arx_stm32_interchip_uart_handle(void) {
    return &huart_link;
}

UART_HandleTypeDef *arx_stm32_pedal_uart_handle(void) {
    return &huart_pedal;
}

void arx_stm32_slave_reset(bool asserted) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, asserted ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void arx_stm32_slave_can_sleep(bool sleep) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, sleep ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

#endif
