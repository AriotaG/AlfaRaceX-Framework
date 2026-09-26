#include "arx/features/arx_elm327.h"
#include <string.h>

static void session_defaults(ArxElm327 *f, bool keep_programmable) {
    if (!f) return;

    const bool enabled = f->enabled;
    const uint32_t commands = f->commands;
    const uint8_t pp2c = keep_programmable ? f->pp2c : 0x00u;
    const uint8_t pp2d = keep_programmable ? f->pp2d : 0x01u;
    const uint8_t pp2e = keep_programmable ? f->pp2e : 0x00u;
    const uint8_t pp2f = keep_programmable ? f->pp2f : 0x01u;

    memset(f,0,sizeof(*f));

    f->enabled = enabled;
    f->commands = commands;

    f->echo = true;
    f->headers = false;
    f->spaces = true;
    f->linefeeds = false;
    f->variable_dlc = false;
    f->auto_format = true;
    f->auto_flow_control = true;
    f->adaptive_timing = 1u;
    f->timeout_ms = ARX_ELM_DEFAULT_TIMEOUT;

    f->protocol = ARX_ELM_PROTOCOL_11_500;
    f->protocol_auto = true;
    f->bitrate_divisor = 1u;
    f->can_priority = 0x18u;

    f->pp2c = pp2c;
    f->pp2d = pp2d ? pp2d : 1u;
    f->pp2e = pp2e;
    f->pp2f = pp2f ? pp2f : 1u;

    f->tx_header = 0x7DFu;
    f->tx_extended = false;

    f->fc_data[0] = 0x30u;
    f->fc_data[1] = 0x00u;
    f->fc_data[2] = 0x00u;
    f->fc_length = 3u;
}

void arx_elm327_init(ArxElm327 *f) {
    if (!f) return;
    memset(f,0,sizeof(*f));
    f->pp2d=1u;
    f->pp2f=1u;
    session_defaults(f,true);
}

static char upper_ascii(char c) {
    if(c>='a'&&c<='z') return (char)(c-('a'-'A'));
    return c;
}

static bool compact_upper(const char *in,char *out,size_t cap) {
    if(!in||!out||cap==0u) return false;
    size_t n=0u;
    while (*in) {
        unsigned char c=(unsigned char)*in++;
        if (c==' '||c=='\r'||c=='\n'||c=='\t') continue;
        if(n+1u>=cap){
            out[0]='\0';
            return false;
        }
        out[n++]=upper_ascii((char)c);
    }
    out[n]='\0';
    return true;
}

static int hexn(char c) {
    if(c>='0'&&c<='9') return c-'0';
    if(c>='A'&&c<='F') return c-'A'+10;
    return -1;
}

static bool parse_hex_u32(const char *s,uint32_t *v) {
    if (!s||!*s||!v) return false;
    uint32_t x=0u;
    size_t digits=0u;
    for(;*s;s++){
        int n=hexn(*s);
        if(n<0 || digits>=8u) return false;
        x=(x<<4u)|(uint32_t)n;
        digits++;
    }
    *v=x;
    return true;
}

static size_t text_copy(char *dst,size_t cap,const char *src) {
    if(!dst||cap==0u) return 0u;
    size_t n=0u;
    if(src){
        while(src[n] && n+1u<cap){dst[n]=src[n];n++;}
    }
    dst[n]='\0';
    return n;
}

static size_t text_append(char *dst,size_t cap,size_t pos,const char *src) {
    if(!dst||cap==0u||pos>=cap) return pos;
    if(!src) return pos;
    while(*src && pos+1u<cap) dst[pos++]=*src++;
    dst[pos]='\0';
    return pos;
}

static size_t append_u16_decimal(char *dst,size_t cap,size_t pos,uint16_t value) {
    char tmp[5];
    size_t n=0u;
    do{
        tmp[n++]=(char)('0'+(value%10u));
        value=(uint16_t)(value/10u);
    }while(value && n<sizeof(tmp));
    while(n>0u && pos+1u<cap) dst[pos++]=tmp[--n];
    if(pos<cap) dst[pos]='\0';
    return pos;
}

static size_t finish(const ArxElm327 *f,const char *body,char *reply,size_t cap) {
    if (!reply||cap==0u) return 0u;
    reply[0]='\0';
    size_t pos=0u;
    pos=text_append(reply,cap,pos,body?body:"");
    pos=text_append(reply,cap,pos,(f&&f->linefeeds)?"\r\n":"\r");
    pos=text_append(reply,cap,pos,(f&&f->linefeeds)?"\r\n":"\r");
    if(pos+1u<cap){ reply[pos++]='>'; reply[pos]='\0'; }
    return pos;
}

