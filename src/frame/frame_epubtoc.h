#ifndef _FRAME_EPUBTOC_H_
#define _FRAME_EPUBTOC_H_

#include "frame_base.h"
#include "../epdgui/epdgui.h"

class Frame_EpubReader;

class Frame_EpubToc : public Frame_Base {
public:
    Frame_EpubToc(Frame_EpubReader *reader, int page = 0);
    ~Frame_EpubToc();
    int init(epdgui_args_vector_t &args);

private:
    void BuildList();

    Frame_EpubReader *_reader;
    int _toc_page;
    std::vector<EPDGUI_Button*> _key_chapters;
    EPDGUI_Button *_key_toc_prev = NULL;
    EPDGUI_Button *_key_toc_next = NULL;
};

#endif //_FRAME_EPUBTOC_H_
