#ifndef ARX_ODOMETER_H
#define ARX_ODOMETER_H

#include "arx/arx_can.h"
#include <stdbool.h>

bool arx_odometer_build_no_blink(const ArxCanFrame *source_356, ArxCanFrame *out);

#endif
