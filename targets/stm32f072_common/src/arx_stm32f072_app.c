#include "arx_stm32f072_app.h"

#if defined(ARX_TARGET_STM32F072)

#include "arx_stm32f072_target.h"
#include "arx_stm32f072_storage.h"
#include "arx_stm32f072_log.h"
#include "arx_stm32f072_usb.h"
#include "stm32f0xx_hal.h"
#include <string.h>

extern void arx_stm32f072_platform_init(ArxTargetRole role);
extern CAN_HandleTypeDef *arx_stm32_can_handle(void);
extern UART_HandleTypeDef *arx_stm32_interchip_uart_handle(void);
extern UART_HandleTypeDef *arx_stm32_pedal_uart_handle(void);

static ArxRuntime runtime_ctx;
static ArxTargetRole target_role;
static uint8_t uart2_rx[ARX_INTERCHIP_FRAME_SIZE];
static uint8_t pedal_rx_byte;
static uint16_t ws2812_pwm[ARX_WS2812_PWM_WORDS];
static ArxStorageBackend target_storage;

static void power_uart_pause(void *user) {
    (void)user;
    UART_HandleTypeDef *u=arx_stm32_interchip_uart_handle();
    if(u) (void)HAL_UART_AbortReceive(u);
}

static void power_uart_resume(void *user) {
    (void)user;
    UART_HandleTypeDef *u=arx_stm32_interchip_uart_handle();
    if(u) (void)HAL_UART_Receive_IT(u,uart2_rx,ARX_INTERCHIP_FRAME_SIZE);
}

static void power_reset(bool asserted,void *user) {
    (void)user;
    arx_stm32_slave_reset(asserted);
}

static void power_can_sleep(bool sleep,void *user) {
    (void)user;
    arx_stm32_slave_can_sleep(sleep);
}

static void power_wake_reconfigure(void *user) {
    (void)user;
    runtime_ctx.remote_dyno_active=false;
    runtime_ctx.remote_brake_forced=false;
    arx_runtime_queue_config_sync(&runtime_ctx);
}

static const ArxPowerOps power_ops={
    .interchip_uart_pause=power_uart_pause,
    .interchip_uart_resume=power_uart_resume,
    .slave_reset_set=power_reset,
    .slave_can_sleep_set=power_can_sleep,
    .on_wake_reconfigure=power_wake_reconfigure,
    .user=0
};

static ArxStatus can_sender(const ArxCanFrame *f,void *user) {
    (void)user;
    CAN_HandleTypeDef *h=arx_stm32_can_handle();
    if(!h||!f)return ARX_STATUS_INVALID;

    CAN_TxHeaderTypeDef tx={0};
    tx.RTR=CAN_RTR_DATA;
    tx.DLC=f->dlc;

    if(f->extended_id){
        tx.IDE=CAN_ID_EXT;
        tx.ExtId=f->id;
    }else{
        tx.IDE=CAN_ID_STD;
        tx.StdId=f->id;
    }

    uint32_t mailbox=0u;
    return HAL_CAN_AddTxMessage(h,&tx,(uint8_t*)f->data,&mailbox)==HAL_OK
        ?ARX_STATUS_OK:ARX_STATUS_IO_ERROR;
}

static bool interchip_sender(const uint8_t frame[ARX_INTERCHIP_FRAME_SIZE],void *user) {
    (void)user;
    UART_HandleTypeDef *u=arx_stm32_interchip_uart_handle();
    if(!u)return false;
    return HAL_UART_Transmit(
        u,(uint8_t*)frame,ARX_INTERCHIP_FRAME_SIZE,20u
    )==HAL_OK;
}

static bool pedal_sender(const uint8_t packet[ARX_PEDAL_PACKET_SIZE],void *user) {
    (void)user;
    if(target_role!=ARX_TARGET_ROLE_C1)return false;
    UART_HandleTypeDef *u=arx_stm32_pedal_uart_handle();
    if(!u)return false;
    return HAL_UART_Transmit(
        u,(uint8_t*)packet,ARX_PEDAL_PACKET_SIZE,20u
    )==HAL_OK;
}

static bool usb_attach_cb(ArxUsbMode mode,void *user) {
    (void)user;
    return arx_stm32f072_usb_attach(mode,target_role==ARX_TARGET_ROLE_C1);
}

static bool usb_detach_cb(void *user) {
    (void)user;
    return arx_stm32f072_usb_detach();
}

static bool usb_sender(const uint8_t *data,size_t length,void *user) {
    (void)user;
    return arx_stm32f072_usb_send(data,length);
}

