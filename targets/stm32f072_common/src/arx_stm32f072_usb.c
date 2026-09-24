#include "arx_stm32f072_usb.h"

#if defined(ARX_TARGET_STM32F072)

#include "arx_stm32f072_target.h"
#include "stm32f0xx_hal.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_msc.h"
#include <string.h>

#ifndef ARX_FIRMWARE_VERSION
#define ARX_FIRMWARE_VERSION "dev"
#endif

#ifndef ARX_USB_VID
#define ARX_USB_VID 0x0483U
#endif
#ifndef ARX_USB_CDC_PID
#define ARX_USB_CDC_PID 0x5740U
#endif
#ifndef ARX_USB_MSC_PID
#define ARX_USB_MSC_PID 0x572AU
#endif

#define ARX_USB_SERIAL_DESC_SIZE 26U
#define ARX_USB_RX_SIZE          CDC_DATA_FS_OUT_PACKET_SIZE
#define ARX_USB_TX_SIZE          CDC_DATA_FS_IN_PACKET_SIZE
#define ARX_MSC_BLOCK_SIZE       512U
#define ARX_MSC_BLOCK_COUNT      128U

#if defined(ARX_BUILD_BH)
#define ARX_MSC_VOLUME_LABEL "ALFARACEXBH"
#elif defined(ARX_BUILD_C2)
#define ARX_MSC_VOLUME_LABEL "ALFARACEXC2"
#else
#define ARX_MSC_VOLUME_LABEL "ALFARACEXC1"
#endif

static USBD_HandleTypeDef usb_device;
static PCD_HandleTypeDef usb_pcd;
static uint8_t usb_rx[ARX_USB_RX_SIZE];
static uint8_t usb_tx[ARX_USB_TX_SIZE];
static uint8_t string_desc[USBD_MAX_STR_DESC_SIZ];
static uint8_t serial_desc[ARX_USB_SERIAL_DESC_SIZE]={ARX_USB_SERIAL_DESC_SIZE,USB_DESC_TYPE_STRING};
static bool usb_attached;
static bool restore_led_after_detach;
static ArxUsbMode active_mode=ARX_USB_MODE_NONE;

static const uint8_t cdc_device_desc[USB_LEN_DEV_DESC]={
    USB_LEN_DEV_DESC,USB_DESC_TYPE_DEVICE,
    0x00U,0x02U,
    0x02U,0x02U,0x00U,
    USB_MAX_EP0_SIZE,
    LOBYTE(ARX_USB_VID),HIBYTE(ARX_USB_VID),
    LOBYTE(ARX_USB_CDC_PID),HIBYTE(ARX_USB_CDC_PID),
    0x00U,0x01U,
    USBD_IDX_MFC_STR,USBD_IDX_PRODUCT_STR,USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION
};

static const uint8_t msc_device_desc[USB_LEN_DEV_DESC]={
    USB_LEN_DEV_DESC,USB_DESC_TYPE_DEVICE,
    0x00U,0x02U,
    0x00U,0x00U,0x00U,
    USB_MAX_EP0_SIZE,
    LOBYTE(ARX_USB_VID),HIBYTE(ARX_USB_VID),
    LOBYTE(ARX_USB_MSC_PID),HIBYTE(ARX_USB_MSC_PID),
    0x00U,0x01U,
    USBD_IDX_MFC_STR,USBD_IDX_PRODUCT_STR,USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION
};

static const uint8_t lang_desc[USB_LEN_LANGID_STR_DESC]={
    USB_LEN_LANGID_STR_DESC,USB_DESC_TYPE_STRING,0x09U,0x04U
};

static const char msc_version_text[]="ALFARACEX V." ARX_FIRMWARE_VERSION "\r\n";

static int8_t msc_inquiry[36]={
    0x00,0x80,0x02,0x02,31,0x00,0x00,0x00,
    'A','R','X',' ',' ',' ',' ',' ',
    'A','l','f','a','R','a','c','e','X',' ','M','S','C',' ',' ',' ',
    'R','C','5',' '
};

static void put_le16(uint8_t *p,uint16_t v){
    p[0]=(uint8_t)v;
    p[1]=(uint8_t)(v>>8u);
}

static void put_le32(uint8_t *p,uint32_t v){
    p[0]=(uint8_t)v;
    p[1]=(uint8_t)(v>>8u);
    p[2]=(uint8_t)(v>>16u);
    p[3]=(uint8_t)(v>>24u);
}

