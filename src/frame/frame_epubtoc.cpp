#include "frame_epubtoc.h"
#include "frame_epubreader.h"

#define MAX_PER_PAGE 12
#define ROW_H 61
#define ROW_Y0 100

static void key_epubtoc_select_cb(epdgui_args_vector_t &args) {
    Frame_EpubReader *reader = (Frame_EpubReader*)(args[0]);
    int index = (int)((intptr_t)(args[1]));
    reader->RequestChapterJump(index);
    EPDGUI_PopFrame(true);
    *((int*)(args[2])) = 0;
}

static void key_epubtoc_page_cb(epdgui_args_vector_t &args) {
    Frame_EpubReader *reader = (Frame_EpubReader*)(args[0]);
    int newPage = (int)((intptr_t)(args[1]));
    Frame_Base *frame = new Frame_EpubToc(reader, newPage);
    EPDGUI_PopFrame(true);
    EPDGUI_PushFrame(frame);
    *((int*)(args[2])) = 0;
}

static void key_epubtoc_exit_cb(epdgui_args_vector_t &args) {
    EPDGUI_PopFrame(true);
    *((int*)(args[0])) = 0;
}

Frame_EpubToc::Frame_EpubToc(Frame_EpubReader *reader, int page) {
    _frame_name = "Frame_EpubToc";
    _reader = reader;
    _toc_page = page;

    exitbtn("Back");
    _canvas_title->drawString("Table of Contents", 270, 34);

    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, key_epubtoc_exit_cb);

    BuildList();
}

Frame_EpubToc::~Frame_EpubToc(void) {
    for (size_t i = 0; i < _key_chapters.size(); i++) {
        delete _key_chapters[i];
    }
    if (_key_toc_prev != NULL) delete _key_toc_prev;
    if (_key_toc_next != NULL) delete _key_toc_next;
}

void Frame_EpubToc::BuildList(void) {
    int total = _reader->GetChapterCount();
    int start = _toc_page * MAX_PER_PAGE;
    int end = start + MAX_PER_PAGE;
    if (end > total) {
        end = total;
    }

    for (int i = start; i < end; i++) {
        int row = i - start;
        EPDGUI_Button *btn = new EPDGUI_Button(4, ROW_Y0 + row * ROW_H, 532, ROW_H - 4);
        _key_chapters.push_back(btn);

        String label = String(i + 1) + ". " + _reader->GetChapterTitle(i);
        if (label.length() > 40) {
            label = label.substring(0, 40) + "...";
        }
        btn->CanvasNormal()->fillCanvas(0);
        btn->CanvasNormal()->drawRect(0, 0, 532, ROW_H - 4, 15);
        btn->CanvasNormal()->setTextSize(26);
        btn->CanvasNormal()->setTextDatum(CL_DATUM);
        btn->CanvasNormal()->setTextColor(15);
        btn->CanvasNormal()->drawString(label, 15, (ROW_H - 4) / 2);
        *(btn->CanvasPressed()) = *(btn->CanvasNormal());
        btn->CanvasPressed()->ReverseColor();

        btn->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)_reader);
        btn->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)(intptr_t)i);
        btn->AddArgs(EPDGUI_Button::EVENT_RELEASED, 2, (void*)(&_is_run));
        btn->Bind(EPDGUI_Button::EVENT_RELEASED, key_epubtoc_select_cb);
    }

    int bottomY = ROW_Y0 + MAX_PER_PAGE * ROW_H + 10;
    if (start > 0) {
        _key_toc_prev = new EPDGUI_Button("< Prev", 4, bottomY, 260, 60);
        _key_toc_prev->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)_reader);
        _key_toc_prev->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)(intptr_t)(_toc_page - 1));
        _key_toc_prev->AddArgs(EPDGUI_Button::EVENT_RELEASED, 2, (void*)(&_is_run));
        _key_toc_prev->Bind(EPDGUI_Button::EVENT_RELEASED, key_epubtoc_page_cb);
    }
    if (end < total) {
        _key_toc_next = new EPDGUI_Button("Next >", 276, bottomY, 260, 60);
        _key_toc_next->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)_reader);
        _key_toc_next->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)(intptr_t)(_toc_page + 1));
        _key_toc_next->AddArgs(EPDGUI_Button::EVENT_RELEASED, 2, (void*)(&_is_run));
        _key_toc_next->Bind(EPDGUI_Button::EVENT_RELEASED, key_epubtoc_page_cb);
    }
}

int Frame_EpubToc::init(epdgui_args_vector_t &args) {
    _is_run = 1;
    M5.EPD.Clear();
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);
    EPDGUI_AddObject(_key_exit);
    for (size_t i = 0; i < _key_chapters.size(); i++) {
        EPDGUI_AddObject(_key_chapters[i]);
    }
    if (_key_toc_prev != NULL) EPDGUI_AddObject(_key_toc_prev);
    if (_key_toc_next != NULL) EPDGUI_AddObject(_key_toc_next);
    return 3;
}
