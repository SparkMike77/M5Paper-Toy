#include "frame_home.h"
#include "../wifi_power.h"
#include "../homeassistant_client.h"
#include <WiFi.h>
#include <HTTPClient.h>

void Frame_Home::InitSwitch(EPDGUI_Switch* sw, String title, String subtitle, const uint8_t *img1, const uint8_t *img2) {
    memcpy(sw->Canvas(0)->frameBuffer(), ImageResource_home_button_background_228x228, 228 * 228 / 2);
    sw->Canvas(0)->setTextSize(36);
    sw->Canvas(0)->setTextDatum(TC_DATUM);
    sw->Canvas(0)->drawString(title, 114, 136);
    sw->Canvas(0)->setTextSize(26);
    sw->Canvas(0)->drawString(subtitle, 114, 183);
    memcpy(sw->Canvas(1)->frameBuffer(), sw->Canvas(0)->frameBuffer(), 228 * 228 / 2);
    sw->Canvas(0)->pushImage(68, 20, 92, 92, img1);
    sw->Canvas(1)->pushImage(68, 20, 92, 92, img2);
}

// Used when the matching /HomeAssistant/EntityN.txt is missing or blank -
// a plain "?" instead of a title/icon nobody configured, and disabled so
// tapping it can't fire a service call with an empty entity_id.
void Frame_Home::InitSwitchUnconfigured(EPDGUI_Switch* sw) {
    memcpy(sw->Canvas(0)->frameBuffer(), ImageResource_home_button_background_228x228, 228 * 228 / 2);
    sw->Canvas(0)->setTextSize(72);
    sw->Canvas(0)->setTextDatum(CC_DATUM);
    sw->Canvas(0)->drawString("?", 114, 114);
    memcpy(sw->Canvas(1)->frameBuffer(), sw->Canvas(0)->frameBuffer(), 228 * 228 / 2);
    sw->SetEnable(false);
}

void key_home_air_adjust_cb(epdgui_args_vector_t &args) {
    int operation = ((EPDGUI_Button*)(args[0]))->GetCustomString().toInt();
    EPDGUI_Switch *sw = ((EPDGUI_Switch*)(args[1]));
    if (sw->getState() == 0) {
        return;
    }
    int temp = sw->GetCustomString().toInt();
    char buf[10];
    if (operation == 1) {
        temp++;
        
     } else {
        temp--;
    }
    sprintf(buf, "%d", temp);
    sw->SetCustomString(buf);
    sprintf(buf, "%d℃", temp);
    sw->Canvas(1)->setTextSize(36);
    sw->Canvas(1)->setTextDatum(TC_DATUM);
    sw->Canvas(1)->fillRect(114 - 100, 108, 200, 38, 0);
    sw->Canvas(1)->drawString(buf, 114, 108);
    sw->Canvas(1)->pushCanvas(sw->getX(), sw->getY(), UPDATE_MODE_A2);
}

void key_home_air_state0_cb(epdgui_args_vector_t &args) {
    EPDGUI_Button *b1 = ((EPDGUI_Button*)(args[0]));
    EPDGUI_Button *b2 = ((EPDGUI_Button*)(args[1]));
    b1->SetEnable(false);
    b2->SetEnable(false);
}

void key_home_air_state1_cb(epdgui_args_vector_t &args) {
    EPDGUI_Button *b1 = ((EPDGUI_Button*)(args[0]));
    EPDGUI_Button *b2 = ((EPDGUI_Button*)(args[1]));
    b1->SetEnable(true);
    b2->SetEnable(true);
}

void key_home_hass_toggle_cb(epdgui_args_vector_t &args) {
    HomeAssistantSwitchBinding* binding = (HomeAssistantSwitchBinding*)(args[0]);
    if (binding == NULL || binding->sw == NULL) {
        return;
    }

    bool enabled = binding->sw->getState() == 1;
    if (binding->frame == NULL) {
        return;
    }

    binding->frame->SetHomeAssistantState(binding, enabled);
}