static void msc_make_sector(uint32_t sector,uint8_t out[ARX_MSC_BLOCK_SIZE]){
    memset(out,0,ARX_MSC_BLOCK_SIZE);

    if(sector==0u){
        out[0]=0xEBu;out[1]=0xFEu;out[2]=0x90u;
        memcpy(&out[3],"MSDOS5.0",8u);
        put_le16(&out[11],ARX_MSC_BLOCK_SIZE);
        out[13]=1u;
        put_le16(&out[14],1u);
        out[16]=1u;
        put_le16(&out[17],32u);
        put_le16(&out[19],ARX_MSC_BLOCK_COUNT);
        out[21]=0xF8u;
        put_le16(&out[22],1u);
        put_le16(&out[24],63u);
        put_le16(&out[26],255u);
        put_le32(&out[28],0u);
        put_le32(&out[32],0u);
        out[36]=0x80u;
        out[37]=0u;
        out[38]=0x29u;
        put_le32(&out[39],0x5AD80080u);
        memcpy(&out[43],ARX_MSC_VOLUME_LABEL,11u);
        memcpy(&out[54],"FAT12   ",8u);
        out[510]=0x55u;
        out[511]=0xAAu;
        return;
    }

    if(sector==1u){
        out[0]=0xF8u;
        out[1]=0xFFu;
        out[2]=0xFFu;
        out[3]=0xFFu;
        out[4]=0x0Fu;
        return;
    }

    if(sector==2u){
        memcpy(&out[0],ARX_MSC_VOLUME_LABEL,11u);
        out[11]=0x08u;

        memcpy(&out[32],"VERSION TXT",11u);
        out[43]=0x20u;
        put_le16(&out[32+26],2u);
        put_le32(&out[32+28],(uint32_t)(sizeof(msc_version_text)-1u));
        return;
    }

    if(sector==4u){
        const size_t n=(sizeof(msc_version_text)-1u)<ARX_MSC_BLOCK_SIZE
            ?(sizeof(msc_version_text)-1u):ARX_MSC_BLOCK_SIZE;
        memcpy(out,msc_version_text,n);
    }
}

static int8_t msc_init(uint8_t lun){(void)lun;return 0;}
static int8_t msc_capacity(uint8_t lun,uint32_t *blocks,uint16_t *size){
    (void)lun;
    if(!blocks||!size)return -1;
    *blocks=ARX_MSC_BLOCK_COUNT;
    *size=ARX_MSC_BLOCK_SIZE;
    return 0;
}
static int8_t msc_ready(uint8_t lun){(void)lun;return 0;}
static int8_t msc_write_protected(uint8_t lun){(void)lun;return 1;}
static int8_t msc_read(uint8_t lun,uint8_t *buf,uint32_t block,uint16_t count){
    (void)lun;
    if(!buf||block>=ARX_MSC_BLOCK_COUNT||
       (uint32_t)count>ARX_MSC_BLOCK_COUNT-block)return -1;
    for(uint16_t i=0u;i<count;i++)
        msc_make_sector(block+i,&buf[(size_t)i*ARX_MSC_BLOCK_SIZE]);
    return 0;
}
static int8_t msc_write(uint8_t lun,uint8_t *buf,uint32_t block,uint16_t count){
    (void)lun;(void)buf;(void)block;(void)count;
    return -1;
}
static int8_t msc_max_lun(void){return 0;}

static USBD_StorageTypeDef msc_ops={
    msc_init,msc_capacity,msc_ready,msc_write_protected,
    msc_read,msc_write,msc_max_lun,msc_inquiry
};

static void unicode_hex32(uint32_t value,uint8_t *dst,uint8_t nibbles){
    static const char hex[]="0123456789ABCDEF";
    for(uint8_t i=0u;i<nibbles;i++){
        dst[(size_t)i*2u]=(uint8_t)hex[(value>>28u)&0x0Fu];
        dst[(size_t)i*2u+1u]=0u;
        value<<=4u;
    }
}

static void build_serial(void){
    const uint32_t a=*(volatile const uint32_t*)0x1FFFF7ACu;
    const uint32_t b=*(volatile const uint32_t*)0x1FFFF7B0u;
    const uint32_t c=*(volatile const uint32_t*)0x1FFFF7B4u;
    unicode_hex32(a+c,&serial_desc[2],8u);
    unicode_hex32(b,&serial_desc[18],4u);
}

