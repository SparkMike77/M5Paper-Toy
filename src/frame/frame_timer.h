#ifndef _FRAME_TIMER_H_
#define _FRAME_TIMER_H_

#include "frame_base.h"
#include "../epdgui/epdgui.h"

class Frame_Timer : public Frame_Base {
public:
    Frame_Timer();
    ~Frame_Timer();
    int init(epdgui_args_vector_t &args);
    int run();

private:
    M5EPD_Canvas *_display;
    EPDGUI_Button *_key_startpause;
    EPDGUI_Button *_key_reset;
    EPDGUI_Button *_key_preset[8];
};

#endif //_FRAME_TIMER_H_
