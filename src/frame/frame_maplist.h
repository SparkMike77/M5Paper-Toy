#ifndef _FRAME_MAPLIST_H_
#define _FRAME_MAPLIST_H_

#include "frame_base.h"
#include "../epdgui/epdgui.h"
#include <vector>

// Lists offline map tile sets: one entry per subfolder of /maps on the SD
// card. Tapping one opens Frame_MapReader on that folder.
class Frame_MapList : public Frame_Base {
public:
    Frame_MapList();
    ~Frame_MapList();
    int init(epdgui_args_vector_t &args);

private:
    void BuildList();

    std::vector<EPDGUI_Button*> _key_maps;
    bool _scanned = false;
};

#endif //_FRAME_MAPLIST_H_
