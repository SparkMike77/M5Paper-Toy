#ifndef _WIFI_POWER_H_
#define _WIFI_POWER_H_

#include <Arduino.h>

// Call once per main loop iteration. When the "Wi-Fi Power Save" setting is
// on, disconnects Wi-Fi after 60 seconds with no WifiPower_NoteActivity()
// calls. A no-op when the setting is off, leaving Wi-Fi connected for the
// whole session like before this feature existed.
void WifiPower_Loop(void);

// Resets the 60-second idle countdown. Call whenever something is actively
// using the Wi-Fi connection (an in-flight HTTP request, a file upload
// still receiving chunks).
void WifiPower_NoteActivity(void);

// Reconnects Wi-Fi if it's currently off due to the idle timeout, then
// resets the countdown either way. Blocks for up to ~8s if a reconnect is
// actually needed (same timeout as the boot-time connect). A no-op if
// power save is off (Wi-Fi is expected to already be connected) or Wi-Fi
// was never configured.
void WifiPower_Reconnect(void);

#endif //_WIFI_POWER_H_
