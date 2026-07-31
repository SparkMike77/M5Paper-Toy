#ifndef _FRAME_MAPREADER_H_
#define _FRAME_MAPREADER_H_

#include "frame_base.h"
#include "../epdgui/epdgui.h"
#include "../maps/tile_map.h"

// Pans and zooms an offline XYZ tile set (see tile_map.h). No GPS on this
// hardware yet, so this is a manual browse: a fixed 2x3 grid of whole
// tiles is shown at a time (never a partial tile - see tile_map.h for
// why), moved one tile at a time by the direction buttons, with a
// zoom in/out pair that rescales the current view to roughly the same
// area at the new zoom level.
class Frame_MapReader : public Frame_Base {
public:
    Frame_MapReader(String path);
    ~Frame_MapReader();
    int init(epdgui_args_vector_t &args);
    int run();

    void Pan(int dx, int dy);
    void StepZoom(int delta);

private:
    void Render();
    void UpdateStatusBar();

    static const int kGridW = 2;
    static const int kGridH = 3;

    String _path;
    TileMap _map;
    bool _open_failed = false;
    bool _loaded = false;

    int _zoomIndex = 0;
    long _originX = 0;
    long _originY = 0;

    M5EPD_Canvas *_canvas_map;
    M5EPD_Canvas *_canvas_status;

    EPDGUI_Button *_key_zoom_out;
    EPDGUI_Button *_key_zoom_in;
    EPDGUI_Button *_key_left;
    EPDGUI_Button *_key_right;
    EPDGUI_Button *_key_up;
    EPDGUI_Button *_key_down;
};

#endif //_FRAME_MAPREADER_H_
