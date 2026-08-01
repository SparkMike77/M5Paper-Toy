#include "homeassistant_client.h"
#include "global_setting.h"

String HomeAssistantUrl(const String& path) {
    return GetHomeAssistantURL() + path;
}

void ApplyHomeAssistantHeaders(HTTPClient& http) {
    String token = GetHomeAssistantToken();
    if (!token.isEmpty()) {
        http.addHeader("Authorization", String("Bearer ") + token);
    }
    http.addHeader("Content-Type", "application/json");
}
