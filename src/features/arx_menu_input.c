#include "arx/features/arx_menu_input.h"
#include <string.h>

void arx_menu_input_init(ArxMenuInput *i) {
    if(!i)return;
    memset(i,0,sizeof(*i));
    i->last_button=0x10u;
}

static uint8_t wrap_prev(uint8_t v,uint8_t count) {
    if(!count)return 0u;
    return v==0u?(uint8_t)(count-1u):(uint8_t)(v-1u);
}

static void sub_next(ArxMenuState *m,uint8_t step,uint8_t pc,uint8_t sc) {
    if(m->main_page==1u){
        uint16_t n=(uint16_t)m->param_page+step;
        m->param_page=(n>=pc)?0u:(uint8_t)n;
    }else if(m->main_page==9u){
        if(step==1u) arx_menu_next_setup(m,1u);
        else { uint16_t n=(uint16_t)m->setup_page+step; m->setup_page=(n>=ARX_SETUP_MENU_COUNT)?0u:(uint8_t)n; }
    }else if(m->main_page==10u){
        uint16_t n=(uint16_t)m->param_page+step;
        m->param_page=(n>sc)?0u:(uint8_t)n;
    }
}

static void sub_prev(ArxMenuState *m,uint8_t step,uint8_t pc,uint8_t sc) {
    if(m->main_page==1u){
        if(step==1u)m->param_page=wrap_prev(m->param_page,pc);
        else m->param_page=(m->param_page<step)?0u:(uint8_t)(m->param_page-step);
    }else if(m->main_page==9u){
        if(step==1u)arx_menu_prev_setup(m,1u);
        else m->setup_page=(m->setup_page<step)?0u:(uint8_t)(m->setup_page-step);
    }else if(m->main_page==10u){
        if(step==1u)m->param_page=(m->param_page==0u)?sc:(uint8_t)(m->param_page-1u);
        else m->param_page=(m->param_page<step)?0u:(uint8_t)(m->param_page-step);
    }
}

ArxMenuInputEvent arx_menu_input_on_button(
    ArxMenuInput *i,ArxMenuState *m,const ArxMenuCapabilities *caps,
    uint8_t button,uint8_t parameter_page_count,uint8_t parameter_setup_page_count
) {
    if(!i||!m)return ARX_MENU_INPUT_NONE;

    switch(button){
        case 0x18u:
            if(i->last_button==0x10u&&m->visible&&m->commands_enabled){
                i->last_button=0x18u;
                if(m->level==ARX_MENU_LEVEL_MAIN)arx_menu_next_main(m,caps,1u);
                else sub_next(m,1u,parameter_page_count,parameter_setup_page_count);
                return ARX_MENU_INPUT_RENDER;
            }
            break;
        case 0x20u:
            if(i->last_button==0x18u&&m->visible&&m->commands_enabled){
                i->last_button=0x20u;
                if(m->level==ARX_MENU_LEVEL_MAIN)arx_menu_next_main(m,caps,1u);
                else sub_next(m,10u,parameter_page_count,parameter_setup_page_count);
                return ARX_MENU_INPUT_RENDER;
            }
            break;
        case 0x08u:
            if(i->last_button==0x10u&&m->visible&&m->commands_enabled){
                i->last_button=0x08u;
                if(m->level==ARX_MENU_LEVEL_MAIN)arx_menu_prev_main(m,caps,1u);
                else sub_prev(m,1u,parameter_page_count,parameter_setup_page_count);
                return ARX_MENU_INPUT_RENDER;
            }
            break;
        case 0x00u:
            if(i->last_button==0x08u&&m->visible&&m->commands_enabled){
                i->last_button=0x00u;
                if(m->level==ARX_MENU_LEVEL_MAIN)arx_menu_prev_main(m,caps,1u);
                else sub_prev(m,10u,parameter_page_count,parameter_setup_page_count);
                return ARX_MENU_INPUT_RENDER;
            }
            break;
        case 0x90u:
        case 0x50u:
            if(i->last_button==0x10u)i->last_button=0x89u;
            if(i->last_button==0x89u){
                if(i->res_hold_frames<0xFFFFu)i->res_hold_frames++;
                if(i->res_hold_frames>50u){
                    i->last_button=0x90u;
                    i->res_hold_frames=0u;
                    m->visible=!m->visible;
                    return ARX_MENU_INPUT_VISIBILITY_CHANGED;
                }
            }
            break;
        case 0x10u:{
            const bool short_res=(i->last_button==0x89u&&m->visible);
            i->last_button=0x10u;
            i->res_hold_frames=0u;
            if(short_res)return ARX_MENU_INPUT_ACTIVATE;
            break;
        }
        default:break;
    }
    return ARX_MENU_INPUT_NONE;
}
