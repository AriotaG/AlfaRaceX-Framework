# STM32F072 replacement target

The first hardware target is the installed three-controller STM32F072 architecture.
AlfaRaceX 1.0 preserves that hardware and its existing memory map; see
`FLASH_MEMORY_REQUIREMENT.md`.

## Roles

| Image | Vehicle bus | Bit rate | Additional responsibility |
|---|---:|---:|---|
| ARX-C1 | C1 | 500 kbit/s | master, pedal UART, slave power/reset control |
| ARX-C2 | C2 | 500 kbit/s | C2 vehicle interface |
| ARX-BH | BH | 125 kbit/s | body/cluster vehicle interface |

The three images share the same portable ARX core and differ only in role/profile.

## Clock

The target runs directly from HSI48 at 48 MHz with AHB and APB1 divided by 1.
The USB peripheral clock is also sourced from HSI48.

## CAN

- PB8: CAN RX
- PB9: CAN TX
- AF4
- SJW: 1 TQ
- BS1: 4 TQ
- BS2: 3 TQ
- prescaler 12: 500 kbit/s
- prescaler 48: 125 kbit/s
- automatic bus-off recovery enabled
- receive filter initially accepts the complete bus

## Inter-controller UART

- USART2
- PA14
- 38,400 baud
- 8-N-1
- half duplex
- open drain with pull-up
- fixed 19-byte blocks
- unused bytes padded with `0x20`

The C1 controller is master and normally sends at most one block every 250 ms.
C2/BH send replies only inside the 200 ms response window opened by a master request.

## Pedal UART

ARX-C1 additionally provides:

- USART1
- PB6
- 9,600 baud
- 8-N-1
- half duplex
- open drain with pull-up

The protocol packet size is 9 bytes.

## Power management

ARX-C1 controls the other two processors and their CAN transceivers:

- PA4: slave reset, active low, open drain
- PA5: slave transceiver sleep, active high, push-pull

After 3,500 ms without CAN traffic, the slave side can be placed into low-consumption
mode unless USB/sniffer/diagnostic activity blocks sleep. New CAN activity within the
3,400 ms wake window releases reset and transceiver sleep and re-arms the inter-chip
UART.

## LEDs and auxiliary outputs

- PA0: red status LED
- PA1: blue status LED
- PA8 / PA7: auxiliary open/close outputs on C1

## WS2812 strip

- PA11 / TIM1 channel 4
- DMA1 channel 4
- 48 MHz timer clock
- period: 60 ticks
- logical 0 high time: 17 ticks
- logical 1 high time: 34 ticks
- 46 LEDs
- 50 reset slots

## Persistent storage compatibility

The deployed hardware uses 2 KiB pages at:

- settings: `0x0801F800`
- statistics: `0x0801F000`
- visible-parameter selection: `0x0801E800`

ARX keeps these addresses isolated behind a storage backend so migration and redundant
storage can be added without leaking fixed addresses into feature code.

## Current target status

The hardware contract and HAL initialization source now exist. Host tests validate
all target constants and protocol/timing assumptions. An actual `.hex/.bin` build
still requires the ARM GCC toolchain and the STM32CubeF0 HAL/CMSIS package.
