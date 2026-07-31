#include "frame_homeassistant_config.h"

namespace {
void key_homeassistant_save_cb(epdgui_args_vector_t &args) {
    Frame_HomeAssistantConfig* frame = (Frame_HomeAssistantConfig*)(args[0]);
    frame->init(args);
    EPDGUI_PopFrame(true);
    *((int*)(args[1])) = 0;
}

void key_homeassistant_cancel_cb(epdgui_args_vector_t &args) {
    EPDGUI_PopFrame(true);
    *((int*)(args[0])) = 0;
}

void key_homeassistant_focus_cb(epdgui_args_vector_t &args) {
    EPDGUI_Textbox* box = (EPDGUI_Textbox*)(args[0]);
    box->SetState(EPDGUI_Textbox::EVENT_PRESSED);
}
}

Frame_HomeAssistantConfig::Frame_HomeAssistantConfig() : Frame_Base() {
    _frame_name = "Frame_HomeAssistantConfig";
    _focus_index = 0;

    _url_box = new EPDGUI_Textbox(4, 100, 532, 60);
    _token_box = new EPDGUI_Textbox(4, 190, 532, 60);
    _entity_boxes[0] = new EPDGUI_Textbox(4, 280, 532, 44);
    _entity_boxes[1] = new EPDGUI_Textbox(4, 340, 532, 44);
    _entity_boxes[2] = new EPDGUI_Textbox(4, 400, 532, 44);
    _entity_boxes[3] = new EPDGUI_Textbox(4, 460, 532, 44);
    _keyboard = new EPDGUI_Keyboard();
    _key_save = new EPDGUI_Button("Save", 4, 620, 260, 52);
    _key_cancel = new EPDGUI_Button("Cancel", 280, 620, 256, 52);
    _active_box = _url_box;

    for (int i = 0; i < 4; i++) {
        _entity_boxes[i]->SetTextSize(20);
    }

    _url_box->SetText(GetHomeAssistantURL());
    _token_box->SetText(GetHomeAssistantToken());
    for (int i = 0; i < 4; i++) {
        _entity_boxes[i]->SetText(GetHomeAssistantEntity(i));
    }

    exitbtn("Home");
    _canvas_title->drawString("Home Assistant", 270, 34);
    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, &Frame_Base::exit_cb);

    _key_save->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_save->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)(&_is_run));
    _key_save->Bind(EPDGUI_Button::EVENT_RELEASED, key_homeassistant_save_cb);
    _key_cancel->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_cancel->Bind(EPDGUI_Button::EVENT_RELEASED, key_homeassistant_cancel_cb);
}

Frame_HomeAssistantConfig::~Frame_HomeAssistantConfig() {
    delete _url_box;
    delete _token_box;
    for (int i = 0; i < 4; i++) {
        delete _entity_boxes[i];
    }
    delete _keyboard;
    delete _key_save;
    delete _key_cancel;
}

int Frame_HomeAssistantConfig::init(epdgui_args_vector_t &args) {
    _is_run = 1;
    if (args.size() > 0) {
        SetHomeAssistantConfig(_url_box->GetText(), _token_box->GetText());
        for (int i = 0; i < 4; i++) {
            SetHomeAssistantEntity(i, _entity_boxes[i]->GetText());
        }
    }
    M5.EPD.Clear();
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);
    EPDGUI_AddObject(_url_box);
    EPDGUI_AddObject(_token_box);
    for (int i = 0; i < 4; i++) {
        EPDGUI_AddObject(_entity_boxes[i]);
    }
    EPDGUI_AddObject(_keyboard);
    EPDGUI_AddObject(_key_save);
    EPDGUI_AddObject(_key_cancel);
    EPDGUI_AddObject(_key_exit);
    return 3;
}

int Frame_HomeAssistantConfig::run(void) {
    String data = _keyboard->getData();
    if (!data.isEmpty()) {
        _active_box->SetText(_active_box->GetText() + data);
        _keyboard->Draw(UPDATE_MODE_NONE);
    }
    return 1;
}
