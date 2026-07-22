#ifndef _FRAME_PDFREADER_H_
#define _FRAME_PDFREADER_H_

#include "frame_base.h"
#include "../epdgui/epdgui.h"
#include "../pdf/pdf_book.h"
#include <map>

// Reads a PDF as extracted plain text, one PDF page at a time - each PDF
// page may itself paginate into several screen pages if its text doesn't
// fit on one, exactly like the EPUB/TXT readers. See pdf_book.h for what
// is and isn't supported.
class Frame_PdfReader : public Frame_Base {
public:
    Frame_PdfReader(String path);
    ~Frame_PdfReader();
    int init(epdgui_args_vector_t &args);
    int run();

    // Bound directly to the prev/next page buttons - same frame, no stack
    // transition involved, so these apply and render immediately.
    void StepPageAndShow(int delta);

private:
    uint32_t renderText(uint32_t cursor, uint32_t length, M5EPD_Canvas *canvas);
    void LoadPage(int index);
    void UpdateStatusBar();

    String _path;
    PdfBook _book;
    bool _open_failed = false;

    int _page_index = 0;
    String _page_text;

    uint16_t _text_size = 32;
    M5EPD_Canvas *_canvas_prev;
    M5EPD_Canvas *_canvas_current;
    M5EPD_Canvas *_canvas_next;
    M5EPD_Canvas *_canvas_status;
    std::map<uint32_t, uint32_t> _screen_cursor;
    uint32_t _render_len = 1000;
    uint32_t _screen_page = 0;
    uint32_t _screen_page_end = 0;
    uint32_t _last_shown_screen_page = 0xFFFFFFFF;
    int _last_shown_page_index = -1;
    bool _end_accessed = false;

    EPDGUI_Button *_key_next;
    EPDGUI_Button *_key_prev;
    EPDGUI_Button *_key_page_prev;
    EPDGUI_Button *_key_page_next;
};

#endif //_FRAME_PDFREADER_H_