static uint8_t *desc_device(USBD_SpeedTypeDef speed,uint16_t *length){
    (void)speed;
    *length=USB_LEN_DEV_DESC;
    return (uint8_t*)(active_mode==ARX_USB_MODE_LEGACY_MSC
        ?msc_device_desc:cdc_device_desc);
}
static uint8_t *desc_lang(USBD_SpeedTypeDef speed,uint16_t *length){
    (void)speed;*length=(uint16_t)sizeof(lang_desc);return (uint8_t*)lang_desc;
}
static uint8_t *desc_text(const char *text,uint16_t *length){
    USBD_GetString((uint8_t*)text,string_desc,length);return string_desc;
}
static uint8_t *desc_manufacturer(USBD_SpeedTypeDef speed,uint16_t *length){
    (void)speed;return desc_text("AlfaRaceX",length);
}
static uint8_t *desc_product(USBD_SpeedTypeDef speed,uint16_t *length){
    (void)speed;
    if(active_mode==ARX_USB_MODE_LEGACY_MSC)return desc_text("AlfaRaceX MSC",length);
    return desc_text(active_mode==ARX_USB_MODE_DIAGNOSTIC
        ?"AlfaRaceX Diagnostics":"AlfaRaceX CAN Stream",length);
}
static uint8_t *desc_serial(USBD_SpeedTypeDef speed,uint16_t *length){
    (void)speed;build_serial();*length=ARX_USB_SERIAL_DESC_SIZE;return serial_desc;
}
static uint8_t *desc_config(USBD_SpeedTypeDef speed,uint16_t *length){
    (void)speed;
    return desc_text(active_mode==ARX_USB_MODE_LEGACY_MSC?"ARX MSC":"ARX CDC",length);
}
static uint8_t *desc_interface(USBD_SpeedTypeDef speed,uint16_t *length){
    (void)speed;
    return desc_text(active_mode==ARX_USB_MODE_LEGACY_MSC
        ?"ARX Storage":"ARX Interface",length);
}

static USBD_DescriptorsTypeDef descriptors={
    desc_device,desc_lang,desc_manufacturer,desc_product,
    desc_serial,desc_config,desc_interface
};

static int8_t cdc_init(void){
    (void)USBD_CDC_SetTxBuffer(&usb_device,usb_tx,0u);
    (void)USBD_CDC_SetRxBuffer(&usb_device,usb_rx);
    return (int8_t)USBD_OK;
}
static int8_t cdc_deinit(void){return (int8_t)USBD_OK;}
static int8_t cdc_control(uint8_t cmd,uint8_t *buf,uint16_t length){
    (void)length;
    if(cmd==CDC_GET_LINE_CODING&&buf){
        const uint32_t baud=115200u;
        buf[0]=(uint8_t)baud;buf[1]=(uint8_t)(baud>>8u);
        buf[2]=(uint8_t)(baud>>16u);buf[3]=(uint8_t)(baud>>24u);
        buf[4]=0u;buf[5]=0u;buf[6]=8u;
    }
    return (int8_t)USBD_OK;
}
static int8_t cdc_receive(uint8_t *buf,uint32_t *length){
    if(buf&&length&&*length) arx_stm32f072_usb_rx(buf,(size_t)*length);
    (void)USBD_CDC_SetRxBuffer(&usb_device,usb_rx);
    (void)USBD_CDC_ReceivePacket(&usb_device);
    return (int8_t)USBD_OK;
}
static USBD_CDC_ItfTypeDef cdc_ops={
    .Init=cdc_init,.DeInit=cdc_deinit,.Control=cdc_control,
    .Receive=cdc_receive
};

