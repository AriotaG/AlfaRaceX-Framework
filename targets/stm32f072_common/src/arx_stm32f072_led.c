#include "arx_stm32f072_target.h"

#if defined(ARX_TARGET_STM32F072)
#include "stm32f0xx_hal.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static TIM_HandleTypeDef htim1_arx;
static DMA_HandleTypeDef hdma_tim1_arx;
static volatile bool led_dma_busy;

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim) {
    if(!htim||htim->Instance!=TIM1)return;

    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    hdma_tim1_arx.Instance=DMA1_Channel4;
    hdma_tim1_arx.Init.Direction=DMA_MEMORY_TO_PERIPH;
    hdma_tim1_arx.Init.PeriphInc=DMA_PINC_DISABLE;
    hdma_tim1_arx.Init.MemInc=DMA_MINC_ENABLE;
    hdma_tim1_arx.Init.PeriphDataAlignment=DMA_PDATAALIGN_HALFWORD;
    hdma_tim1_arx.Init.MemDataAlignment=DMA_MDATAALIGN_HALFWORD;
    hdma_tim1_arx.Init.Mode=DMA_NORMAL;
    hdma_tim1_arx.Init.Priority=DMA_PRIORITY_LOW;
    if(HAL_DMA_Init(&hdma_tim1_arx)!=HAL_OK)Error_Handler();

    __HAL_LINKDMA(htim,hdma[TIM_DMA_ID_CC4],hdma_tim1_arx);
    __HAL_LINKDMA(htim,hdma[TIM_DMA_ID_TRIGGER],hdma_tim1_arx);
    __HAL_LINKDMA(htim,hdma[TIM_DMA_ID_COMMUTATION],hdma_tim1_arx);

    HAL_NVIC_SetPriority(DMA1_Channel4_5_6_7_IRQn,0,0);
    HAL_NVIC_EnableIRQ(DMA1_Channel4_5_6_7_IRQn);
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim) {
    if(!htim||htim->Instance!=TIM1)return;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g={0};
    g.Pin=GPIO_PIN_11;
    g.Mode=GPIO_MODE_AF_PP;
    g.Pull=GPIO_NOPULL;
    g.Speed=GPIO_SPEED_FREQ_LOW;
    g.Alternate=GPIO_AF2_TIM1;
    HAL_GPIO_Init(GPIOA,&g);
}

void arx_stm32f072_led_init(void) {
    TIM_ClockConfigTypeDef clock={0};
    TIM_MasterConfigTypeDef master={0};
    TIM_OC_InitTypeDef oc={0};
    TIM_BreakDeadTimeConfigTypeDef dead={0};

    htim1_arx.Instance=TIM1;
    htim1_arx.Init.Prescaler=0u;
    htim1_arx.Init.CounterMode=TIM_COUNTERMODE_UP;
    htim1_arx.Init.Period=ARX_STM32_WS2812_PERIOD_TICKS-1u;
    htim1_arx.Init.ClockDivision=TIM_CLOCKDIVISION_DIV1;
    htim1_arx.Init.RepetitionCounter=0u;
    htim1_arx.Init.AutoReloadPreload=TIM_AUTORELOAD_PRELOAD_DISABLE;
    if(HAL_TIM_Base_Init(&htim1_arx)!=HAL_OK)Error_Handler();

    clock.ClockSource=TIM_CLOCKSOURCE_INTERNAL;
    if(HAL_TIM_ConfigClockSource(&htim1_arx,&clock)!=HAL_OK)Error_Handler();
    if(HAL_TIM_PWM_Init(&htim1_arx)!=HAL_OK)Error_Handler();

    master.MasterOutputTrigger=TIM_TRGO_RESET;
    master.MasterSlaveMode=TIM_MASTERSLAVEMODE_DISABLE;
    if(HAL_TIMEx_MasterConfigSynchronization(&htim1_arx,&master)!=HAL_OK)Error_Handler();

    oc.OCMode=TIM_OCMODE_PWM1;
    oc.Pulse=0u;
    oc.OCPolarity=TIM_OCPOLARITY_HIGH;
    oc.OCFastMode=TIM_OCFAST_DISABLE;
    oc.OCIdleState=TIM_OCIDLESTATE_RESET;
    oc.OCNIdleState=TIM_OCNIDLESTATE_RESET;
    if(HAL_TIM_PWM_ConfigChannel(&htim1_arx,&oc,TIM_CHANNEL_4)!=HAL_OK)Error_Handler();

    dead.OffStateRunMode=TIM_OSSR_DISABLE;
    dead.OffStateIDLEMode=TIM_OSSI_DISABLE;
    dead.LockLevel=TIM_LOCKLEVEL_OFF;
    dead.DeadTime=0u;
    dead.BreakState=TIM_BREAK_DISABLE;
    dead.BreakPolarity=TIM_BREAKPOLARITY_HIGH;
    dead.AutomaticOutput=TIM_AUTOMATICOUTPUT_DISABLE;
    if(HAL_TIMEx_ConfigBreakDeadTime(&htim1_arx,&dead)!=HAL_OK)Error_Handler();

    HAL_TIM_MspPostInit(&htim1_arx);
    led_dma_busy=false;
}

void arx_stm32f072_led_suspend(void) {
    if(led_dma_busy){
        (void)HAL_TIM_PWM_Stop_DMA(&htim1_arx,TIM_CHANNEL_4);
        led_dma_busy=false;
    }
    (void)HAL_TIM_PWM_DeInit(&htim1_arx);
    (void)HAL_DMA_DeInit(&hdma_tim1_arx);
    HAL_NVIC_DisableIRQ(DMA1_Channel4_5_6_7_IRQn);
    __HAL_RCC_TIM1_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA,GPIO_PIN_11);
}

bool arx_target_led_dma_start(const uint16_t *pwm,size_t count) {
    if(!pwm||count==0u||led_dma_busy)return false;
    led_dma_busy=true;
    if(HAL_TIM_PWM_Start_DMA(&htim1_arx,TIM_CHANNEL_4,(uint32_t*)pwm,(uint16_t)count)!=HAL_OK){
        led_dma_busy=false;
        return false;
    }
    return true;
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if(htim!=&htim1_arx)return;
    (void)HAL_TIM_PWM_Stop_DMA(&htim1_arx,TIM_CHANNEL_4);
    led_dma_busy=false;
}

void arx_stm32f072_led_dma_irq(void) {
    HAL_DMA_IRQHandler(&hdma_tim1_arx);
}
#endif
