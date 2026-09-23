#include "arx/arx_log_export.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct{char data[512];size_t len;bool begun;bool ended;} Mem;
static bool begin(const char *name,void *u){Mem*m=u;m->begun=name&&*name;return m->begun;}
static bool write_cb(const uint8_t*d,size_t n,void*u){Mem*m=u;if(m->len+n>sizeof(m->data))return false;memcpy(m->data+m->len,d,n);m->len+=n;return true;}
static bool end_cb(void*u){((Mem*)u)->ended=true;return true;}
int main(void){
    Mem m={0};ArxLogSink sink={begin,write_cb,end_cb,&m};ArxLogExport l;arx_log_export_init(&l);
    assert(arx_log_export_begin(&l,&sink,"ARX.CSV"));
    ArxCanFrame f={.bus=ARX_BUS_C1,.id=0x123,.dlc=3,.data={0xAA,0xBB,0xCC},.timestamp_ms=42};
    assert(arx_log_export_frame(&l,&sink,&f));assert(l.lines==1u);
    assert(arx_log_export_end(&l,&sink));assert(m.begun&&m.ended);
    m.data[m.len<sizeof(m.data)?m.len:sizeof(m.data)-1]='\0';
    assert(strstr(m.data,"timestamp_ms")&&strstr(m.data,"00000123")&&strstr(m.data,"AABBCC"));
    puts("log export tests: OK");return 0;
}
