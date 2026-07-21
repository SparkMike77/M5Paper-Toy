#include "frame_calculator.h"

// Simple chained four-function calculator: each operator immediately
// applies whatever is pending, then holds the new operator (no operator
// precedence/parens - matches how a physical calculator behaves).
static String calc_display = "0";
static double calc_accum = 0;
static char calc_pending_op = 0;
static bool calc_start_new = true;
static bool calc_error = false;

static String calc_format(double v) {
    if (calc_error) {
        return "Error";
    }
    if (v == (double)(long long)v && fabs(v) < 1e15) {
        return String((long long)v);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6g", v);
    return String(buf);
}

static void calc_apply_pending(double operand) {
    switch (calc_pending_op) {
        case 0:   calc_accum = operand; break;
        case '+': calc_accum += operand; break;
        case '-': calc_accum -= operand; break;
        case 'x': calc_accum *= operand; break;
        case '/':
            if (operand == 0) {
                calc_error = true;
             } else {
                calc_accum /= operand;
            }
            break;
    }
}

static void calc_redraw(M5EPD_Canvas *canvas) {
    canvas->fillCanvas(0);
    canvas->drawFastHLine(0, 139, 540, 15);
    canvas->setTextDatum(CR_DATUM);
    canvas->setTextSize(60);
    canvas->drawString(calc_display, 520, 70);
    canvas->pushCanvas(0, 80, UPDATE_MODE_GL16);
}

static void calc_key_cb(epdgui_args_vector_t &args) {
    M5EPD_Canvas *canvas = (M5EPD_Canvas*)(args[0]);
    EPDGUI_Button *btn = (EPDGUI_Button*)(args[1]);
    String label = btn->getLabel();

    if (label == "C") {
        calc_display = "0";
        calc_accum = 0;
        calc_pending_op = 0;
        calc_start_new = true;
        calc_error = false;
     } else if (label == "<-") {
        if (calc_error || calc_display.length() <= 1) {
            calc_display = "0";
         } else {
            calc_display.remove(calc_display.length() - 1);
        }
     } else if (label == "=") {
        if (!calc_error) {
            calc_apply_pending(calc_display.toDouble());
            calc_pending_op = 0;
            calc_display = calc_format(calc_accum);
        }
        calc_start_new = true;
     } else if (label == "+" || label == "-" || label == "x" || label == "/") {
        if (!calc_error) {
            calc_apply_pending(calc_display.toDouble());
            calc_pending_op = label.charAt(0);
            calc_display = calc_format(calc_accum);
        }
        calc_start_new = true;
     } else if (label == ".") {
        if (calc_error) {
            calc_display = "0";
            calc_error = false;
        }
        if (calc_start_new) {
            calc_display = "0.";
            calc_start_new = false;
         } else if (calc_display.indexOf('.') < 0) {
            calc_display += ".";
        }
     } else {
        // digit 0-9
        if (calc_error) {
            calc_display = "";
            calc_error = false;
        }
        if (calc_start_new || calc_display == "0") {
            calc_display = label;
            calc_start_new = false;
         } else if (calc_display.length() < 14) {
            calc_display += label;
        }
    }

    calc_redraw(canvas);
}

Frame_Calculator::Frame_Calculator(void) {
    _frame_name = "Frame_Calculator";

    _display = new M5EPD_Canvas(&M5.EPD);
    _display->createCanvas(540, 140);
    _display->createRender(60);

    const char *kLabels[18] = {
        "7", "8", "9", "/",
        "4", "5", "6", "x",
        "1", "2", "3", "-",
        "C", "0", ".", "+",
        "<-", "="
    };

    // 4x4 grid of digits/operators, cell 135x130, starting below the display.
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            int i = row * 4 + col;
            _key[i] = new EPDGUI_Button(kLabels[i], col * 135, 240 + row * 130, 135, 130);
        }
    }
    // Bottom row: backspace and equals, double-width.
    _key[16] = new EPDGUI_Button(kLabels[16], 0, 760, 270, 130);
    _key[17] = new EPDGUI_Button(kLabels[17], 270, 760, 270, 130);

    for (int i = 0; i < 18; i++) {
        _key[i]->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)_display);
        _key[i]->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)_key[i]);
        _key[i]->Bind(EPDGUI_Button::EVENT_RELEASED, calc_key_cb);
    }

    exitbtn("Home");
    _canvas_title->drawString("Calculator", 270, 34);

    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, &Frame_Base::exit_cb);
}

Frame_Calculator::~Frame_Calculator(void) {
    delete _display;
    for (int i = 0; i < 18; i++) {
        delete _key[i];
    }
}

int Frame_Calculator::init(epdgui_args_vector_t &args) {
    _is_run = 1;
    calc_display = "0";
    calc_accum = 0;
    calc_pending_op = 0;
    calc_start_new = true;
    calc_error = false;

    M5.EPD.Clear();
    EPDGUI_SetAutoUpdate(false);
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);
    calc_redraw(_display);
    for (int i = 0; i < 18; i++) {
        EPDGUI_AddObject(_key[i]);
    }
    EPDGUI_AddObject(_key_exit);
    return 19;
}