static bool led_sender(const ArxRgb rgb[ARX_LED_COUNT],void *user) {
    (void)user;
    if(target_role!=ARX_TARGET_ROLE_C1)return false;
    size_t n=arx_led_strip_encode_ws2812_brg(
        rgb,ws2812_pwm,ARX_WS2812_PWM_WORDS
    );
    return n==ARX_WS2812_PWM_WORDS&&arx_target_led_dma_start(ws2812_pwm,n);
}


static bool persist_config_cb(const ArxRuntimeConfig *config,void *user) {
    (void)user;
    if(target_role!=ARX_TARGET_ROLE_C1)return false;
    return arx_storage_write_settings(&target_storage,config);
}

static bool persist_performance_cb(float a,float b,void *user) {
    (void)user;
    if(target_role!=ARX_TARGET_ROLE_C1)return false;
    return arx_storage_write_best_seconds(&target_storage,a,b);
}

static bool persist_visibility_cb(const uint8_t *visible,uint16_t count,void *user) {
    (void)user;
    if(target_role!=ARX_TARGET_ROLE_C1)return false;
    return arx_storage_write_visible(&target_storage,visible,count);
}

static bool persist_mirror_cb(const ArxMirrorStorage *mirror,void *user) {
    (void)user;
    if(target_role!=ARX_TARGET_ROLE_BH)return false;
    return arx_storage_write_mirror(&target_storage,mirror);
}

static bool save_log_cb(void *user) {
    (void)user;
    return arx_stm32f072_log_save(&runtime_ctx,HAL_GetTick());
}

static ArxRuntimeRole runtime_role(ArxTargetRole r) {
    return r==ARX_TARGET_ROLE_C1?ARX_RUNTIME_C1:
           r==ARX_TARGET_ROLE_C2?ARX_RUNTIME_C2:ARX_RUNTIME_BH;
}

void arx_stm32f072_app_init(ArxTargetRole role) {
    target_role=role;
    arx_stm32f072_platform_init(role);

    ArxRuntimeOps ops={
        .can_send=can_sender,
        .interchip_send=interchip_sender,
        .pedal_send=pedal_sender,
        .usb_attach=usb_attach_cb,
        .usb_detach=usb_detach_cb,
        .usb_send=usb_sender,
        .led_submit=led_sender,
        .persist_config=persist_config_cb,
        .persist_performance=persist_performance_cb,
        .persist_visibility=persist_visibility_cb,
        .persist_mirror=persist_mirror_cb,
        .save_log=save_log_cb,
        .user=0
    };
    arx_runtime_init(&runtime_ctx,runtime_role(role),&ops);
    runtime_ctx.power.enabled=(role==ARX_TARGET_ROLE_C1);

    target_storage=arx_stm32f072_storage_backend();

    if(role==ARX_TARGET_ROLE_C1){
        ArxRuntimeConfig stored;
        if(arx_storage_read_settings(&target_storage,&stored)){
            arx_runtime_apply_config(&runtime_ctx,&stored,HAL_GetTick());
        }

        (void)arx_storage_read_visible(
            &target_storage,runtime_ctx.visible_params,
            (uint16_t)sizeof(runtime_ctx.visible_params)
        );

        const float best_0_100=arx_storage_read_best_seconds(&target_storage,1u);
        const float best_100_200=arx_storage_read_best_seconds(&target_storage,2u);
        if(best_0_100>0.0f&&best_0_100<=20.0f)
            runtime_ctx.performance.best_zero_to_100_s=best_0_100;
        if(best_100_200>0.0f&&best_100_200<=40.0f)
            runtime_ctx.performance.best_hundred_to_200_s=best_100_200;
        runtime_ctx.performance.best_dirty=false;

        /* Synchronize the effective C1 configuration, after Flash has been read. */
        arx_runtime_queue_config_sync(&runtime_ctx);
    }

    if(role==ARX_TARGET_ROLE_BH){
        ArxMirrorStorage mirror;
        if(arx_storage_read_mirror(&target_storage,&mirror)){
            runtime_ctx.park_mirror.park.left_h=mirror.left_park_h;
            runtime_ctx.park_mirror.park.left_v=mirror.left_park_v;
            runtime_ctx.park_mirror.park.right_h=mirror.right_park_h;
            runtime_ctx.park_mirror.park.right_v=mirror.right_park_v;

            runtime_ctx.park_mirror.normal.left_h=mirror.left_normal_h;
            runtime_ctx.park_mirror.normal.left_v=mirror.left_normal_v;
            runtime_ctx.park_mirror.normal.right_h=mirror.right_normal_h;
            runtime_ctx.park_mirror.normal.right_v=mirror.right_normal_v;

            runtime_ctx.park_mirror.calibrated_park=
                mirror.left_park_h!=0xFFu || mirror.left_park_v!=0xFFu ||
                mirror.right_park_h!=0xFFu || mirror.right_park_v!=0xFFFFu;

            runtime_ctx.park_mirror.calibrated_normal=
                !mirror.normal_position_uninitialized;
        }
    }

    HAL_UART_Receive_IT(
        arx_stm32_interchip_uart_handle(),
        uart2_rx,ARX_INTERCHIP_FRAME_SIZE
    );
    if(role==ARX_TARGET_ROLE_C1){
        HAL_UART_Receive_IT(
            arx_stm32_pedal_uart_handle(),
            &pedal_rx_byte,1u
        );
    }
}

