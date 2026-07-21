#include "frame_epubreader.h"
#include "frame_epubtoc.h"

static const int16_t kStatusBarY = 72;
static const int16_t kContentY  = 104;
static const int16_t kContentH  = 856;

static int8_t s_key_operation = 0;

static void key_epubreader_exit_cb(epdgui_args_vector_t &args) {
    EPDGUI_PopFrame(true);
    *((int*)(args[0])) = 0;
}

static void key_nextpage_cb(epdgui_args_vector_t &args) {
    s_key_operation = 1;
}

static void key_prevpage_cb(epdgui_args_vector_t &args) {
    s_key_operation = -1;
}

static void key_chapter_prev_cb(epdgui_args_vector_t &args) {
    ((Frame_EpubReader*)(args[0]))->StepChapterAndShow(-1);
}

static void key_chapter_next_cb(epdgui_args_vector_t &args) {
    ((Frame_EpubReader*)(args[0]))->StepChapterAndShow(1);
}

static void key_toc_cb(epdgui_args_vector_t &args) {
    Frame_EpubReader *reader = (Frame_EpubReader*)(args[0]);
    Frame_Base *frame = new Frame_EpubToc(reader);
    EPDGUI_PushFrame(frame);
    *((int*)(args[1])) = 0;
}

Frame_EpubReader::Frame_EpubReader(String path) {
    _frame_name = "Frame_EpubReader";
    _path = path;

    _canvas_prev    = new M5EPD_Canvas(&M5.EPD);
    _canvas_current = new M5EPD_Canvas(&M5.EPD);
    _canvas_next    = new M5EPD_Canvas(&M5.EPD);
    _canvas_status  = new M5EPD_Canvas(&M5.EPD);

    _key_next = new EPDGUI_Button("", 270, kContentY, 270, kContentH, EPDGUI_Button::STYLE_INVISABLE);
    _key_prev = new EPDGUI_Button("", 0, kContentY, 270, kContentH, EPDGUI_Button::STYLE_INVISABLE);
    _key_next->Bind(EPDGUI_Button::EVENT_RELEASED, key_nextpage_cb);
    _key_prev->Bind(EPDGUI_Button::EVENT_RELEASED, key_prevpage_cb);

    _key_chapter_prev = new EPDGUI_Button("< Ch", 195, 12, 106, 48);
    _key_toc          = new EPDGUI_Button("TOC", 303, 12, 106, 48);
    _key_chapter_next = new EPDGUI_Button("Ch >", 411, 12, 121, 48);

    _key_chapter_prev->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_chapter_prev->Bind(EPDGUI_Button::EVENT_RELEASED, key_chapter_prev_cb);

    _key_chapter_next->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_chapter_next->Bind(EPDGUI_Button::EVENT_RELEASED, key_chapter_next_cb);

    _key_toc->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_toc->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)(&_is_run));
    _key_toc->Bind(EPDGUI_Button::EVENT_RELEASED, key_toc_cb);

    exitbtn("Back");

    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, key_epubreader_exit_cb);
}

Frame_EpubReader::~Frame_EpubReader(void) {
    if (_text_size != 26) {
        _canvas_prev->destoryRender(_text_size);
    }
    delete _canvas_prev;
    delete _canvas_current;
    delete _canvas_next;
    delete _canvas_status;
    delete _key_next;
    delete _key_prev;
    delete _key_chapter_prev;
    delete _key_chapter_next;
    delete _key_toc;
}

int Frame_EpubReader::GetChapterCount() {
    return _book.ChapterCount();
}

String Frame_EpubReader::GetChapterTitle(int index) {
    return _book.ChapterTitle(index);
}

void Frame_EpubReader::RequestChapterJump(int index) {
    _pending_chapter = index;
}

uint32_t Frame_EpubReader::renderText(uint32_t cursor, uint32_t length, M5EPD_Canvas *canvas) {
    canvas->fillCanvas(0);
    canvas->setTextArea(10, 10, 520, kContentH - 20);
    canvas->setTextSize(_text_size);

    uint32_t total = _chapter_text.length();
    uint32_t avail = (total > cursor) ? (total - cursor) : 0;
    uint32_t take = (avail < length) ? avail : length;
    String chunk = _chapter_text.substring(cursor, cursor + take);
    canvas->print(chunk);
    return canvas->getExceedOffset();
}

void Frame_EpubReader::LoadChapter(int index) {
    if ((index < 0) || (index >= _book.ChapterCount())) {
        return;
    }
    _chapter_index = index;
    _chapter_text = _book.GetChapterText(index);
    _page_cursor.clear();
    _page_cursor[0] = 0;
    _page = 0;
    _end_accessed = false;

    uint32_t cursor = renderText(0, _render_len, _canvas_current);
    if (cursor == 0) {
        _page_end = 0;
        _end_accessed = true;
     } else {
        _page_end = 1;
        _page_cursor[1] = cursor;
        uint32_t offset = renderText(_page_cursor[1], _render_len, _canvas_next);
        if (offset == 0) {
            _page_end = 1;
            _end_accessed = true;
         } else {
            _page_cursor[2] = cursor + offset;
        }
    }
}

