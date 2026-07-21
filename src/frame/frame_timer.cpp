#include "frame_timer.h"

// Plain countdown timer with quick-select duration presets. Counts down
// while RUNNING; stops (rather than looping or auto-restarting) at zero
// so the user notices it finished.
enum class TimerState { STOPPED, RUNNING, PAUSED };

static const uint16_t kPresetMinutes[8] = {5, 10, 15, 20, 25, 30, 45, 60};

static TimerState timer_state           = TimerState::STOPPED;
static uint32_t   timer_selected_seconds = 25 * 60;
static uint32_t   timer_remaining        = 25 * 60;
static uint32_t   timer_completed        = 0;
static uint32_t   timer_last_tick        = 0;

static void TimerRedraw(M5EPD_Canvas *canvas, m5epd_update_mode_t mode) {
    char buf[32];
    canvas->fillCanvas(0);
    canvas->drawFastHLine(0, 199, 540, 15);

    canvas->setTextDatum(CC_DATUM);
    sprintf(buf, "%02lu:%02lu", timer_remaining / 60, timer_remaining % 60);
    canvas->setTextSize(72);
    canvas->drawString(buf, 270, 90);

    sprintf(buf, "Completed: %lu", timer_completed);
    canvas->setTextSize(26);
    canvas->drawString(buf, 270, 170);

    canvas->pushCanvas(0, 80, mode);
}

static void timer_startpause_cb(epdgui_args_vector_t &args) {
    M5EPD_Canvas *canvas = (M5EPD_Canvas*)(args[0]);
    EPDGUI_Button *btn = (EPDGUI_Button*)(args[1]);

    if (timer_state == TimerState::RUNNING) {
        timer_state = TimerState::PAUSED;
        btn->setLabel("Resume");
     } else {
        if (timer_remaining == 0) {
            timer_remaining = timer_selected_seconds;
        }
        timer_state = TimerState::RUNNING;
        timer_last_tick = millis();
        btn->setLabel("Pause");
    }
    btn->Draw(UPDATE_MODE_GL16);
    TimerRedraw(canvas, UPDATE_MODE_DU4);
}

static void timer_reset_cb(epdgui_args_vector_t &args) {
    M5EPD_Canvas *canvas = (M5EPD_Canvas*)(args[0]);
    EPDGUI_Button *startpause = (EPDGUI_Button*)(args[1]);

    timer_state = TimerState::STOPPED;
    timer_remaining = timer_selected_seconds;
    startpause->setLabel("Start");
    startpause->Draw(UPDATE_MODE_GL16);
    TimerRedraw(canvas, UPDATE_MODE_DU4);
}

static void timer_preset_cb(epdgui_args_vector_t &args) {
    M5EPD_Canvas *canvas = (M5EPD_Canvas*)(args[0]);
    EPDGUI_Button *btn = (EPDGUI_Button*)(args[1]);
    EPDGUI_Button *startpause = (EPDGUI_Button*)(args[2]);

    timer_selected_seconds = (uint32_t)(btn->getLabel().toInt()) * 60;
    timer_remaining = timer_selected_seconds;
    timer_state = TimerState::STOPPED;
    startpause->setLabel("Start");
    startpause->Draw(UPDATE_MODE_GL16);
    TimerRedraw(canvas, UPDATE_MODE_DU4);
}

Frame_Timer::Frame_Timer(void) {
    _frame_name = "Frame_Timer";

    _display = new M5EPD_Canvas(&M5.EPD);
    _display->createCanvas(540, 200);
    _display->createRender(72);

    for (int i = 0; i < 8; i++) {
        int col = i % 4;
        int row = i / 4;
        char label[6];
        sprintf(label, "%dm", kPresetMinutes[i]);
        _key_preset[i] = new EPDGUI_Button(label, col * 135, 280 + row * 110, 135, 110);
    }

    _key_startpause = new EPDGUI_Button("Start", 0, 500, 270, 140);
    _key_reset      = new EPDGUI_Button("Reset", 270, 500, 270, 140);

    _key_startpause->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)_display);
    _key_startpause->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)_key_startpause);
    _key_startpause->Bind(EPDGUI_Button::EVENT_RELEASED, timer_startpause_cb);

    _key_reset->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)_display);
    _key_reset->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)_key_startpause);
    _key_reset->Bind(EPDGUI_Button::EVENT_RELEASED, timer_reset_cb);

    for (int i = 0; i < 8; i++) {
        _key_preset[i]->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)_display);
        _key_preset[i]->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)_key_preset[i]);
        _key_preset[i]->AddArgs(EPDGUI_Button::EVENT_RELEASED, 2, (void*)_key_startpause);
        _key_preset[i]->Bind(EPDGUI_Button::EVENT_RELEASED, timer_preset_cb);
    }

    exitbtn("Home");
    _canvas_title->drawString("Timer", 270, 34);

    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, &Frame_Base::exit_cb);
}

Frame_Timer::~Frame_Timer(void) {
    delete _display;
    delete _key_startpause;
    delete _key_reset;
    for (int i = 0; i < 8; i++) {
        delete _key_preset[i];
    }
}

int Frame_Timer::init(epdgui_args_vector_t &args) {
    _is_run = 1;
    EPDGUI_SetAutoUpdate(false);

    M5.EPD.Clear();
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);
    TimerRedraw(_display, UPDATE_MODE_NONE);
    for (int i = 0; i < 8; i++) {
        EPDGUI_AddObject(_key_preset[i]);
    }
    EPDGUI_AddObject(_key_startpause);
    EPDGUI_AddObject(_key_reset);
    EPDGUI_AddObject(_key_exit);
    return 11;
}

int Frame_Timer::run(void) {
    if (timer_state == TimerState::RUNNING) {
        uint32_t now = millis();
        if (now - timer_last_tick >= 1000) {
            timer_last_tick += 1000;
            if (timer_remaining > 0) {
                timer_remaining--;
            }
            if (timer_remaining == 0) {
                timer_completed++;
                timer_state = TimerState::STOPPED;
                _key_startpause->setLabel("Start");
                _key_startpause->Draw(UPDATE_MODE_GL16);
            }
            TimerRedraw(_display, UPDATE_MODE_DU4);
        }
    }
    return _is_run;
}
