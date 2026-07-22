#include "frame_pdfreader.h"

static const int16_t kStatusBarY = 72;
static const int16_t kContentY  = 104;
static const int16_t kContentH  = 856;

static int8_t s_key_operation = 0;

static void key_pdfreader_exit_cb(epdgui_args_vector_t &args) {
    EPDGUI_PopFrame(true);
    *((int*)(args[0])) = 0;
}

static void key_nextpage_cb(epdgui_args_vector_t &args) {
    s_key_operation = 1;
}

static void key_prevpage_cb(epdgui_args_vector_t &args) {
    s_key_operation = -1;
}

static void key_page_prev_cb(epdgui_args_vector_t &args) {
    ((Frame_PdfReader*)(args[0]))->StepPageAndShow(-1);
}

static void key_page_next_cb(epdgui_args_vector_t &args) {
    ((Frame_PdfReader*)(args[0]))->StepPageAndShow(1);
}

Frame_PdfReader::Frame_PdfReader(String path) {
    _frame_name = "Frame_PdfReader";
    _path = path;

    _canvas_prev    = new M5EPD_Canvas(&M5.EPD);
    _canvas_current = new M5EPD_Canvas(&M5.EPD);
    _canvas_next    = new M5EPD_Canvas(&M5.EPD);
    _canvas_status  = new M5EPD_Canvas(&M5.EPD);

    _key_next = new EPDGUI_Button("", 270, kContentY, 270, kContentH, EPDGUI_Button::STYLE_INVISABLE);
    _key_prev = new EPDGUI_Button("", 0, kContentY, 270, kContentH, EPDGUI_Button::STYLE_INVISABLE);
    _key_next->Bind(EPDGUI_Button::EVENT_RELEASED, key_nextpage_cb);
    _key_prev->Bind(EPDGUI_Button::EVENT_RELEASED, key_prevpage_cb);

    _key_page_prev = new EPDGUI_Button("< Pg", 195, 12, 165, 48);
    _key_page_next = new EPDGUI_Button("Pg >", 372, 12, 165, 48);

    _key_page_prev->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_page_prev->Bind(EPDGUI_Button::EVENT_RELEASED, key_page_prev_cb);

    _key_page_next->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_page_next->Bind(EPDGUI_Button::EVENT_RELEASED, key_page_next_cb);

    exitbtn("Back");

    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, key_pdfreader_exit_cb);
}

Frame_PdfReader::~Frame_PdfReader(void) {
    if (_text_size != 26) {
        _canvas_prev->destoryRender(_text_size);
    }
    delete _canvas_prev;
    delete _canvas_current;
    delete _canvas_next;
    delete _canvas_status;
    delete _key_next;
    delete _key_prev;
    delete _key_page_prev;
    delete _key_page_next;
}

uint32_t Frame_PdfReader::renderText(uint32_t cursor, uint32_t length, M5EPD_Canvas *canvas) {
    canvas->fillCanvas(0);
    canvas->setTextArea(10, 10, 520, kContentH - 20);
    canvas->setTextSize(_text_size);

    uint32_t total = _page_text.length();
    uint32_t avail = (total > cursor) ? (total - cursor) : 0;
    uint32_t take = (avail < length) ? avail : length;
    String chunk = _page_text.substring(cursor, cursor + take);
    canvas->print(chunk);
    return canvas->getExceedOffset();
}

void Frame_PdfReader::LoadPage(int index) {
    if ((index < 0) || (index >= _book.PageCount())) {
        return;
    }
    _page_index = index;
    _page_text = _book.GetPageText(index);
    _screen_cursor.clear();
    _screen_cursor[0] = 0;
    _screen_page = 0;
    _end_accessed = false;

    uint32_t cursor = renderText(0, _render_len, _canvas_current);
    if (cursor == 0) {
        _screen_page_end = 0;
        _end_accessed = true;
     } else {
        _screen_page_end = 1;
        _screen_cursor[1] = cursor;
        uint32_t offset = renderText(_screen_cursor[1], _render_len, _canvas_next);
        if (offset == 0) {
            _screen_page_end = 1;
            _end_accessed = true;
         } else {
            _screen_cursor[2] = cursor + offset;
        }
    }
}

void Frame_PdfReader::UpdateStatusBar() {
    if ((_last_shown_screen_page == _screen_page) && (_last_shown_page_index == _page_index)) {
        return;
    }
    _last_shown_screen_page = _screen_page;
    _last_shown_page_index = _page_index;

    _canvas_status->fillCanvas(0);
    _canvas_status->setTextSize(26);

    _canvas_status->setTextDatum(CL_DATUM);
    _canvas_status->drawString(_path.substring(_path.lastIndexOf("/") + 1), 10, 16);

    char buf[24];
    sprintf(buf, "Pg %d/%d  p.%lu", _page_index + 1, _book.PageCount(), (unsigned long)(_screen_page + 1));
    _canvas_status->setTextDatum(CR_DATUM);
    _canvas_status->drawString(buf, 530, 16);

    _canvas_status->pushCanvas(0, kStatusBarY, UPDATE_MODE_GL16);
}