void Frame_EpubReader::UpdateStatusBar() {
    if ((_last_shown_page == _page) && (_last_shown_chapter == _chapter_index)) {
        return;
    }
    _last_shown_page = _page;
    _last_shown_chapter = _chapter_index;

    _canvas_status->fillCanvas(0);

    String title = GetChapterTitle(_chapter_index);
    if (title.length() > 34) {
        title = title.substring(0, 34) + "...";
    }
    _canvas_status->setTextDatum(CL_DATUM);
    _canvas_status->drawString(title, 10, 16);

    char buf[24];
    sprintf(buf, "Ch %d/%d  p.%lu", _chapter_index + 1, _book.ChapterCount(), (unsigned long)(_page + 1));
    _canvas_status->setTextDatum(CR_DATUM);
    _canvas_status->drawString(buf, 530, 16);

    _canvas_status->pushCanvas(0, kStatusBarY, UPDATE_MODE_GL16);
}

void Frame_EpubReader::StepChapterAndShow(int delta) {
    int target = _chapter_index + delta;
    if ((target < 0) || (target >= _book.ChapterCount())) {
        return;
    }
    LoadChapter(target);
    _canvas_current->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
    UpdateStatusBar();
}

int Frame_EpubReader::init(epdgui_args_vector_t &args) {
    _is_run = 1;

    if (!_open_failed && (_book.ChapterCount() == 0)) {
        if (!_book.Open(_path)) {
            _open_failed = true;
        }
    }

    M5.EPD.Clear();
    _canvas_title->drawString(_path.substring(_path.lastIndexOf("/") + 1, _path.lastIndexOf(".")), 100, 34);
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);

    EPDGUI_AddObject(_key_exit);

    if (_open_failed) {
        _canvas_status->fillCanvas(0);
        _canvas_status->setTextDatum(CL_DATUM);
        _canvas_status->drawString("Could not open this EPUB.", 10, 16);
        _canvas_status->pushCanvas(0, kStatusBarY, UPDATE_MODE_GC16);
        return 1;
    }

    if (!_canvas_prev->isRenderExist(_text_size)) {
        _canvas_prev->createRender(_text_size, 128);
    }

    EPDGUI_AddObject(_key_next);
    EPDGUI_AddObject(_key_prev);
    EPDGUI_AddObject(_key_chapter_prev);
    EPDGUI_AddObject(_key_chapter_next);
    EPDGUI_AddObject(_key_toc);

    if (_pending_chapter >= 0) {
        int target = _pending_chapter;
        _pending_chapter = -1;
        LoadChapter(target);
     } else if (_last_shown_chapter < 0) {
        LoadChapter(0);
    }

    _canvas_current->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
    _last_shown_page = 0xFFFFFFFF; // force UpdateStatusBar to redraw
    UpdateStatusBar();
    return 8;
}

int Frame_EpubReader::run() {
    if (_open_failed) {
        return _is_run;
    }

    M5.update();
    if (M5.BtnR.wasReleased() || (s_key_operation == 1)) {
        s_key_operation = 0;
        if (_page != _page_end) {
            _page++;
            _canvas_next->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
            memcpy(_canvas_prev->frameBuffer(), _canvas_current->frameBuffer(), _canvas_current->getBufferSize());
            memcpy(_canvas_current->frameBuffer(), _canvas_next->frameBuffer(), _canvas_next->getBufferSize());

            if ((!_end_accessed) || (_page != _page_end)) {
                uint32_t offset = renderText(_page_cursor[_page + 1], _render_len, _canvas_next);
                if (offset != 0) {
                    if (_page_cursor.count(_page + 2) == 0) {
                        _page_cursor[_page + 2] = _page_cursor[_page + 1] + offset;
                    }
                 } else if (!_end_accessed) {
                    _page_end = _page + 1;
                    _end_accessed = true;
                }
                if (!_end_accessed) {
                    _page_end = _page + 1;
                }
            }
            UpdateStatusBar();
         } else if (_chapter_index + 1 < _book.ChapterCount()) {
            LoadChapter(_chapter_index + 1);
            _canvas_current->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
            UpdateStatusBar();
        }
     } else if (M5.BtnL.wasReleased() || (s_key_operation == -1)) {
        s_key_operation = 0;
        if (_page > 0) {
            _page--;
            _canvas_prev->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
            memcpy(_canvas_next->frameBuffer(), _canvas_current->frameBuffer(), _canvas_current->getBufferSize());
            memcpy(_canvas_current->frameBuffer(), _canvas_prev->frameBuffer(), _canvas_prev->getBufferSize());
            if (_page != 0) {
                renderText(_page_cursor[_page - 1], _render_len, _canvas_prev);
            }
            UpdateStatusBar();
         } else if (_chapter_index > 0) {
            LoadChapter(_chapter_index - 1);
            // Walk the whole chapter to find its true last page - moving
            // backward should land at the end of the previous chapter, not
            // its start.
            uint32_t page = 0;
            uint32_t cursor = 0;
            while (true) {
                uint32_t offset = renderText(cursor, _render_len, _canvas_current);
                if (offset == 0) {
                    break;
                }
                cursor += offset;
                page++;
                _page_cursor[page] = cursor;
            }
            _page = page;
            _page_end = page;
            _end_accessed = true;
            if (_page > 0) {
                renderText(_page_cursor[_page - 1], _render_len, _canvas_prev);
            }
            _canvas_current->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
            UpdateStatusBar();
        }
    }
    return _is_run;
}