static const char *protocol_name(ArxElmProtocol p) {
    switch(p){
        case ARX_ELM_PROTOCOL_11_500:return "ISO 15765-4 (CAN 11/500)";
        case ARX_ELM_PROTOCOL_29_500:return "ISO 15765-4 (CAN 29/500)";
        case ARX_ELM_PROTOCOL_11_250:return "ISO 15765-4 (CAN 11/250)";
        case ARX_ELM_PROTOCOL_29_250:return "ISO 15765-4 (CAN 29/250)";
        case ARX_ELM_PROTOCOL_J1939_250:return "SAE J1939 (CAN 29/250)";
        case ARX_ELM_PROTOCOL_USER1:return "USER1";
        case ARX_ELM_PROTOCOL_USER2:return "USER2";
        default:return "ISO 15765-4 (CAN 11/500)";
    }
}

static char protocol_number(ArxElmProtocol p) {
    if((int)p<=9) return (char)('0'+(int)p);
    if(p==ARX_ELM_PROTOCOL_J1939_250) return 'A';
    if(p==ARX_ELM_PROTOCOL_USER1) return 'B';
    return 'C';
}

static bool set_protocol_number(ArxElm327 *f, char c, bool auto_prefix) {
    if (!f) return false;

    f->protocol_auto=auto_prefix;
    switch(c){
        case '0':
            f->protocol=ARX_ELM_PROTOCOL_11_500;
            f->protocol_auto=true;
            f->bitrate_divisor=1u;
            return true;
        case '6':
            f->protocol=ARX_ELM_PROTOCOL_11_500;
            f->bitrate_divisor=1u;
            return true;
        case '7':
            f->protocol=ARX_ELM_PROTOCOL_29_500;
            f->bitrate_divisor=1u;
            return true;
        case '8':
            f->protocol=ARX_ELM_PROTOCOL_11_250;
            f->bitrate_divisor=2u;
            return true;
        case '9':
            f->protocol=ARX_ELM_PROTOCOL_29_250;
            f->bitrate_divisor=2u;
            return true;
        case 'A':
            f->protocol=ARX_ELM_PROTOCOL_J1939_250;
            f->bitrate_divisor=2u;
            return true;
        case 'B':
            f->protocol=ARX_ELM_PROTOCOL_USER1;
            f->bitrate_divisor=f->pp2d?f->pp2d:1u;
            return true;
        case 'C':
            f->protocol=ARX_ELM_PROTOCOL_USER2;
            f->bitrate_divisor=f->pp2f?f->pp2f:1u;
            return true;
        default:
            return false;
    }
}

static bool parse_wildcard_filter(const char *s,uint32_t *value,uint32_t *mask) {
    if(!s||!value||!mask) return false;
    if(!*s){ *value=0u; *mask=0u; return true; }

    uint32_t v=0u,m=0u;
    for(const char *p=s;*p;p++){
        v<<=4u; m<<=4u;
        if(*p=='X'){
            continue;
        }
        int n=hexn(*p);
        if(n<0) return false;
        v|=(uint32_t)n;
        m|=0x0Fu;
    }
    *value=v; *mask=m;
    return true;
}

uint16_t arx_elm327_effective_bitrate_kbps(const ArxElm327 *f) {
    if(!f) return 500u;
    uint8_t d=f->bitrate_divisor?f->bitrate_divisor:1u;
    return (uint16_t)(500u/d);
}

