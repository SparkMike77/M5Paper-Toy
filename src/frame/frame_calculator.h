#ifndef _FRAME_CALCULATOR_H_
#define _FRAME_CALCULATOR_H_

#include "frame_base.h"
#include "../epdgui/epdgui.h"

class Frame_Calculator : public Frame_Base {
public:
    Frame_Calculator();
    ~Frame_Calculator();
    int init(epdgui_args_vector_t &args);

private:
    M5EPD_Canvas *_display;
    EPDGUI_Button *_key[18];
};

#endif //_FRAME_CALCULATOR_H_
