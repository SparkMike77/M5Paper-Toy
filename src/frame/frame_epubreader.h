#ifndef _FRAME_EPUBREADER_H_
#define _FRAME_EPUBREADER_H_

#include "frame_base.h"
#include "../epdgui/epdgui.h"
#include "../epub/epub_book.h"
#include <map>

class Frame_EpubReader : public Frame_Base {
public:
    Frame_EpubReader(String path);
    ~Frame_EpubReader();
    int init(epdgui_args_vector_t &args);
    int run();

    // Used by Frame_EpubToc, pushed on top of this frame, to request a
    // chapter jump - applied the next time this frame's init() runs (i.e.
    // once the TOC frame pops back off the stack).
    int GetChapterCount();
    String GetChapterTitle(int index);
    void RequestChapterJump(int index);

    // Bound directly to the prev/next chapter buttons - same frame, no
    // stack transition involved, so these apply and render immediately.
    void StepChapterAndShow(int delta);

private:
    uint32_t renderText(uint32_t cursor, uint32_t length, M5EPD_Canvas *canvas);
    void LoadChapter(int index);
    void UpdateStatusBar();

    String _path;
    EpubBook _book;
    bool _open_failed = false;

    int _chapter_index = 0;
    int _pending_chapter = -1;
    String _chapter_text;

    uint16_t _text_size = 32;
    M5EPD_Canvas *_canvas_prev;
    M5EPD_Canvas *_canvas_current;
    M5EPD_Canvas *_canvas_next;
    M5EPD_Canvas *_canvas_status;
    std::map<uint32_t, uint32_t> _page_cursor;
    uint32_t _render_len = 1000;
    uint32_t _page = 0;
    uint32_t _page_end = 0;
    uint32_t _last_shown_page = 0xFFFFFFFF;
    int _last_shown_chapter = -1;
    bool _end_accessed = false;
    bool _needs_render = true;

    EPDGUI_Button *_key_next;
    EPDGUI_Button *_key_prev;
    EPDGUI_Button *_key_chapter_prev;
    EPDGUI_Button *_key_chapter_next;
    EPDGUI_Button *_key_toc;
};

#endif //_FRAME_EPUBREADER_H_