size_t arx_elm327_command(ArxElm327 *f,const char *command,char *reply,size_t cap) {
    if(!f||!command||!reply||cap==0u) return 0u;
    char cmd[96]={0};
    if(!compact_upper(command,cmd,sizeof(cmd))) return finish(f,"?",reply,cap);
    f->commands++;

    if(strncmp(cmd,"AT",2)!=0) return finish(f,"NO DATA",reply,cap);
    const char *at=cmd+2;
    char body[128]="OK";

    if(!strcmp(at,"Z")) {
        session_defaults(f,true);
        text_copy(body,sizeof(body),ARX_ELM_ID_STRING);
    } else if(!strcmp(at,"WS")) {
        text_copy(body,sizeof(body),ARX_ELM_ID_STRING);
    } else if(!strcmp(at,"I")) {
        text_copy(body,sizeof(body),ARX_ELM_ID_STRING);
    } else if(!strcmp(at,"@1")) {
        text_copy(body,sizeof(body),ARX_ELM_DESCRIPTION);
    } else if(!strcmp(at,"@2")) {
        strcpy(body,"?");
    } else if(!strcmp(at,"?")) {
        strcpy(body,"?");
    } else if(!strcmp(at,"D")) {
        session_defaults(f,true);
        strcpy(body,"OK");
    } else if(!strcmp(at,"E0")) {f->echo=false;}
    else if(!strcmp(at,"E1")) {f->echo=true;}
    else if(!strcmp(at,"L0")) {f->linefeeds=false;}
    else if(!strcmp(at,"L1")) {f->linefeeds=true;}
    else if(!strcmp(at,"H0")) {f->headers=false;}
    else if(!strcmp(at,"H1")) {f->headers=true;}
    else if(!strcmp(at,"S0")) {f->spaces=false;}
    else if(!strcmp(at,"S1")) {f->spaces=true;}
    else if(!strcmp(at,"V0")) {f->variable_dlc=false;}
    else if(!strcmp(at,"V1")) {f->variable_dlc=true;}
    else if(!strcmp(at,"CAF0")) {f->auto_format=false;}
    else if(!strcmp(at,"CAF1")) {f->auto_format=true;}
    else if(!strcmp(at,"CFC0")) {f->auto_flow_control=false;}
    else if(!strcmp(at,"CFC1")) {f->auto_flow_control=true;}
    else if(!strcmp(at,"AL")) {f->allow_long=true;}
    else if(!strcmp(at,"NL")) {f->allow_long=false;}
    else if(!strcmp(at,"AR")||!strcmp(at,"BI")||!strcmp(at,"PC")) {;}
    else if(!strcmp(at,"MA")) {f->monitor_all=true;}
    else if(!strcmp(at,"DP")) {
        if(f->protocol==ARX_ELM_PROTOCOL_USER1 || f->protocol==ARX_ELM_PROTOCOL_USER2){
            size_t pos=text_copy(body,sizeof(body),protocol_name(f->protocol));
            pos=text_append(body,sizeof(body),pos," (CAN ");
            pos=append_u16_decimal(body,sizeof(body),pos,arx_elm327_effective_bitrate_kbps(f));
            (void)text_append(body,sizeof(body),pos,")");
        } else {
            text_copy(body,sizeof(body),protocol_name(f->protocol));
        }
    } else if(!strcmp(at,"DPN")) {
        size_t pos=0u;
        body[0]='\0';
        if(f->protocol_auto && pos+1u<sizeof(body)) body[pos++]='A';
        if(pos+1u<sizeof(body)) body[pos++]=protocol_number(f->protocol);
        body[pos]='\0';
    } else if(!strcmp(at,"RV")) {
        strcpy(body,"12.3V");
    } else if(strlen(at)==3u && at[0]=='A' && at[1]=='T' && at[2]>='0'&&at[2]<='2') {
        f->adaptive_timing=(uint8_t)(at[2]-'0');
    } else if(!strncmp(at,"ST",2)) {
        uint32_t x;
        if(parse_hex_u32(at+2,&x))
            f->timeout_ms=(x==0u)?ARX_ELM_DEFAULT_TIMEOUT:(uint16_t)(x*4u);
        else strcpy(body,"?");
    } else if(!strncmp(at,"CP",2)) {
        uint32_t x;
        if(parse_hex_u32(at+2,&x)) f->can_priority=(uint8_t)x;
        else strcpy(body,"?");
    } else if(!strncmp(at,"CRA",3)) {
        if(!parse_wildcard_filter(at+3,&f->filter_value,&f->filter_mask)) strcpy(body,"?");
    } else if(!strncmp(at,"SH",2)) {
        const char *p=at+2;
        uint32_t x;
        if(parse_hex_u32(p,&x)) {
            size_t n=strlen(p);
            if(n>3u){
                f->tx_header=(n==8u)?x:(((uint32_t)f->can_priority<<24u)|(x&0x00FFFFFFu));
                f->tx_extended=true;
            }else{
                f->tx_header=x;
                f->tx_extended=false;
            }
        } else strcpy(body,"?");
    } else if(!strncmp(at,"FCSH",4)) {
        const char *p=at+4;
        if(!*p){
            f->fc_header=0u; f->fc_extended=false;
        } else {
            uint32_t x;
            if(parse_hex_u32(p,&x)){
                size_t n=strlen(p);
                if(n>3u){
                    f->fc_header=(n==8u)?x:(((uint32_t)f->can_priority<<24u)|(x&0x00FFFFFFu));
                    f->fc_extended=true;
                }else{
                    f->fc_header=x; f->fc_extended=false;
                }
            } else strcpy(body,"?");
        }
    } else if(!strncmp(at,"FCSM",4) && strlen(at)==5u && at[4]>='0'&&at[4]<='2') {
        f->fc_mode=(uint8_t)(at[4]-'0');
    } else if(!strncmp(at,"FCSD",4)) {
        const char *p=at+4; size_t n=strlen(p);
        if((n%2u)==0u && n<=10u) {
            uint8_t temp[5]={0};
            uint8_t count=0u; bool ok=true;
            for(size_t i=0;i<n;i+=2u){
                int a=hexn(p[i]),b=hexn(p[i+1u]);
                if(a<0||b<0){ok=false;break;}
                temp[count++]=(uint8_t)((a<<4)|b);
            }
            if(ok){
                if(count==0u){
                    temp[0]=0x30;temp[1]=0x00;temp[2]=0x00;count=3u;
                }
                memset(f->fc_data,0,sizeof(f->fc_data));
                memcpy(f->fc_data,temp,count);
                f->fc_length=count;
            } else strcpy(body,"?");
        } else strcpy(body,"?");
    } else if(!strncmp(at,"CF",2)) {
        const char *p=at+2;
        if(!*p){f->filter_value=0u;f->filter_mask=0u;}
        else{
            uint32_t x;
            if(parse_hex_u32(p,&x)){
                f->filter_value=x;
                if(f->filter_mask==0u) f->filter_mask=(strlen(p)>3u)?0x1FFFFFFFu:0x7FFu;
            }else strcpy(body,"?");
        }
    } else if(!strncmp(at,"CM",2)) {
        const char *p=at+2;
        if(!*p) f->filter_mask=0u;
        else{
            uint32_t x;
            if(parse_hex_u32(p,&x)) f->filter_mask=x;
            else strcpy(body,"?");
        }
    } else if(!strncmp(at,"PP",2)) {
        const char *p=at+2;
        if(strlen(p)>=2u && hexn(p[0])>=0 && hexn(p[1])>=0){
            uint8_t pn=(uint8_t)((hexn(p[0])<<4)|hexn(p[1]));
            const char *rest=p+2;
            if(!strncmp(rest,"SV",2) && strlen(rest)>=4u &&
               hexn(rest[2])>=0 && hexn(rest[3])>=0){
                uint8_t v=(uint8_t)((hexn(rest[2])<<4)|hexn(rest[3]));
                switch(pn){
                    case 0x2C:f->pp2c=v;break;
                    case 0x2D:f->pp2d=v?v:1u;break;
                    case 0x2E:f->pp2e=v;break;
                    case 0x2F:f->pp2f=v?v:1u;break;
                    default:break;
                }
            }
        }
    } else if((!strncmp(at,"SP",2)||!strncmp(at,"TP",2)) && strlen(at)>=3u) {
        const char *p=at+2;
        bool automatic=false;
        if(*p=='A'){automatic=true;p++;}
        if(strlen(p)==1u && set_protocol_number(f,*p,automatic)) {;}
        else strcpy(body,"?");
    } else if(!strncmp(at,"IB",2)||!strncmp(at,"FC",2)||!strncmp(at,"BRT",3)) {
        ;
    } else if(!strncmp(at,"BRD",3)) {
        /* USB CDC baud is virtual; transport performs the formal handshake. */
        strcpy(body,"OK");
    } else {
        /* Compatibility default: unknown AT extensions are accepted. */
        strcpy(body,"OK");
    }

    return finish(f,body,reply,cap);
}

