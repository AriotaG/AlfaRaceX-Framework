cmake_minimum_required(VERSION 3.16)
project(AlfaRaceX_STM32F072 C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(NOT DEFINED STM32CUBE_F0_ROOT)
  message(FATAL_ERROR "Set STM32CUBE_F0_ROOT to a STM32CubeF0 package")
endif()

if(NOT DEFINED ARX_ROLE)
  set(ARX_ROLE "C1")
endif()

set(CPU_DEFS STM32F072xB ARX_TARGET_STM32F072)
option(ARX_SAFE_BENCH_START "Disable active vehicle functions on every boot, including imported settings" ON)
if(ARX_SAFE_BENCH_START)
  list(APPEND CPU_DEFS ARX_SAFE_BENCH_START=1)
endif()

if(ARX_ROLE STREQUAL "C1")
  list(APPEND CPU_DEFS ARX_IMAGE_ROLE=ARX_TARGET_ROLE_C1 ARX_BUILD_C1=1)
  set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/linker/arx_c1_128k_compat.ld)
elseif(ARX_ROLE STREQUAL "C2")
  list(APPEND CPU_DEFS ARX_IMAGE_ROLE=ARX_TARGET_ROLE_C2 ARX_BUILD_C2=1)
  set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/linker/arx_c2_bh_128k_compat.ld)
elseif(ARX_ROLE STREQUAL "BH")
  list(APPEND CPU_DEFS ARX_IMAGE_ROLE=ARX_TARGET_ROLE_BH ARX_BUILD_BH=1)
  set(LINKER_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/linker/arx_c2_bh_128k_compat.ld)
else()
  message(FATAL_ERROR "ARX_ROLE must be C1, C2 or BH")
endif()

set(ARX_ROOT ${CMAKE_CURRENT_LIST_DIR}/../..)
file(READ "${ARX_ROOT}/VERSION" ARX_FIRMWARE_VERSION_VALUE)
string(STRIP "${ARX_FIRMWARE_VERSION_VALUE}" ARX_FIRMWARE_VERSION_VALUE)

file(GLOB ARX_CORE
  ${ARX_ROOT}/src/core/*.c
  ${ARX_ROOT}/src/vehicle/*.c
  ${ARX_ROOT}/src/features/*.c
)

set(HAL_DIR ${STM32CUBE_F0_ROOT}/Drivers/STM32F0xx_HAL_Driver)
set(CMSIS_DIR ${STM32CUBE_F0_ROOT}/Drivers/CMSIS)
set(USB_DEVICE_DIR ${STM32CUBE_F0_ROOT}/Middlewares/ST/STM32_USB_Device_Library)

set(SYSTEM_SRC
  ${CMSIS_DIR}/Device/ST/STM32F0xx/Source/Templates/system_stm32f0xx.c
  ${CMSIS_DIR}/Device/ST/STM32F0xx/Source/Templates/gcc/startup_stm32f072xb.s
)

set(HAL_SRC
  ${HAL_DIR}/Src/stm32f0xx_hal.c
  ${HAL_DIR}/Src/stm32f0xx_hal_can.c
  ${HAL_DIR}/Src/stm32f0xx_hal_cortex.c
  ${HAL_DIR}/Src/stm32f0xx_hal_dma.c
  ${HAL_DIR}/Src/stm32f0xx_hal_flash.c
  ${HAL_DIR}/Src/stm32f0xx_hal_flash_ex.c
  ${HAL_DIR}/Src/stm32f0xx_hal_gpio.c
  ${HAL_DIR}/Src/stm32f0xx_hal_pcd.c
  ${HAL_DIR}/Src/stm32f0xx_hal_pcd_ex.c
  ${HAL_DIR}/Src/stm32f0xx_hal_rcc.c
  ${HAL_DIR}/Src/stm32f0xx_hal_rcc_ex.c
  ${HAL_DIR}/Src/stm32f0xx_hal_tim.c
  ${HAL_DIR}/Src/stm32f0xx_hal_tim_ex.c
  ${HAL_DIR}/Src/stm32f0xx_hal_uart.c
  ${HAL_DIR}/Src/stm32f0xx_ll_usb.c
)

set(USB_DEVICE_SRC
  ${USB_DEVICE_DIR}/Core/Src/usbd_core.c
  ${USB_DEVICE_DIR}/Core/Src/usbd_ctlreq.c
  ${USB_DEVICE_DIR}/Core/Src/usbd_ioreq.c
  ${USB_DEVICE_DIR}/Class/CDC/Src/usbd_cdc.c
  ${USB_DEVICE_DIR}/Class/MSC/Src/usbd_msc.c
  ${USB_DEVICE_DIR}/Class/MSC/Src/usbd_msc_bot.c
  ${USB_DEVICE_DIR}/Class/MSC/Src/usbd_msc_scsi.c
  ${USB_DEVICE_DIR}/Class/MSC/Src/usbd_msc_data.c
)

add_executable(arx_${ARX_ROLE}.elf
  ${ARX_CORE}
  src/arx_stm32f072_target.c
  src/arx_stm32f072_hal.c
  src/arx_stm32f072_storage.c
  src/arx_stm32f072_log.c
  src/arx_stm32f072_led.c
  src/arx_stm32f072_usb.c
  src/arx_stm32f072_irq.c
  src/arx_stm32f072_app.c
  src/main.c
  ${SYSTEM_SRC}
  ${HAL_SRC}
  ${USB_DEVICE_SRC}
)

target_compile_definitions(arx_${ARX_ROLE}.elf PRIVATE
  ${CPU_DEFS}
  USE_HAL_DRIVER
  ARX_FIRMWARE_VERSION="${ARX_FIRMWARE_VERSION_VALUE}"
)
target_compile_options(arx_${ARX_ROLE}.elf PRIVATE
  -Os -ffunction-sections -fdata-sections -Wall -Wextra -Wpedantic -Werror
)

# Vendor STM32CubeF0 sources are built as provided by ST. AlfaRaceX sources retain
# -Werror; vendor warnings must not make an ARX release fail.
set_source_files_properties(
  ${SYSTEM_SRC} ${HAL_SRC} ${USB_DEVICE_SRC}
  PROPERTIES COMPILE_OPTIONS "-Wno-error"
)

target_include_directories(arx_${ARX_ROLE}.elf PRIVATE
  ${ARX_ROOT}/include
  include
  ${HAL_DIR}/Inc
  ${HAL_DIR}/Inc/Legacy
  ${CMSIS_DIR}/Include
  ${CMSIS_DIR}/Device/ST/STM32F0xx/Include
  ${USB_DEVICE_DIR}/Core/Inc
  ${USB_DEVICE_DIR}/Class/CDC/Inc
  ${USB_DEVICE_DIR}/Class/MSC/Inc
)

target_link_options(arx_${ARX_ROLE}.elf PRIVATE
  -T${LINKER_SCRIPT}
  -Wl,-Map=arx_${ARX_ROLE}.map
  -Wl,--gc-sections
)

add_custom_command(TARGET arx_${ARX_ROLE}.elf POST_BUILD
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:arx_${ARX_ROLE}.elf>
  COMMAND ${Python3_EXECUTABLE}
          ${ARX_ROOT}/tools/release/check_firmware_size.py
          --role ${ARX_ROLE}
          --elf $<TARGET_FILE:arx_${ARX_ROLE}.elf>
          --size-tool ${CMAKE_SIZE}
          --min-flash-headroom 2048
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:arx_${ARX_ROLE}.elf> arx_${ARX_ROLE}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:arx_${ARX_ROLE}.elf> arx_${ARX_ROLE}.bin
)
