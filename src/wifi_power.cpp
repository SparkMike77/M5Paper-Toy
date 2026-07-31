#include "wifi_power.h"
#include "global_setting.h"
#include <WiFi.h>

namespace {
const uint32_t kIdleTimeoutMs = 60000;
uint32_t last_activity_ms = 0;
bool started = false;
bool was_charging = false;

// No charge-status pin is exposed on this hardware, so USB-C power is
// inferred the same way the status bar infers it for its charging icon
// (frame_main.cpp) - a rise in battery voltage between polls - but tracked
// independently here since this needs to keep working no matter which
// frame is on screen, unlike the status bar which only samples while
// Frame_Main itself is drawing.
bool IsChargingViaUSB(void) {
    static uint32_t last_check_ms = 0;
    static uint32_t last_voltage = 0;
    static bool charging = false;

    uint32_t now = millis();
    if (last_check_ms != 0 && (now - last_check_ms) < 60000) {
        return charging;
    }
    last_check_ms = now;

    uint32_t voltage = M5.getBatteryVoltage();
    if (last_voltage > 0) {
        if (voltage > last_voltage + 5) {
            charging = true;
         } else if (voltage + 20 < last_voltage) {
            charging = false;
        }
    }
    last_voltage = voltage;
    return charging;
}
}

void WifiPower_NoteActivity(void) {
    last_activity_ms = millis();
    started = true;
}

void WifiPower_Loop(void) {
    if (!IsWifiPowerSaveEnabled()) {
        return;
    }
    if (!started) {
        // First tick after boot - start the countdown from here rather than
        // from an unset (zero) timestamp, which would read as "already 60s
        // idle" almost immediately.
        WifiPower_NoteActivity();
        was_charging = IsChargingViaUSB();
        return;
    }

    bool charging = IsChargingViaUSB();
    if (charging && !was_charging) {
        // Just plugged into USB-C - power save is off by default while
        // charging, so bring Wi-Fi back if the idle timeout had already
        // dropped it.
        WifiPower_Reconnect();
     } else if (!charging && was_charging) {
        // Just unplugged - resume the idle countdown from a full 60s
        // rather than from however long had already elapsed while charging.
        WifiPower_NoteActivity();
    }
    was_charging = charging;

    if (charging) {
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    if (millis() - last_activity_ms < kIdleTimeoutMs) {
        return;
    }
    log_d("Wi-Fi idle for %lums, disconnecting to save power", (unsigned long)kIdleTimeoutMs);
    WiFi.disconnect(true);
}

void WifiPower_Reconnect(void) {
    WifiPower_NoteActivity();
    if (!IsWifiPowerSaveEnabled() || !isWiFiConfiged()) {
        return;
    }
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(GetWifiSSID().c_str(), GetWifiPassword().c_str());
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 8000) {
        delay(10);
    }
}