bool arx_elm327_prepare_request(const ArxElm327 *f,const char *hex,ArxElmRequest *r) {
    if(!f||!hex||!r) return false;
    /* Validate before touching the output, without a 511-byte stack copy. */
    size_t n=0u;
    for(const char *p=hex;*p;p++){
        const char c=*p;
        if(c==' '||c=='\r'||c=='\n'||c=='\t')continue;
        if(hexn(upper_ascii(c))<0||++n>ARX_ELM_MAX_PAYLOAD*2u)return false;
    }
    if(n==0u||(n%2u)!=0u||(!f->auto_format&&n>16u))return false;

    memset(r,0,sizeof(*r));
    r->can_id=f->tx_header;
    r->extended_id=f->tx_extended;
    r->auto_format=f->auto_format;
    r->auto_flow_control=f->auto_flow_control;

    int high=-1;
    for(const char *p=hex;*p;p++){
        const char c=*p;
        if(c==' '||c=='\r'||c=='\n'||c=='\t')continue;
        const int digit=hexn(upper_ascii(c));
        if(high<0)high=digit;
        else{
            r->data[r->length++]=(uint8_t)((high<<4)|digit);
            high=-1;
        }
    }
    return true;
}

bool arx_elm327_filter_accept(const ArxElm327 *f,uint32_t id) {
    if(!f) return false;
    if(f->filter_mask==0u) return true;
    return (id&f->filter_mask)==(f->filter_value&f->filter_mask);
}
