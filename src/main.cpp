#include <M5EPD.h>
#include "epdgui/epdgui.h"
#include "frame/frame.h"
#include "systeminit.h"

// Default is 8KB (see arduino-esp32's main.cpp). The epub reader's tinyxml2
// parsing (container.xml/OPF/NCX) is a recursive-descent parser with fairly
// large per-call stack frames, which overflows the default stack on top of
// this app's own UI call depth - bump it via this framework-provided hook.
size_t getArduinoLoopTaskStackSize(void) {
    return 32768;
}

void setup() {
    pinMode(M5EPD_MAIN_PWR_PIN, OUTPUT);
    M5.enableMainPower();

    SysInit_Start();
}


void loop() {
    EPDGUI_MainLoop();
}
