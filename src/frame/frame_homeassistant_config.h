#ifndef _FRAME_HOMEASSISTANT_CONFIG_H_
#define _FRAME_HOMEASSISTANT_CONFIG_H_

#include "frame_base.h"
#include "../epdgui/epdgui.h"

class Frame_HomeAssistantConfig : public Frame_Base {
public:
    Frame_HomeAssistantConfig();
    ~Frame_HomeAssistantConfig();
    int init(epdgui_args_vector_t &args);
    int run(void);

private:
    EPDGUI_Textbox *_url_box;
    EPDGUI_Textbox *_token_box;
    EPDGUI_Textbox *_entity_boxes[4];
    EPDGUI_Keyboard *_keyboard;
    EPDGUI_Button *_key_save;
    EPDGUI_Button *_key_cancel;
    EPDGUI_Textbox *_active_box;
    uint16_t _focus_index;
};

#endif
