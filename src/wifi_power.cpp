#include "wifi_power.h"
#include "global_setting.h"
#include <WiFi.h>

namespace {
const uint32_t kIdleTimeoutMs = 60000;
uint32_t last_activity_ms = 0;
bool started = false;
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
