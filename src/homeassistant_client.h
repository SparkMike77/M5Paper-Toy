#ifndef _HOMEASSISTANT_CLIENT_H_
#define _HOMEASSISTANT_CLIENT_H_

#include <Arduino.h>
#include <HTTPClient.h>

// Joins the configured Home Assistant base URL with a REST path, e.g.
// HomeAssistantUrl("/api/shopping_list").
String HomeAssistantUrl(const String& path);

// Adds the Authorization (if a token is configured) and Content-Type
// headers every Home Assistant REST call needs.
void ApplyHomeAssistantHeaders(HTTPClient& http);

#endif //_HOMEASSISTANT_CLIENT_H_