void *arx_usbd_static_malloc(uint32_t size){
    enum {
        words=((sizeof(USBD_MSC_BOT_HandleTypeDef)>sizeof(USBD_CDC_HandleTypeDef)
            ?sizeof(USBD_MSC_BOT_HandleTypeDef):sizeof(USBD_CDC_HandleTypeDef))+3u)/4u
    };
    static uint32_t memory[words];
    if(size>sizeof(memory))return NULL;
    memset(memory,0,sizeof(memory));
    return memory;
}
void arx_usbd_static_free(void *ptr){(void)ptr;}

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd){
    if(!hpcd||hpcd->Instance!=USB)return;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USB_CLK_ENABLE();
    GPIO_InitTypeDef gpio={0};
    gpio.Pin=GPIO_PIN_11|GPIO_PIN_12;
    gpio.Mode=GPIO_MODE_AF_PP;
    gpio.Pull=GPIO_NOPULL;
    gpio.Speed=GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate=GPIO_AF2_USB;
    HAL_GPIO_Init(GPIOA,&gpio);
    HAL_NVIC_SetPriority(USB_IRQn,1u,0u);
    HAL_NVIC_EnableIRQ(USB_IRQn);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd){
    if(!hpcd||hpcd->Instance!=USB)return;
    HAL_NVIC_DisableIRQ(USB_IRQn);
    HAL_GPIO_DeInit(GPIOA,GPIO_PIN_11|GPIO_PIN_12);
    __HAL_RCC_USB_CLK_DISABLE();
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *h){USBD_LL_SetupStage((USBD_HandleTypeDef*)h->pData,(uint8_t*)h->Setup);}
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *h,uint8_t ep){USBD_LL_DataOutStage((USBD_HandleTypeDef*)h->pData,ep,h->OUT_ep[ep].xfer_buff);}
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *h,uint8_t ep){USBD_LL_DataInStage((USBD_HandleTypeDef*)h->pData,ep,h->IN_ep[ep].xfer_buff);}
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *h){USBD_LL_SOF((USBD_HandleTypeDef*)h->pData);}
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *h){USBD_LL_SetSpeed((USBD_HandleTypeDef*)h->pData,USBD_SPEED_FULL);USBD_LL_Reset((USBD_HandleTypeDef*)h->pData);}
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *h){USBD_LL_Suspend((USBD_HandleTypeDef*)h->pData);}
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *h){USBD_LL_Resume((USBD_HandleTypeDef*)h->pData);}
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *h,uint8_t ep){USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef*)h->pData,ep);}
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *h,uint8_t ep){USBD_LL_IsoINIncomplete((USBD_HandleTypeDef*)h->pData,ep);}
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *h){USBD_LL_DevConnected((USBD_HandleTypeDef*)h->pData);}
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *h){USBD_LL_DevDisconnected((USBD_HandleTypeDef*)h->pData);}

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev){
    memset(&usb_pcd,0,sizeof(usb_pcd));
    usb_pcd.Instance=USB;
    usb_pcd.Init.dev_endpoints=8u;
    usb_pcd.Init.speed=PCD_SPEED_FULL;
    usb_pcd.Init.phy_itface=PCD_PHY_EMBEDDED;
    usb_pcd.pData=pdev;pdev->pData=&usb_pcd;
    if(HAL_PCD_Init(&usb_pcd)!=HAL_OK)return USBD_FAIL;
    (void)HAL_PCDEx_PMAConfig(&usb_pcd,0x00u,PCD_SNG_BUF,0x40u);
    (void)HAL_PCDEx_PMAConfig(&usb_pcd,0x80u,PCD_SNG_BUF,0x80u);
    (void)HAL_PCDEx_PMAConfig(&usb_pcd,CDC_IN_EP,PCD_SNG_BUF,0xC0u);
    (void)HAL_PCDEx_PMAConfig(&usb_pcd,CDC_CMD_EP,PCD_SNG_BUF,0x100u);
    (void)HAL_PCDEx_PMAConfig(&usb_pcd,CDC_OUT_EP,PCD_SNG_BUF,0x110u);
    return USBD_OK;
}
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev){return HAL_PCD_DeInit((PCD_HandleTypeDef*)pdev->pData)==HAL_OK?USBD_OK:USBD_FAIL;}
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev){return HAL_PCD_Start((PCD_HandleTypeDef*)pdev->pData)==HAL_OK?USBD_OK:USBD_FAIL;}
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev){return HAL_PCD_Stop((PCD_HandleTypeDef*)pdev->pData)==HAL_OK?USBD_OK:USBD_FAIL;}
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev,uint8_t ep,uint8_t type,uint16_t mps){return HAL_PCD_EP_Open((PCD_HandleTypeDef*)pdev->pData,ep,mps,type)==HAL_OK?USBD_OK:USBD_FAIL;}
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev,uint8_t ep){return HAL_PCD_EP_Close((PCD_HandleTypeDef*)pdev->pData,ep)==HAL_OK?USBD_OK:USBD_FAIL;}
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev,uint8_t ep){return HAL_PCD_EP_Flush((PCD_HandleTypeDef*)pdev->pData,ep)==HAL_OK?USBD_OK:USBD_FAIL;}
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev,uint8_t ep){return HAL_PCD_EP_SetStall((PCD_HandleTypeDef*)pdev->pData,ep)==HAL_OK?USBD_OK:USBD_FAIL;}
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev,uint8_t ep){return HAL_PCD_EP_ClrStall((PCD_HandleTypeDef*)pdev->pData,ep)==HAL_OK?USBD_OK:USBD_FAIL;}
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev,uint8_t ep){PCD_HandleTypeDef *h=(PCD_HandleTypeDef*)pdev->pData;return (ep&0x80u)?h->IN_ep[ep&0x7Fu].is_stall:h->OUT_ep[ep&0x7Fu].is_stall;}
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev,uint8_t addr){return HAL_PCD_SetAddress((PCD_HandleTypeDef*)pdev->pData,addr)==HAL_OK?USBD_OK:USBD_FAIL;}
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev,uint8_t ep,uint8_t *buf,uint16_t size){return HAL_PCD_EP_Transmit((PCD_HandleTypeDef*)pdev->pData,ep,buf,size)==HAL_OK?USBD_OK:USBD_FAIL;}
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev,uint8_t ep,uint8_t *buf,uint16_t size){return HAL_PCD_EP_Receive((PCD_HandleTypeDef*)pdev->pData,ep,buf,size)==HAL_OK?USBD_OK:USBD_FAIL;}
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev,uint8_t ep){return HAL_PCD_EP_GetRxCount((PCD_HandleTypeDef*)pdev->pData,ep);}
void USBD_LL_Delay(uint32_t delay){HAL_Delay(delay);}

