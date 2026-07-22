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

void Frame_EpubReader::renderImagePage(const String &href, M5EPD_Canvas *canvas) {
    canvas->fillCanvas(0);
    if (!_book.DrawImage(href, canvas, 10, 10, 520, kContentH - 20)) {
        canvas->setTextSize(_text_size);
        canvas->setTextDatum(CL_DATUM);
        canvas->drawString("[image could not be loaded]", 10, 10);
    }
}

Frame_EpubReader::PageCursor Frame_EpubReader::renderPage(PageCursor cursor, M5EPD_Canvas *canvas, bool *hasMore) {
    if ((cursor.imgIdx < _chapter_images.size()) && (_chapter_images[cursor.imgIdx].pos == cursor.textPos)) {
        renderImagePage(_chapter_images[cursor.imgIdx].href, canvas);
        PageCursor next{cursor.textPos, cursor.imgIdx + 1};
        *hasMore = (next.imgIdx < _chapter_images.size()) || (next.textPos < _chapter_text.length());
        return next;
    }

    uint32_t total = _chapter_text.length();
    uint32_t limit = _render_len;
    if (cursor.imgIdx < _chapter_images.size()) {
        uint32_t toImage = _chapter_images[cursor.imgIdx].pos - cursor.textPos;
        if (toImage < limit) {
            limit = toImage;
        }
    }

    canvas->fillCanvas(0);
    canvas->setTextArea(10, 10, 520, kContentH - 20);
    canvas->setTextSize(_text_size);

    uint32_t avail = (total > cursor.textPos) ? (total - cursor.textPos) : 0;
    uint32_t take = (avail < limit) ? avail : limit;
    String chunk = _chapter_text.substring(cursor.textPos, cursor.textPos + take);
    canvas->print(chunk);
    uint32_t exceed = canvas->getExceedOffset();
    uint32_t consumed = (exceed != 0) ? exceed : take;

    PageCursor next{cursor.textPos + consumed, cursor.imgIdx};
    *hasMore = (next.imgIdx < _chapter_images.size()) || (next.textPos < total);
    return next;
}

void Frame_EpubReader::LoadChapter(int index) {
    if ((index < 0) || (index >= _book.ChapterCount())) {
        return;
    }
    _chapter_index = index;
    _chapter_images.clear();
    _chapter_text = _book.GetChapterText(index, &_chapter_images);
    _page_cursor.clear();
    _page_cursor[0] = PageCursor{0, 0};
    _page = 0;
    _end_accessed = false;

    bool hasMore = false;
    PageCursor next = renderPage(_page_cursor[0], _canvas_current, &hasMore);
    if (!hasMore) {
        _page_end = 0;
        _end_accessed = true;
     } else {
        _page_end = 1;
        _page_cursor[1] = next;
        bool hasMore2 = false;
        PageCursor next2 = renderPage(_page_cursor[1], _canvas_next, &hasMore2);
        if (!hasMore2) {
            _page_end = 1;
            _end_accessed = true;
         } else {
            _page_cursor[2] = next2;
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
    _canvas_status->setTextSize(26);

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

    _canvas_prev->createCanvas(540, kContentH);
    _canvas_current->createCanvas(540, kContentH);
    _canvas_next->createCanvas(540, kContentH);
    _canvas_status->createCanvas(540, kContentY - kStatusBarY);

    EPDGUI_AddObject(_key_exit);

    if (_open_failed) {
        _canvas_status->fillCanvas(0);
        _canvas_status->setTextSize(26);
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
                bool hasMore = false;
                PageCursor next = renderPage(_page_cursor[_page + 1], _canvas_next, &hasMore);
                if (hasMore) {
                    if (_page_cursor.count(_page + 2) == 0) {
                        _page_cursor[_page + 2] = next;
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
                bool hasMore;
                renderPage(_page_cursor[_page - 1], _canvas_prev, &hasMore);
            }
            UpdateStatusBar();
         } else if (_chapter_index > 0) {
            LoadChapter(_chapter_index - 1);
            // Walk the whole chapter to find its true last page - moving
            // backward should land at the end of the previous chapter, not
            // its start.
            uint32_t page = 0;
            PageCursor cursor{0, 0};
            while (true) {
                bool hasMore = false;
                PageCursor next = renderPage(cursor, _canvas_current, &hasMore);
                if (!hasMore) {
                    break;
                }
                cursor = next;
                page++;
                _page_cursor[page] = cursor;
            }
            _page = page;
            _page_end = page;
            _end_accessed = true;
            if (_page > 0) {
                bool hasMore;
                renderPage(_page_cursor[_page - 1], _canvas_prev, &hasMore);
            }
            _canvas_current->pushCanvas(0, kContentY, UPDATE_MODE_GC16);
            UpdateStatusBar();
        }
    }
    return _is_run;
}
