#include "arx/arx_log_export.h"
#include <stdio.h>
#include <string.h>

void arx_log_export_init(ArxLogExport *l){if(l)memset(l,0,sizeof(*l));}

bool arx_log_export_begin(ArxLogExport *l,const ArxLogSink *s,const char *name){
    if(!l||!s||!s->begin||!s->write||!s->end)return false;
    if(!s->begin(name?name:"ARX_LOG.CSV",s->user))return false;
    static const char hdr[]="timestamp_ms,bus,id,ext,dlc,data\r\n";
    if(!s->write((const uint8_t*)hdr,sizeof(hdr)-1u,s->user)){s->end(s->user);return false;}
    l->active=true;l->lines=0;l->failed_writes=0;return true;
}

bool arx_log_export_frame(ArxLogExport *l,const ArxLogSink *s,const ArxCanFrame *f){
    if(!l||!s||!f||!l->active||!s->write)return false;
    char line[ARX_LOG_LINE_MAX];
    const char *bus=f->bus==ARX_BUS_C1?"C1":f->bus==ARX_BUS_C2?"C2":"BH";
    int n=snprintf(line,sizeof(line),"%lu,%s,%08lX,%u,%u,",
        (unsigned long)f->timestamp_ms,bus,(unsigned long)f->id,
        f->extended_id?1u:0u,f->dlc);
    if(n<0||(size_t)n>=sizeof(line))return false;
    size_t pos=(size_t)n;
    for(uint8_t i=0;i<f->dlc&&i<8u;i++){
        int k=snprintf(&line[pos],sizeof(line)-pos,"%02X",f->data[i]);
        if(k<0||(size_t)k>=sizeof(line)-pos) return false;
        pos+=(size_t)k;
    }
    if(pos+2u>=sizeof(line))return false;
    line[pos++]='\r';line[pos++]='\n';
    if(!s->write((const uint8_t*)line,pos,s->user)){l->failed_writes++;return false;}
    l->lines++;return true;
}

bool arx_log_export_end(ArxLogExport *l,const ArxLogSink *s){
    if(!l||!s||!l->active||!s->end)return false;
    bool ok=s->end(s->user);l->active=false;return ok;
}
