#include "frame_notes.h"
#include "SD.h"
#include <string.h>
#include <math.h>

// Freehand notepad. Touch is normally consumed by the epdgui button/widget
// system (see epdgui.cpp), but that only handles tap-in-a-box widgets, not
// continuous drag input - so the drawing canvas reads M5.TP directly here,
// the same way Frame_FactoryTest's touch test screen does.
static const int      kNumSlots      = 5;
static const char    *kNotesDir      = "/notes";
static const int16_t  kCanvasTop     = 220;
static const int16_t  kCanvasWidth   = 540;
static const int16_t  kCanvasHeight  = 710;

static M5EPD_Canvas *notes_canvas      = NULL;
static int           notes_slot        = 0;
static bool          notes_pen_down    = false;
static int16_t       notes_last_x      = -1;
static int16_t       notes_last_y      = -1;

static size_t NotesBufferSize() {
    return (size_t)kCanvasWidth * kCanvasHeight / 2; // 4bpp packed, 2px/byte
}

static String NotesSlotPath(int slot) {
    char buf[32];
    sprintf(buf, "%s/note_%d.raw", kNotesDir, slot + 1);
    return String(buf);
}

static void NotesLoad(int slot) {
    notes_slot = slot;
    notes_canvas->fillCanvas(0);
    if (!GetInitStatus(0)) {
        return;
    }
    String path = NotesSlotPath(slot);
    if (SD.exists(path)) {
        File f = SD.open(path, FILE_READ);
        if (f) {
            f.read((uint8_t*)notes_canvas->frameBuffer(), NotesBufferSize());
            f.close();
        }
    }
}

static void NotesSave(int slot) {
    if (!GetInitStatus(0)) {
        return;
    }
    if (!SD.exists(kNotesDir)) {
        SD.mkdir(kNotesDir);
    }
    File f = SD.open(NotesSlotPath(slot), FILE_WRITE);
    if (f) {
        f.write((uint8_t*)notes_canvas->frameBuffer(), NotesBufferSize());
        f.close();
    }
}

// M5EPD_Canvas::pushCanvas() always transfers/refreshes the canvas's full
// width/height (see M5EPD_Canvas.cpp), which for a 540x710 canvas dominates
// the per-sample cost of live stroke tracking. This pushes just a small
// rectangle instead, going straight through M5EPD_Driver's own partial-area
// primitives, which take an arbitrary x/y/w/h. WritePartGram4bpp requires a
// tightly-packed 4bpp buffer for exactly that sub-rectangle (not a slice of
// the canvas's own buffer, which has a wider row stride) and w must be a
// multiple of 4, so the rows are memcpy'd out row-by-row into a small stack
// buffer first.
static void NotesPushRegion(int16_t cx0, int16_t cy0, int16_t cx1, int16_t cy1) {
    static const int16_t kMargin  = 3;
    static const int16_t kMaxTile = 96; // multiple of 4; bounds the stack buffer

    if (cx0 > cx1) { int16_t t = cx0; cx0 = cx1; cx1 = t; }
    if (cy0 > cy1) { int16_t t = cy0; cy0 = cy1; cy1 = t; }
    cx0 -= kMargin; cy0 -= kMargin;
    cx1 += kMargin; cy1 += kMargin;

    if (cx0 < 0) cx0 = 0;
    if (cy0 < 0) cy0 = 0;
    if (cx1 > kCanvasWidth)  cx1 = kCanvasWidth;
    if (cy1 > kCanvasHeight) cy1 = kCanvasHeight;

    // Align to a 4-pixel boundary (WritePartGram4bpp's hard requirement),
    // anchored on the latest point so a big jump clips its trailing edge
    // rather than the pen's current position.
    int16_t x0 = cx0 & ~3;
    int16_t w  = ((cx1 - x0) + 3) & ~3;
    if (w > kMaxTile) w = kMaxTile;
    if (x0 + w > kCanvasWidth) x0 = kCanvasWidth - w;

    int16_t y0 = cy0;
    int16_t h  = cy1 - cy0;
    if (h > kMaxTile) h = kMaxTile;
    if (y0 + h > kCanvasHeight) y0 = kCanvasHeight - h;

    if ((w <= 0) || (h <= 0)) {
        return;
    }

    uint16_t rowBytes = w / 2;
    static uint8_t tile[kMaxTile * kMaxTile / 2];
    const uint8_t *master = (const uint8_t*)notes_canvas->frameBuffer();
    uint16_t masterStride = kCanvasWidth / 2;

    for (int16_t r = 0; r < h; r++) {
        memcpy(tile + r * rowBytes, master + (y0 + r) * masterStride + x0 / 2, rowBytes);
    }

    m5epd_err_t err = M5.EPD.WritePartGram4bpp(x0, kCanvasTop + y0, w, h, tile);
    if (err == M5EPD_OK) {
        M5.EPD.UpdateArea(x0, kCanvasTop + y0, w, h, UPDATE_MODE_DU4);
     } else {
        // Something about the region was rejected (shouldn't happen given
        // the alignment above) - fall back to a full push so the stroke
        // still shows up rather than silently vanishing.
        notes_canvas->pushCanvas(0, kCanvasTop, UPDATE_MODE_DU4);
    }
}

