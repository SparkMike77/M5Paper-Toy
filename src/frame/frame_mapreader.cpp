#include "frame_mapreader.h"

static void key_mapreader_exit_cb(epdgui_args_vector_t &args) {
    EPDGUI_PopFrame(true);
    *((int*)(args[0])) = 0;
}

static void key_mapreader_zoomout_cb(epdgui_args_vector_t &args) {
    ((Frame_MapReader*)(args[0]))->StepZoom(-1);
}

static void key_mapreader_zoomin_cb(epdgui_args_vector_t &args) {
    ((Frame_MapReader*)(args[0]))->StepZoom(1);
}

static void key_mapreader_left_cb(epdgui_args_vector_t &args) {
    ((Frame_MapReader*)(args[0]))->Pan(-1, 0);
}

static void key_mapreader_right_cb(epdgui_args_vector_t &args) {
    ((Frame_MapReader*)(args[0]))->Pan(1, 0);
}

static void key_mapreader_up_cb(epdgui_args_vector_t &args) {
    ((Frame_MapReader*)(args[0]))->Pan(0, -1);
}

static void key_mapreader_down_cb(epdgui_args_vector_t &args) {
    ((Frame_MapReader*)(args[0]))->Pan(0, 1);
}

Frame_MapReader::Frame_MapReader(String path) {
    _frame_name = "Frame_MapReader";
    _path = path;

    _canvas_map = new M5EPD_Canvas(&M5.EPD);
    _canvas_status = new M5EPD_Canvas(&M5.EPD);

    exitbtn("Back", 90);

    _key_zoom_out = new EPDGUI_Button("Zoom-", 104, 12, 68, 48);
    _key_zoom_in  = new EPDGUI_Button("Zoom+", 176, 12, 68, 48);
    _key_left     = new EPDGUI_Button("<",     248, 12, 68, 48);
    _key_right    = new EPDGUI_Button(">",     320, 12, 68, 48);
    _key_up       = new EPDGUI_Button("^",     392, 12, 68, 48);
    _key_down     = new EPDGUI_Button("v",     464, 12, 68, 48);

    _key_zoom_out->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_zoom_out->Bind(EPDGUI_Button::EVENT_RELEASED, key_mapreader_zoomout_cb);

    _key_zoom_in->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_zoom_in->Bind(EPDGUI_Button::EVENT_RELEASED, key_mapreader_zoomin_cb);

    _key_left->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_left->Bind(EPDGUI_Button::EVENT_RELEASED, key_mapreader_left_cb);

    _key_right->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_right->Bind(EPDGUI_Button::EVENT_RELEASED, key_mapreader_right_cb);

    _key_up->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_up->Bind(EPDGUI_Button::EVENT_RELEASED, key_mapreader_up_cb);

    _key_down->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_down->Bind(EPDGUI_Button::EVENT_RELEASED, key_mapreader_down_cb);

    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, key_mapreader_exit_cb);
}

Frame_MapReader::~Frame_MapReader(void) {
    delete _canvas_map;
    delete _canvas_status;
    delete _key_zoom_out;
    delete _key_zoom_in;
    delete _key_left;
    delete _key_right;
    delete _key_up;
    delete _key_down;
}

void Frame_MapReader::Render() {
    _canvas_map->fillCanvas(0);
    int zoom = _map.ZoomLevelAt(_zoomIndex);
    for (int gy = 0; gy < kGridH; gy++) {
        for (int gx = 0; gx < kGridW; gx++) {
            _map.DrawTile(_canvas_map, zoom, _originX + gx, _originY + gy,
                          gx * TileMap::kTileSize, gy * TileMap::kTileSize);
        }
    }
    // Centered within the content area: (540 - 512) / 2 = 14, 104 + (856 - 768) / 2 = 148.
    _canvas_map->pushCanvas(14, 148, UPDATE_MODE_GC16);
    UpdateStatusBar();
}

