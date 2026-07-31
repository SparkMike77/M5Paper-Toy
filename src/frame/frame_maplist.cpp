#include "frame_maplist.h"
#include "frame_mapreader.h"

static const char *kMapsRoot = "/maps";
#define ROW_H 61
#define ROW_Y0 100

static void key_maplist_exit_cb(epdgui_args_vector_t &args) {
    EPDGUI_PopFrame(true);
    *((int*)(args[0])) = 0;
}

static void key_maplist_open_cb(epdgui_args_vector_t &args) {
    Frame_Base *frame = new Frame_MapReader(((EPDGUI_Button*)(args[0]))->GetCustomString());
    EPDGUI_PushFrame(frame);
    *((int*)(args[1])) = 0;
}

Frame_MapList::Frame_MapList() {
    _frame_name = "Frame_MapList";

    exitbtn("Back");
    _canvas_title->drawString("Maps", 270, 34);

    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, key_maplist_exit_cb);
}

Frame_MapList::~Frame_MapList(void) {
    for (size_t i = 0; i < _key_maps.size(); i++) {
        delete _key_maps[i];
    }
}

void Frame_MapList::BuildList() {
    File dir = SD.open(kMapsRoot);
    if (!dir || !dir.isDirectory()) {
        EPDGUI_Button *btn = new EPDGUI_Button(4, ROW_Y0, 532, ROW_H - 4);
        btn->CanvasNormal()->fillCanvas(0);
        btn->CanvasNormal()->setTextSize(26);
        btn->CanvasNormal()->setTextDatum(CL_DATUM);
        btn->CanvasNormal()->setTextColor(15);
        btn->CanvasNormal()->drawString("No /maps folder found on the SD card.", 15, (ROW_H - 4) / 2);
        btn->SetEnable(false);
        _key_maps.push_back(btn);
        return;
    }

    File entry = dir.openNextFile();
    while (entry) {
        if (entry.isDirectory()) {
            String name(entry.name());
            int slash = name.lastIndexOf('/');
            if (slash >= 0) {
                name = name.substring(slash + 1);
            }

            int row = (int)_key_maps.size();
            EPDGUI_Button *btn = new EPDGUI_Button(4, ROW_Y0 + row * ROW_H, 532, ROW_H - 4);
            btn->CanvasNormal()->fillCanvas(0);
            btn->CanvasNormal()->drawRect(0, 0, 532, ROW_H - 4, 15);
            btn->CanvasNormal()->setTextSize(26);
            btn->CanvasNormal()->setTextDatum(CL_DATUM);
            btn->CanvasNormal()->setTextColor(15);
            btn->CanvasNormal()->drawString(name, 15, (ROW_H - 4) / 2);
            *(btn->CanvasPressed()) = *(btn->CanvasNormal());
            btn->CanvasPressed()->ReverseColor();

            btn->SetCustomString(String(kMapsRoot) + "/" + name);
            btn->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, btn);
            btn->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)(&_is_run));
            btn->Bind(EPDGUI_Button::EVENT_RELEASED, key_maplist_open_cb);

            _key_maps.push_back(btn);
        }
        entry = dir.openNextFile();
    }

    if (_key_maps.size() == 0) {
        EPDGUI_Button *btn = new EPDGUI_Button(4, ROW_Y0, 532, ROW_H - 4);
        btn->CanvasNormal()->fillCanvas(0);
        btn->CanvasNormal()->setTextSize(26);
        btn->CanvasNormal()->setTextDatum(CL_DATUM);
        btn->CanvasNormal()->setTextColor(15);
        btn->CanvasNormal()->drawString("No map folders found under /maps.", 15, (ROW_H - 4) / 2);
        btn->SetEnable(false);
        _key_maps.push_back(btn);
    }
}

int Frame_MapList::init(epdgui_args_vector_t &args) {
    _is_run = 1;
    if (!_scanned) {
        _scanned = true;
        BuildList();
    }

    M5.EPD.Clear();
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);
    EPDGUI_AddObject(_key_exit);
    for (size_t i = 0; i < _key_maps.size(); i++) {
        EPDGUI_AddObject(_key_maps[i]);
    }
    return 3;
}