static void notes_slot_cb(epdgui_args_vector_t &args) {
    EPDGUI_Button *btn = (EPDGUI_Button*)(args[0]);
    EPDGUI_Button *save_btn = (EPDGUI_Button*)(args[1]);
    int slot = btn->getLabel().toInt() - 1;

    NotesLoad(slot);
    notes_pen_down = false;
    notes_last_x = -1;
    notes_last_y = -1;

    char buf[10];
    sprintf(buf, "Save %d", slot + 1);
    save_btn->setLabel(buf);
    save_btn->Draw(UPDATE_MODE_GL16);
    notes_canvas->pushCanvas(0, kCanvasTop, UPDATE_MODE_GC16);
}

static void notes_save_cb(epdgui_args_vector_t &args) {
    NotesSave(notes_slot);
}

static void notes_clear_cb(epdgui_args_vector_t &args) {
    notes_canvas->fillCanvas(0);
    notes_pen_down = false;
    notes_last_x = -1;
    notes_last_y = -1;
    notes_canvas->pushCanvas(0, kCanvasTop, UPDATE_MODE_GC16);
}

Frame_Notes::Frame_Notes(void) {
    _frame_name = "Frame_Notes";

    notes_canvas = new M5EPD_Canvas(&M5.EPD);
    notes_canvas->createCanvas(kCanvasWidth, kCanvasHeight);

    for (int i = 0; i < kNumSlots; i++) {
        char label[4];
        sprintf(label, "%d", i + 1);
        _key_slot[i] = new EPDGUI_Button(label, i * (540 / kNumSlots), 80, 540 / kNumSlots, 70);
    }
    _key_save  = new EPDGUI_Button("Save 1", 0, 150, 270, 70);
    _key_clear = new EPDGUI_Button("Clear", 270, 150, 270, 70);

    for (int i = 0; i < kNumSlots; i++) {
        _key_slot[i]->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)_key_slot[i]);
        _key_slot[i]->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)_key_save);
        _key_slot[i]->Bind(EPDGUI_Button::EVENT_RELEASED, notes_slot_cb);
    }
    _key_save->Bind(EPDGUI_Button::EVENT_RELEASED, notes_save_cb);
    _key_clear->Bind(EPDGUI_Button::EVENT_RELEASED, notes_clear_cb);

    exitbtn("Home");
    _canvas_title->drawString("Notes", 270, 34);

    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, &Frame_Base::exit_cb);
}

Frame_Notes::~Frame_Notes(void) {
    delete notes_canvas;
    notes_canvas = NULL;
    delete _key_save;
    delete _key_clear;
    for (int i = 0; i < kNumSlots; i++) {
        delete _key_slot[i];
    }
}

int Frame_Notes::init(epdgui_args_vector_t &args) {
    _is_run = 1;
    EPDGUI_SetAutoUpdate(false);
    notes_pen_down = false;
    notes_last_x = -1;
    notes_last_y = -1;

    NotesLoad(notes_slot);

    M5.EPD.Clear();
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);
    for (int i = 0; i < kNumSlots; i++) {
        EPDGUI_AddObject(_key_slot[i]);
    }
    EPDGUI_AddObject(_key_save);
    EPDGUI_AddObject(_key_clear);
    EPDGUI_AddObject(_key_exit);
    notes_canvas->pushCanvas(0, kCanvasTop, UPDATE_MODE_NONE);
    return 8;
}

int Frame_Notes::run(void) {
    bool up = M5.TP.isFingerUp();
    if (up) {
        if (notes_pen_down) {
            // DU4 trades contrast for speed while actively drawing; settle
            // with one full-quality refresh once the stroke actually ends
            // rather than paying that cost on every sample.
            notes_canvas->pushCanvas(0, kCanvasTop, UPDATE_MODE_GL16);
        }
        notes_pen_down = false;
        notes_last_x = -1;
        notes_last_y = -1;
        return _is_run;
    }

    int16_t x = M5.TP.readFingerX(0);
    int16_t y = M5.TP.readFingerY(0);

    if ((y < kCanvasTop) || (y >= kCanvasTop + kCanvasHeight) || (x < 0) || (x >= kCanvasWidth)) {
        notes_pen_down = false;
        notes_last_x = -1;
        notes_last_y = -1;
        return _is_run;
    }

    if ((x == notes_last_x) && (y == notes_last_y)) {
        return _is_run;
    }

    int16_t cx = x;
    int16_t cy = y - kCanvasTop;
    int16_t prev_cx = cx, prev_cy = cy;
    static const int16_t kPenRadius = 2; // ~2x the previous 1px line

    if (notes_pen_down) {
        prev_cx = notes_last_x;
        prev_cy = notes_last_y - kCanvasTop;
        // Stamp filled circles along the segment instead of a thin drawLine()
        // so the whole stroke is a uniform thickness, not just the endpoints.
        int16_t dx = cx - prev_cx;
        int16_t dy = cy - prev_cy;
        int steps = (int)sqrtf((float)dx * dx + (float)dy * dy) / 2;
        for (int i = 1; i <= steps; i++) {
            notes_canvas->fillCircle(prev_cx + dx * i / steps, prev_cy + dy * i / steps, kPenRadius, 15);
        }
     } else {
        notes_pen_down = true;
    }
    notes_canvas->fillCircle(cx, cy, kPenRadius, 15);
    NotesPushRegion(prev_cx, prev_cy, cx, cy);

    notes_last_x = x;
    notes_last_y = y;
    return _is_run;
}

void Frame_Notes::exit(void) {
    NotesSave(notes_slot);
}
