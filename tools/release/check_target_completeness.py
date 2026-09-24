#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[2]
TARGET=ROOT/'targets'/'stm32f072_common'
required=[
    TARGET/'src'/'arx_stm32f072_app.c',
    TARGET/'src'/'arx_stm32f072_hal.c',
    TARGET/'src'/'arx_stm32f072_irq.c',
    TARGET/'src'/'arx_stm32f072_led.c',
    TARGET/'src'/'arx_stm32f072_log.c',
    TARGET/'src'/'arx_stm32f072_storage.c',
    TARGET/'src'/'arx_stm32f072_usb.c',
]
errors=[]
for path in required:
    if not path.is_file(): errors.append(f'missing target source: {path.relative_to(ROOT)}')

for path in TARGET.rglob('*'):
    if path.is_file() and path.suffix in {'.c','.h','.ld'}:
        text=path.read_text(errors='ignore')
        if '__attribute__((weak))' in text:
            errors.append(f'production weak binding found: {path.relative_to(ROOT)}')

for ld in (TARGET/'linker').glob('*.ld'):
    text=ld.read_text(errors='ignore')
    if 'libgcc.a(*)' in text or 'libc.a(*)' in text:
        errors.append(f'linker discards runtime library: {ld.relative_to(ROOT)}')

cmake=(TARGET/'CMakeLists.txt').read_text(errors='ignore')
for name in ('arx_stm32f072_usb.c','arx_stm32f072_log.c','usbd_cdc.c','usbd_msc.c','usbd_msc_bot.c','usbd_msc_scsi.c','--gc-sections','check_firmware_size.py'):
    if name not in cmake: errors.append(f'target build omits required item: {name}')

runtime=(ROOT/'src'/'core'/'arx_runtime.c').read_text(errors='ignore')
for name in ('arx_runtime_usb_rx','ARX_LINK_TO_C2','ARX_LINK_TO_BH','ARX_USB_MODE_LEGACY_MSC'):
    if name not in runtime: errors.append(f'runtime omits RC5 target binding: {name}')

if errors:
    for e in errors: print(f'FAIL: {e}')
    sys.exit(1)
print('target completeness gate: PASS')
