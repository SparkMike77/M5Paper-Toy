#ifndef _FRAME_MAIN_H_
#define _FRAME_MAIN_H_

#include "frame_base.h"

class Frame_Main : public Frame_Base {
public:
    Frame_Main();
    ~Frame_Main();
    int run();
    int init(epdgui_args_vector_t &args);
    void StatusBar(m5epd_update_mode_t mode);
    void AppName(m5epd_update_mode_t mode);

private:
    EPDGUI_Button *_key[12];
    M5EPD_Canvas *_bar;
    M5EPD_Canvas *_names;
    uint32_t _next_update_time;
    uint32_t _time;
    uint32_t _last_battery_voltage = 0;
    bool _is_charging = false;
    // 0xFF is not a reachable percentage (0-100), so the first StatusBar()
    // call after each frame entry always redraws regardless of what the
    // battery was last showing.
    uint8_t _last_battery_pct = 0xFF;
};

#endif //_FRAME_MAIN_H_