int Frame_PdfReader::init(epdgui_args_vector_t &args) {
    _is_run = 1;

    if (!_open_failed && (_book.PageCount() == 0)) {
        if (!_book.Open(_path)) {
            _open_failed = true;
        }
    }

    M5.EPD.Clear();
    _canvas_title->drawString(_path.substring(_path.lastIndexOf("/") + 1, _path.lastIndexOf(".")), 100, 34);
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);

    _canvas_prev->createCanvas(540, kContentH);
    _canvas_current->createCanvas(540, kContentH);
    _canvas_next->createCanvas(540, kContentH);
    _canvas_status->createCanvas(540, kContentY - kStatusBarY);

    EPDGUI_AddObject(_key_exit);

    if (_open_failed) {
        _canvas_status->fillCanvas(0);
        _canvas_status->setTextSize(26);
        _canvas_status->setTextDatum(CL_DATUM);
        _canvas_status->drawString("Could not open this PDF.", 10, 16);
        _canvas_status->pushCanvas(0, kStatusBarY, UPDATE_MODE_GC16);
        return 1;
    }

    if (!_canvas_prev->isRenderExist(_text_size)) {
        _canvas_prev->createRender(_text_size, 128);
    }

    EPDGUI_AddObject(_key_next);
    EPDGUI_AddObject(_key_prev);
    EPDGUI_AddObject(_key_page_prev);
    EPDGUI_AddObject(_key_page_next);

    if (_last_shown_page_index < 0) {
        LoadPage(0);
    }

    _canvas_current->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
    _last_shown_screen_page = 0xFFFFFFFF; // force UpdateStatusBar to redraw
    UpdateStatusBar();
    return 8;
}

void Frame_PdfReader::StepPageAndShow(int delta) {
    int target = _page_index + delta;
    if ((target < 0) || (target >= _book.PageCount())) {
        return;
    }
    LoadPage(target);
    _canvas_current->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
    UpdateStatusBar();
}

int Frame_PdfReader::run() {
    if (_open_failed) {
        return _is_run;
    }

    M5.update();
    if (M5.BtnR.wasReleased() || (s_key_operation == 1)) {
        s_key_operation = 0;
        if (_screen_page != _screen_page_end) {
            _screen_page++;
            _canvas_next->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
            memcpy(_canvas_prev->frameBuffer(), _canvas_current->frameBuffer(), _canvas_current->getBufferSize());
            memcpy(_canvas_current->frameBuffer(), _canvas_next->frameBuffer(), _canvas_next->getBufferSize());

            if ((!_end_accessed) || (_screen_page != _screen_page_end)) {
                uint32_t offset = renderText(_screen_cursor[_screen_page + 1], _render_len, _canvas_next);
                if (offset != 0) {
                    if (_screen_cursor.count(_screen_page + 2) == 0) {
                        _screen_cursor[_screen_page + 2] = _screen_cursor[_screen_page + 1] + offset;
                    }
                 } else if (!_end_accessed) {
                    _screen_page_end = _screen_page + 1;
                    _end_accessed = true;
                }
                if (!_end_accessed) {
                    _screen_page_end = _screen_page + 1;
                }
            }
            UpdateStatusBar();
         } else if (_page_index + 1 < _book.PageCount()) {
            LoadPage(_page_index + 1);
            _canvas_current->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
            UpdateStatusBar();
        }
     } else if (M5.BtnL.wasReleased() || (s_key_operation == -1)) {
        s_key_operation = 0;
        if (_screen_page > 0) {
            _screen_page--;
            _canvas_prev->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
            memcpy(_canvas_next->frameBuffer(), _canvas_current->frameBuffer(), _canvas_current->getBufferSize());
            memcpy(_canvas_current->frameBuffer(), _canvas_prev->frameBuffer(), _canvas_prev->getBufferSize());
            if (_screen_page != 0) {
                renderText(_screen_cursor[_screen_page - 1], _render_len, _canvas_prev);
            }
            UpdateStatusBar();
         } else if (_page_index > 0) {
            LoadPage(_page_index - 1);
            // Walk the whole PDF page to find its true last screen page -
            // moving backward should land at its end, not its start.
            uint32_t page = 0;
            uint32_t cursor = 0;
            while (true) {
                uint32_t offset = renderText(cursor, _render_len, _canvas_current);
                if (offset == 0) {
                    break;
                }
                cursor += offset;
                page++;
                _screen_cursor[page] = cursor;
            }
            _screen_page = page;
            _screen_page_end = page;
            _end_accessed = true;
            if (_screen_page > 0) {
                renderText(_screen_cursor[_screen_page - 1], _render_len, _canvas_prev);
            }
            _canvas_current->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
            UpdateStatusBar();
        }
    }
    return _is_run;
}