void Frame_Home::ApplyOfflineVisualState() {
    if (_offline_visual_applied) {
        return;
    }

    _canvas_title->fillCanvas(0);
    _canvas_title->drawFastHLine(0, 64, 540, 15);
    _canvas_title->drawFastHLine(0, 63, 540, 15);
    _canvas_title->drawFastHLine(0, 62, 540, 15);
    _canvas_title->setTextSize(26);
    _canvas_title->setTextDatum(CC_DATUM);
    _canvas_title->setTextColor(15);
    _canvas_title->drawString("Home Assistant Offline", 270, 34);
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);

    _sw_light1->Canvas(0)->ReverseColor();
    _sw_light1->Canvas(1)->ReverseColor();
    _sw_light2->Canvas(0)->ReverseColor();
    _sw_light2->Canvas(1)->ReverseColor();
    _sw_socket1->Canvas(0)->ReverseColor();
    _sw_socket1->Canvas(1)->ReverseColor();
    _sw_socket2->Canvas(0)->ReverseColor();
    _sw_socket2->Canvas(1)->ReverseColor();
    _sw_air_1->Canvas(0)->ReverseColor();
    _sw_air_1->Canvas(1)->ReverseColor();
    _sw_air_2->Canvas(0)->ReverseColor();
    _sw_air_2->Canvas(1)->ReverseColor();

    _key_air_1_plus->CanvasNormal()->ReverseColor();
    _key_air_1_plus->CanvasPressed()->ReverseColor();
    _key_air_1_minus->CanvasNormal()->ReverseColor();
    _key_air_1_minus->CanvasPressed()->ReverseColor();
    _key_air_2_plus->CanvasNormal()->ReverseColor();
    _key_air_2_plus->CanvasPressed()->ReverseColor();
    _key_air_2_minus->CanvasNormal()->ReverseColor();
    _key_air_2_minus->CanvasPressed()->ReverseColor();

    _key_exit->CanvasNormal()->ReverseColor();
    _key_exit->CanvasPressed()->ReverseColor();

    _offline_visual_applied = true;
}