void Frame_MapReader::UpdateStatusBar() {
    _canvas_status->fillCanvas(0);
    _canvas_status->setTextSize(24);
    _canvas_status->setTextDatum(CL_DATUM);
    char buf[64];
    int zoom = _map.ZoomLevelAt(_zoomIndex);
    sprintf(buf, "Zoom %d (%d/%d)   tile %ld,%ld", zoom, _zoomIndex + 1, _map.ZoomLevelCount(), _originX, _originY);
    _canvas_status->drawString(buf, 10, 16);
    _canvas_status->pushCanvas(0, 72, UPDATE_MODE_GL16);
}

void Frame_MapReader::Pan(int dx, int dy) {
    if (_open_failed) {
        return;
    }
    _originX += dx;
    _originY += dy;
    if (_originX < 0) {
        _originX = 0;
    }
    if (_originY < 0) {
        _originY = 0;
    }
    Render();
}

void Frame_MapReader::StepZoom(int delta) {
    if (_open_failed) {
        return;
    }
    int newIndex = _zoomIndex + delta;
    if ((newIndex < 0) || (newIndex >= _map.ZoomLevelCount())) {
        return;
    }
    int oldZoom = _map.ZoomLevelAt(_zoomIndex);
    int newZoom = _map.ZoomLevelAt(newIndex);
    int d = newZoom - oldZoom;

    // Rescale around the grid's approximate center so the same area stays
    // in view across zoom levels, rather than drifting toward one corner.
    long centerX = _originX + kGridW / 2;
    long centerY = _originY + kGridH / 2;
    if (d >= 0) {
        centerX <<= d;
        centerY <<= d;
     } else {
        centerX >>= (-d);
        centerY >>= (-d);
    }
    _originX = centerX - kGridW / 2;
    _originY = centerY - kGridH / 2;
    if (_originX < 0) {
        _originX = 0;
    }
    if (_originY < 0) {
        _originY = 0;
    }

    _zoomIndex = newIndex;
    Render();
}

int Frame_MapReader::init(epdgui_args_vector_t &args) {
    _is_run = 1;

    if (!_open_failed && !_loaded) {
        if (_map.Open(_path) && (_map.ZoomLevelCount() > 0)) {
            _zoomIndex = _map.ZoomLevelCount() / 2;
            int zoom = _map.ZoomLevelAt(_zoomIndex);
            long cx, cy;
            if (_map.FindCenterTile(zoom, cx, cy)) {
                _originX = cx - kGridW / 2;
                _originY = cy - kGridH / 2;
                if (_originX < 0) {
                    _originX = 0;
                }
                if (_originY < 0) {
                    _originY = 0;
                }
            }
            _loaded = true;
         } else {
            _open_failed = true;
        }
    }

    M5.EPD.Clear();
    _canvas_title->drawString(_path.substring(_path.lastIndexOf("/") + 1), 270, 34);
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);

    EPDGUI_AddObject(_key_exit);
    _canvas_status->createCanvas(540, 32);

    if (_open_failed) {
        _canvas_status->fillCanvas(0);
        _canvas_status->setTextSize(26);
        _canvas_status->setTextDatum(CL_DATUM);
        _canvas_status->drawString("No tiles found in this map folder.", 10, 16);
        _canvas_status->pushCanvas(0, 72, UPDATE_MODE_GC16);
        return 1;
    }

    _canvas_map->createCanvas(kGridW * TileMap::kTileSize, kGridH * TileMap::kTileSize);

    EPDGUI_AddObject(_key_zoom_out);
    EPDGUI_AddObject(_key_zoom_in);
    EPDGUI_AddObject(_key_left);
    EPDGUI_AddObject(_key_right);
    EPDGUI_AddObject(_key_up);
    EPDGUI_AddObject(_key_down);

    Render();
    return 8;
}

int Frame_MapReader::run() {
    return _is_run;
}
