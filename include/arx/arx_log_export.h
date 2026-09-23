#ifndef ARX_LOG_EXPORT_H
#define ARX_LOG_EXPORT_H

#include "arx/arx_can.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ARX_LOG_LINE_MAX 96u

typedef struct {
    bool (*begin)(const char *name,void *user);
    bool (*write)(const uint8_t *data,size_t length,void *user);
    bool (*end)(void *user);
    void *user;
} ArxLogSink;

typedef struct {
    bool active;
    uint32_t lines;
    uint32_t failed_writes;
} ArxLogExport;

void arx_log_export_init(ArxLogExport *log);
bool arx_log_export_begin(ArxLogExport *log,const ArxLogSink *sink,const char *name);
bool arx_log_export_frame(ArxLogExport *log,const ArxLogSink *sink,const ArxCanFrame *frame);
bool arx_log_export_end(ArxLogExport *log,const ArxLogSink *sink);

#endif