bool Frame_Home::RefreshHomeAssistantState(HomeAssistantSwitchBinding* binding) {
    if (binding == NULL || binding->sw == NULL || binding->entity_id == NULL
            || binding->entity_id[0] == '\0' || binding->domain == NULL) {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    WifiPower_NoteActivity();

    HTTPClient http;
    WiFiClient client;
    http.setTimeout(3000);
    http.begin(client, HomeAssistantUrl(String("/api/states/") + binding->entity_id));
    ApplyHomeAssistantHeaders(http);

    int http_code = http.GET();
    if (http_code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    int state_pos = payload.indexOf("\"state\"");
    if (state_pos < 0) {
        return false;
    }

    int value_pos = payload.indexOf(':', state_pos);
    int value_end = payload.indexOf(',', value_pos);
    if (value_end < 0) {
        value_end = payload.indexOf('}', value_pos);
    }
    if (value_pos < 0 || value_end < 0) {
        return false;
    }

    String state_value = payload.substring(value_pos + 1, value_end);
    state_value.trim();
    state_value.remove(0, 1);
    state_value.remove(state_value.length() - 1, 1);

    bool enabled = state_value.equalsIgnoreCase("on") || state_value.equalsIgnoreCase("true");
    binding->sw->setState(enabled ? 1 : 0);
    return true;
}

bool Frame_Home::SetHomeAssistantState(HomeAssistantSwitchBinding* binding, bool enabled) {
    if (binding == NULL || binding->sw == NULL || binding->entity_id == NULL
            || binding->entity_id[0] == '\0' || binding->domain == NULL) {
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    WifiPower_NoteActivity();

    String service = enabled ? "turn_on" : "turn_off";
    String payload = String("{\"entity_id\":\"") + binding->entity_id + "\"}";
    HTTPClient http;
    WiFiClient client;
    http.setTimeout(3000);
    http.begin(client, HomeAssistantUrl(String("/api/services/") + binding->domain + "/" + service));
    ApplyHomeAssistantHeaders(http);

    int http_code = http.POST(payload);
    bool success = http_code >= 200 && http_code < 300;
    http.end();

    if (!success) {
        binding->sw->setState(enabled ? 0 : 1);
        return false;
    }

    return RefreshHomeAssistantState(binding);
}

Frame_Home::Frame_Home(void) {
    _frame_name = "Frame_Home";
    _home_assistant_online = false;
    _offline_visual_applied = false;

    _sw_light1       = new EPDGUI_Switch(2, 20, 44 + 72, 228, 228);
    _sw_light2       = new EPDGUI_Switch(2, 288, 44 + 72, 228, 228);
    _sw_socket1      = new EPDGUI_Switch(2, 20, 324 + 72, 228, 228);
    _sw_socket2      = new EPDGUI_Switch(2, 288, 324 + 72, 228, 228);
    _sw_air_1        = new EPDGUI_Switch(2, 20, 604 + 72, 228, 184);
    _sw_air_2        = new EPDGUI_Switch(2, 288, 604 + 72, 228, 184);
    _key_air_1_plus  = new EPDGUI_Button(20 + 116, 604 + 72 + 184, 112, 44);
    _key_air_1_minus = new EPDGUI_Button(20, 604 + 72 + 184, 116, 44);
    _key_air_2_plus  = new EPDGUI_Button(288 + 116, 604 + 72 + 184, 112, 44);
    _key_air_2_minus = new EPDGUI_Button(288, 604 + 72 + 184, 116, 44);

    _key_air_1_plus ->SetCustomString("1");
    _key_air_1_minus->SetCustomString("0");
    _key_air_2_plus ->SetCustomString("1");
    _key_air_2_minus->SetCustomString("0");
    _key_air_1_plus->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, _key_air_1_plus);
    _key_air_1_plus->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, _sw_air_1);
    _key_air_1_plus->Bind(EPDGUI_Button::EVENT_RELEASED, key_home_air_adjust_cb);
    _key_air_1_minus->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, _key_air_1_minus);
    _key_air_1_minus->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, _sw_air_1);
    _key_air_1_minus->Bind(EPDGUI_Button::EVENT_RELEASED, key_home_air_adjust_cb);
    _key_air_2_plus->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, _key_air_2_plus);
    _key_air_2_plus->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, _sw_air_2);
    _key_air_2_plus->Bind(EPDGUI_Button::EVENT_RELEASED, key_home_air_adjust_cb);
    _key_air_2_minus->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, _key_air_2_minus);
    _key_air_2_minus->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, _sw_air_2);
    _key_air_2_minus->Bind(EPDGUI_Button::EVENT_RELEASED, key_home_air_adjust_cb);

    M5EPD_Canvas canvas_temp(&M5.EPD);
    canvas_temp.createRender(36);

    // Populated from /HomeAssistant/Entity0-3.txt at boot (LoadHomeAssistant
    // EntitiesFromSD) - stored here rather than taken fresh from
    // GetHomeAssistantEntity() below so entity_id below can point at a
    // String that lives as long as this object, instead of a temporary's
    // buffer that would already be gone by the time it's read.
    for (int i = 0; i < 4; i++) {
        _entity_ids[i] = GetHomeAssistantEntity(i);
    }

    if (_entity_ids[0].isEmpty()) {
        InitSwitchUnconfigured(_sw_light1);
    } else {
        InitSwitch(_sw_light1, "Ceiling Light", "Living Room", ImageResource_home_icon_light_off_92x92, ImageResource_home_icon_light_on_92x92);
    }
    if (_entity_ids[1].isEmpty()) {
        InitSwitchUnconfigured(_sw_light2);
    } else {
        InitSwitch(_sw_light2, "Table Lamp", "Bedroom", ImageResource_home_icon_light_off_92x92, ImageResource_home_icon_light_on_92x92);
    }
    if (_entity_ids[2].isEmpty()) {
        InitSwitchUnconfigured(_sw_socket1);
    } else {
        InitSwitch(_sw_socket1, "Rice Cooker", "Kitchen", ImageResource_home_icon_socket_off_92x92, ImageResource_home_icon_socket_on_92x92);
    }
    if (_entity_ids[3].isEmpty()) {
        InitSwitchUnconfigured(_sw_socket2);
    } else {
        InitSwitch(_sw_socket2, "Computer", "Bedroom", ImageResource_home_icon_socket_off_92x92, ImageResource_home_icon_socket_on_92x92);
    }

    memcpy(_sw_air_1->Canvas(0)->frameBuffer(), ImageResource_home_air_background_228x184, 228 * 184 / 2);
    _sw_air_1->Canvas(0)->setTextDatum(TC_DATUM);
    _sw_air_1->Canvas(0)->setTextSize(26);
    _sw_air_1->Canvas(0)->drawString("Bedroom", 114, 152);
    memcpy(_sw_air_1->Canvas(1)->frameBuffer(), _sw_air_1->Canvas(0)->frameBuffer(), 228 * 184 / 2);
    _sw_air_1->Canvas(0)->setTextSize(36);
    _sw_air_1->Canvas(0)->drawString("OFF", 114, 108);
    _sw_air_1->Canvas(1)->setTextSize(36);
    _sw_air_1->Canvas(1)->setTextDatum(TC_DATUM);
    _sw_air_1->Canvas(1)->drawString("26℃", 114, 108);
    _sw_air_1->SetCustomString("26");

    memcpy(_sw_air_2->Canvas(0)->frameBuffer(), ImageResource_home_air_background_228x184, 228 * 184 / 2);
    _sw_air_2->Canvas(0)->setTextDatum(TC_DATUM);
    _sw_air_2->Canvas(0)->setTextSize(26);
    _sw_air_2->Canvas(0)->drawString("Living Room", 114, 152);
    memcpy(_sw_air_2->Canvas(1)->frameBuffer(), _sw_air_2->Canvas(0)->frameBuffer(), 228 * 184 / 2);
    _sw_air_2->Canvas(0)->setTextSize(36);
    _sw_air_2->Canvas(0)->drawString("OFF", 114, 108);
    _sw_air_2->Canvas(1)->setTextSize(36);
    _sw_air_2->Canvas(1)->setTextDatum(TC_DATUM);
    _sw_air_2->Canvas(1)->drawString("26℃", 114, 108);
    _sw_air_2->SetCustomString("26");

    memcpy(_key_air_1_plus->CanvasNormal()->frameBuffer(), ImageResource_home_air_background_r_112x44, 112 * 44 / 2);
    memcpy(_key_air_1_plus->CanvasPressed()->frameBuffer(), _key_air_1_plus->CanvasNormal()->frameBuffer(), 112 * 44 / 2);
    _key_air_1_plus->CanvasPressed()->ReverseColor();
    memcpy(_key_air_2_plus->CanvasNormal()->frameBuffer(), ImageResource_home_air_background_r_112x44, 112 * 44 / 2);
    memcpy(_key_air_2_plus->CanvasPressed()->frameBuffer(), _key_air_2_plus->CanvasNormal()->frameBuffer(), 112 * 44 / 2);
    _key_air_2_plus->CanvasPressed()->ReverseColor();
    memcpy(_key_air_1_minus->CanvasNormal()->frameBuffer(), ImageResource_home_air_background_l_116x44, 116 * 44 / 2);
    memcpy(_key_air_1_minus->CanvasPressed()->frameBuffer(), _key_air_1_minus->CanvasNormal()->frameBuffer(), 116 * 44 / 2);
    _key_air_1_minus->CanvasPressed()->ReverseColor();
    memcpy(_key_air_2_minus->CanvasNormal()->frameBuffer(), ImageResource_home_air_background_l_116x44, 116 * 44 / 2);
    memcpy(_key_air_2_minus->CanvasPressed()->frameBuffer(), _key_air_2_minus->CanvasNormal()->frameBuffer(), 116 * 44 / 2);
    _key_air_2_minus->CanvasPressed()->ReverseColor();

    _key_air_1_plus->SetEnable(false);
    _key_air_2_plus->SetEnable(false);
    _key_air_1_minus->SetEnable(false);
    _key_air_2_minus->SetEnable(false);

    _sw_air_1->Canvas(0)->pushImage(68, 12, 92, 92, ImageResource_home_icon_conditioner_off_92x92);
    _sw_air_1->Canvas(1)->pushImage(68, 12, 92, 92, ImageResource_home_icon_conditioner_on_92x92);
    _sw_air_2->Canvas(0)->pushImage(68, 12, 92, 92, ImageResource_home_icon_conditioner_off_92x92);
    _sw_air_2->Canvas(1)->pushImage(68, 12, 92, 92, ImageResource_home_icon_conditioner_on_92x92);

    _sw_air_1->AddArgs(EPDGUI_Switch::EVENT_NONE, 0, _key_air_1_plus);
    _sw_air_1->AddArgs(EPDGUI_Switch::EVENT_NONE, 1, _key_air_1_minus);
    _sw_air_1->AddArgs(EPDGUI_Switch::EVENT_NONE, 2, _sw_air_1);
    _sw_air_1->Bind(0, key_home_air_state0_cb);
    _sw_air_1->AddArgs(EPDGUI_Switch::EVENT_PRESSED, 0, _key_air_1_plus);
    _sw_air_1->AddArgs(EPDGUI_Switch::EVENT_PRESSED, 1, _key_air_1_minus);
    _sw_air_1->AddArgs(EPDGUI_Switch::EVENT_PRESSED, 2, _sw_air_1);
    _sw_air_1->Bind(1, key_home_air_state1_cb);

    _sw_air_2->AddArgs(EPDGUI_Switch::EVENT_NONE, 0, _key_air_2_plus);
    _sw_air_2->AddArgs(EPDGUI_Switch::EVENT_NONE, 1, _key_air_2_minus);
    _sw_air_2->AddArgs(EPDGUI_Switch::EVENT_NONE, 2, _sw_air_2);
    _sw_air_2->Bind(0, key_home_air_state0_cb);
    _sw_air_2->AddArgs(EPDGUI_Switch::EVENT_PRESSED, 0, _key_air_2_plus);
    _sw_air_2->AddArgs(EPDGUI_Switch::EVENT_PRESSED, 1, _key_air_2_minus);
    _sw_air_2->AddArgs(EPDGUI_Switch::EVENT_PRESSED, 2, _sw_air_2);
    _sw_air_2->Bind(1, key_home_air_state1_cb);

    _ha_switches[0] = { _sw_light1, _entity_ids[0].c_str(), "light", this };
    _ha_switches[1] = { _sw_light2, _entity_ids[1].c_str(), "light", this };
    _ha_switches[2] = { _sw_socket1, _entity_ids[2].c_str(), "switch", this };
    _ha_switches[3] = { _sw_socket2, _entity_ids[3].c_str(), "switch", this };

    for (int i = 0; i < 4; i++) {
        _ha_switches[i].sw->SetCustomData(&_ha_switches[i]);
        _ha_switches[i].sw->AddArgs(EPDGUI_Switch::EVENT_PRESSED, 0, &_ha_switches[i]);
        _ha_switches[i].sw->Bind(1, key_home_hass_toggle_cb);
        // EVENT_NONE/EVENT_PRESSED (0/1) doubles as the state index here,
        // matching the pattern the air-conditioner switches above use -
        // state 0's args must be added under EVENT_NONE, not EVENT_PRESSED
        // again, or state 0's args array stays empty and toggling a switch
        // off (state 1 -> 0) reads args[0] on an empty vector.
        _ha_switches[i].sw->AddArgs(EPDGUI_Switch::EVENT_NONE, 0, &_ha_switches[i]);
        _ha_switches[i].sw->Bind(0, key_home_hass_toggle_cb);
    }

    exitbtn("Home");
    _canvas_title->drawString("Control Panel", 270, 34);

    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, &Frame_Base::exit_cb);
}