ArxRuntime *arx_stm32f072_runtime(void) {
    return &runtime_ctx;
}

void arx_stm32f072_interchip_rx(
    const uint8_t raw[ARX_INTERCHIP_FRAME_SIZE]
) {
    arx_runtime_on_interchip(&runtime_ctx,raw,HAL_GetTick());
}

void arx_stm32f072_pedal_rx(uint8_t reply_byte) {
    arx_runtime_on_pedal_reply(&runtime_ctx,reply_byte);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    if(hcan!=arx_stm32_can_handle())return;

    CAN_RxHeaderTypeDef h={0};
    uint8_t data[8]={0};
    if(HAL_CAN_GetRxMessage(hcan,CAN_RX_FIFO0,&h,data)!=HAL_OK)return;

    ArxCanFrame f={0};
    f.bus=runtime_ctx.role==ARX_RUNTIME_C1?ARX_BUS_C1:
          runtime_ctx.role==ARX_RUNTIME_C2?ARX_BUS_C2:ARX_BUS_BH;
    f.extended_id=(h.IDE==CAN_ID_EXT);
    f.id=f.extended_id?h.ExtId:h.StdId;
    f.dlc=h.DLC>8u?8u:h.DLC;
    f.timestamp_ms=HAL_GetTick();
    memcpy(f.data,data,f.dlc);

    arx_runtime_on_can(&runtime_ctx,&f,f.timestamp_ms);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if(huart==arx_stm32_interchip_uart_handle()){
        arx_stm32f072_interchip_rx(uart2_rx);
        HAL_UART_Receive_IT(huart,uart2_rx,ARX_INTERCHIP_FRAME_SIZE);
        return;
    }

    if(target_role==ARX_TARGET_ROLE_C1&&
       huart==arx_stm32_pedal_uart_handle()){
        arx_stm32f072_pedal_rx(pedal_rx_byte);
        HAL_UART_Receive_IT(huart,&pedal_rx_byte,1u);
    }
}

void arx_stm32f072_usb_rx(const uint8_t *data,size_t length) {
    if(!data||length==0u)return;
    arx_runtime_usb_command(&runtime_ctx,HAL_GetTick());
}

void arx_stm32f072_app_loop(void) {
    const uint32_t now=HAL_GetTick();
    if(runtime_ctx.usb_mode.state==ARX_USB_WAIT_HOST&&arx_stm32f072_usb_is_configured())
        arx_runtime_usb_configured(&runtime_ctx,now);

    if(target_role==ARX_TARGET_ROLE_C1){
        ArxPowerBlocks blocks={
            .usb_slave_connected=runtime_ctx.remote_usb_c2_active||runtime_ctx.remote_usb_bh_active,
            .sniffer_in_use=runtime_ctx.sniffer.in_use,
            .diagnostic_bridge_in_use=(runtime_ctx.usb_mode.mode==ARX_USB_MODE_DIAGNOSTIC&&
                                      runtime_ctx.usb_mode.state!=ARX_USB_DETACHED)
        };
        if(runtime_ctx.power.state==ARX_POWER_AWAKE){
            if(arx_power_enter_low_consume(&runtime_ctx.power,blocks,now,&power_ops)){
                runtime_ctx.remote_dyno_active=false;
                runtime_ctx.remote_brake_forced=false;
            }
        }else{
            (void)arx_power_wake_if_needed(&runtime_ctx.power,now,&power_ops);
        }
    }

    arx_runtime_tick(&runtime_ctx,now);
    (void)arx_runtime_drain_can(&runtime_ctx,now,8u);
    (void)arx_runtime_drain_interchip(&runtime_ctx,now,1u);
}

#endif
