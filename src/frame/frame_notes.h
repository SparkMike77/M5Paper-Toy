#ifndef _FRAME_NOTES_H_
#define _FRAME_NOTES_H_

#include "frame_base.h"
#include "../epdgui/epdgui.h"

class Frame_Notes : public Frame_Base {
public:
    Frame_Notes();
    ~Frame_Notes();
    int init(epdgui_args_vector_t &args);
    int run();
    void exit();

private:
    EPDGUI_Button *_key_slot[5];
    EPDGUI_Button *_key_save;
    EPDGUI_Button *_key_clear;
};

#endif //_FRAME_NOTES_H_