void Frame_Home::exit(void) {
    // Restarts the idle countdown from a full 60 seconds at the moment
    // this screen closes, rather than from whenever the last HA request
    // happened to fire while it was still open.
    WifiPower_NoteActivity();
}

Frame_Home::~Frame_Home(void) {
    delete _sw_light1;
    delete _sw_light2;
    delete _sw_socket1;
    delete _sw_socket2;
    delete _sw_air_1;
    delete _sw_air_2;
    delete _key_air_1_plus;
    delete _key_air_1_minus;
    delete _key_air_2_plus;
    delete _key_air_2_minus;
}

int Frame_Home::init(epdgui_args_vector_t &args) {
    _is_run = 1;
    // Reconnects (blocking briefly, like the boot-time connect) if Wi-Fi
    // power save had disconnected us since this screen was last open.
    WifiPower_Reconnect();

    // "Online" used to be decided by a separate unauthenticated probe hit
    // against /api/ before ever making a real call - which is exactly the
    // kind of repeated failed-auth request Home Assistant's ip_ban watches
    // for, and got this device's IP banned. Deriving it from whether any of
    // the real, already-authenticated state calls below succeed avoids that
    // extra request entirely instead of just fixing its headers.
    _home_assistant_online = false;
    if (WiFi.status() == WL_CONNECTED) {
        for (int i = 0; i < 4; i++) {
            if (RefreshHomeAssistantState(&_ha_switches[i])) {
                _home_assistant_online = true;
            }
        }
    }

    M5.EPD.Clear();

    _canvas_title->fillCanvas(0);
    _canvas_title->drawFastHLine(0, 64, 540, 15);
    _canvas_title->drawFastHLine(0, 63, 540, 15);
    _canvas_title->drawFastHLine(0, 62, 540, 15);
    _canvas_title->setTextSize(26);
    _canvas_title->setTextDatum(CC_DATUM);
    _canvas_title->setTextColor(15);
    _canvas_title->drawString(_home_assistant_online ? "Control Panel" : "Home Assistant Offline", 270, 34);
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);

    if (!_home_assistant_online) {
        ApplyOfflineVisualState();
    }

    EPDGUI_AddObject(_sw_light1);
    EPDGUI_AddObject(_sw_light2);
    EPDGUI_AddObject(_sw_socket1);
    EPDGUI_AddObject(_sw_socket2);
    EPDGUI_AddObject(_sw_air_1);
    EPDGUI_AddObject(_sw_air_2);
    EPDGUI_AddObject(_key_air_1_plus);
    EPDGUI_AddObject(_key_air_1_minus);
    EPDGUI_AddObject(_key_air_2_plus);
    EPDGUI_AddObject(_key_air_2_minus);
    EPDGUI_AddObject(_key_exit);
    return 3;
}