bool arx_stm32f072_usb_attach(ArxUsbMode mode,bool shared_led_pin){
    if(mode==ARX_USB_MODE_NONE)return false;
#if defined(ARX_BUILD_C1)
    if(mode==ARX_USB_MODE_LEGACY_MSC)return false;
#endif
    if(usb_attached&&active_mode==mode)return true;
    if(usb_attached&&!arx_stm32f072_usb_detach())return false;

    restore_led_after_detach=shared_led_pin;
    if(shared_led_pin)arx_stm32f072_led_suspend();
    active_mode=mode;

    __HAL_RCC_USB_FORCE_RESET();
    HAL_Delay(2u);
    __HAL_RCC_USB_RELEASE_RESET();
    HAL_Delay(10u);

    memset(&usb_device,0,sizeof(usb_device));
    if(USBD_Init(&usb_device,&descriptors,0u)!=USBD_OK)goto fail;

    if(mode==ARX_USB_MODE_LEGACY_MSC){
        if(USBD_RegisterClass(&usb_device,&USBD_MSC)!=USBD_OK)goto fail_deinit;
        if(USBD_MSC_RegisterStorage(&usb_device,&msc_ops)!=USBD_OK)goto fail_deinit;
    }else{
        if(USBD_RegisterClass(&usb_device,&USBD_CDC)!=USBD_OK)goto fail_deinit;
        if(USBD_CDC_RegisterInterface(&usb_device,&cdc_ops)!=USBD_OK)goto fail_deinit;
    }

    if(USBD_Start(&usb_device)!=USBD_OK)goto fail_deinit;
    usb_attached=true;
    return true;

fail_deinit:
    (void)USBD_DeInit(&usb_device);
fail:
    active_mode=ARX_USB_MODE_NONE;
    if(restore_led_after_detach){arx_stm32f072_led_init();restore_led_after_detach=false;}
    return false;
}

bool arx_stm32f072_usb_detach(void){
    if(!usb_attached){
        if(restore_led_after_detach){arx_stm32f072_led_init();restore_led_after_detach=false;}
        active_mode=ARX_USB_MODE_NONE;
        return true;
    }
    bool ok=true;
    if(usb_device.pData)(void)HAL_PCD_DevDisconnect((PCD_HandleTypeDef*)usb_device.pData);
    if(USBD_Stop(&usb_device)!=USBD_OK)ok=false;
    if(USBD_DeInit(&usb_device)!=USBD_OK)ok=false;
    usb_attached=false;
    active_mode=ARX_USB_MODE_NONE;
    if(restore_led_after_detach){arx_stm32f072_led_init();restore_led_after_detach=false;}
    return ok;
}

bool arx_stm32f072_usb_send(const uint8_t *data,size_t length){
    if(active_mode==ARX_USB_MODE_LEGACY_MSC)return false;
    if(!usb_attached||!data||length==0u||length>sizeof(usb_tx))return false;
    if(usb_device.dev_state!=USBD_STATE_CONFIGURED||!usb_device.pClassData)return false;
    USBD_CDC_HandleTypeDef *cdc=(USBD_CDC_HandleTypeDef*)usb_device.pClassData;
    if(cdc->TxState!=0u)return false;
    memcpy(usb_tx,data,length);
    if(USBD_CDC_SetTxBuffer(&usb_device,usb_tx,(uint32_t)length)!=USBD_OK)return false;
    return USBD_CDC_TransmitPacket(&usb_device)==USBD_OK;
}

bool arx_stm32f072_usb_is_configured(void){
    return usb_attached&&usb_device.dev_state==USBD_STATE_CONFIGURED;
}

void arx_stm32f072_usb_irq(void){HAL_PCD_IRQHandler(&usb_pcd);}

#endif
