#include "frame_grocerylist.h"
#include "../wifi_power.h"
#include "../homeassistant_client.h"
#include <WiFi.h>
#include <HTTPClient.h>

// Matches frame_fileindex.cpp's row budget - same row height/spacing and
// the same amount of vertical space available below the title bar.
#define MAX_ITEMS 14

namespace {
// shopping_list items are flat objects (name/id/complete, no nesting), but
// a name could still contain '{', '}' or ',' inside quotes - track whether
// we're inside a quoted string so those don't get mistaken for structural
// characters while splitting the array into per-item substrings.
std::vector<String> SplitJsonObjects(const String& array_json) {
    std::vector<String> objects;
    int depth = 0;
    bool in_string = false;
    bool escaped = false;
    int obj_start = -1;
    for (size_t i = 0; i < array_json.length(); i++) {
        char c = array_json[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
             } else if (c == '\\') {
                escaped = true;
             } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
         } else if (c == '{') {
            if (depth == 0) {
                obj_start = i;
            }
            depth++;
         } else if (c == '}') {
            depth--;
            if (depth == 0 && obj_start >= 0) {
                objects.push_back(array_json.substring(obj_start, i + 1));
                obj_start = -1;
            }
        }
    }
    return objects;
}

// Not a general JSON parser - just enough to pull a string field out of the
// flat {"name":"...","id":"...","complete":false} shape the shopping_list
// API returns.
String ExtractJsonString(const String& obj, const String& key) {
    String needle = "\"" + key + "\"";
    int key_pos = obj.indexOf(needle);
    if (key_pos < 0) {
        return String();
    }
    int colon_pos = obj.indexOf(':', key_pos + needle.length());
    if (colon_pos < 0) {
        return String();
    }
    int quote_start = obj.indexOf('"', colon_pos);
    if (quote_start < 0) {
        return String();
    }
    String out;
    bool escaped = false;
    for (size_t i = quote_start + 1; i < obj.length(); i++) {
        char c = obj[i];
        if (escaped) {
            out += c;
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '"') {
            break;
        }
        out += c;
    }
    return out;
}

bool ExtractJsonBool(const String& obj, const String& key) {
    String needle = "\"" + key + "\"";
    int key_pos = obj.indexOf(needle);
    if (key_pos < 0) {
        return false;
    }
    int colon_pos = obj.indexOf(':', key_pos + needle.length());
    if (colon_pos < 0) {
        return false;
    }
    size_t i = colon_pos + 1;
    while (i < obj.length() && obj[i] == ' ') {
        i++;
    }
    return obj.substring(i, i + 4) == "true";
}

void key_grocerylist_commit_cb(epdgui_args_vector_t &args) {
    Frame_GroceryList* frame = (Frame_GroceryList*)(args[0]);
    int* is_run = (int*)(args[1]);
    frame->CommitChanges();
    // Ends this run() session without popping the frame off the stack, so
    // EPDGUI_MainLoop picks the same instance back up on the next tick and
    // calls init() again - the same rebuild-from-fresh-data path used the
    // first time this screen opened, just re-triggered in place.
    *is_run = 0;
}
}

Frame_GroceryList::Frame_GroceryList(void) {
    _frame_name = "Frame_GroceryList";
    _key_commit = new EPDGUI_Button("Commit", 540 - 8 - 140, 12, 140, 48);

    exitbtn("Home");
    _canvas_title->drawString("Grocery List", 270, 34);

    _key_exit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)(&_is_run));
    _key_exit->Bind(EPDGUI_Button::EVENT_RELEASED, &Frame_Base::exit_cb);

    _key_commit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 0, (void*)this);
    _key_commit->AddArgs(EPDGUI_Button::EVENT_RELEASED, 1, (void*)(&_is_run));
    _key_commit->Bind(EPDGUI_Button::EVENT_RELEASED, key_grocerylist_commit_cb);
}

Frame_GroceryList::~Frame_GroceryList(void) {
    ClearList();
    delete _key_commit;
}

void Frame_GroceryList::ClearList(void) {
    for (size_t i = 0; i < _key_items.size(); i++) {
        delete _key_items[i];
    }
    _key_items.clear();
    _items.clear();
}

void Frame_GroceryList::CommitChanges(void) {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    WifiPower_NoteActivity();

    for (size_t i = 0; i < _items.size() && i < _key_items.size(); i++) {
        bool checked = _key_items[i]->getState() == 1;
        HTTPClient http;
        WiFiClient client;
        http.setTimeout(3000);
        http.begin(client, HomeAssistantUrl(String("/api/shopping_list/item/") + _items[i].id));
        ApplyHomeAssistantHeaders(http);
        String body = String("{\"complete\":") + (checked ? "true" : "false") + "}";
        http.POST(body);
        http.end();
    }
}

void Frame_GroceryList::FetchAndBuildList(void) {
    ClearList();

    String message;
    if (WiFi.status() != WL_CONNECTED) {
        message = "Wi-Fi not connected";
     } else {
        WifiPower_NoteActivity();

        HTTPClient http;
        WiFiClient client;
        http.setTimeout(3000);
        http.begin(client, HomeAssistantUrl("/api/shopping_list"));
        ApplyHomeAssistantHeaders(http);

        int http_code = http.GET();
        if (http_code != HTTP_CODE_OK) {
            message = "Could not reach Home Assistant";
         } else {
            String payload = http.getString();
            std::vector<String> objects = SplitJsonObjects(payload);
            for (size_t i = 0; i < objects.size() && i < MAX_ITEMS; i++) {
                GroceryItem item;
                item.id = ExtractJsonString(objects[i], "id");
                item.name = ExtractJsonString(objects[i], "name");
                item.complete = ExtractJsonBool(objects[i], "complete");
                if (item.id.isEmpty() || item.name.isEmpty()) {
                    continue;
                }
                _items.push_back(item);
            }
            if (_items.empty()) {
                message = "No items in your list";
            }
        }
        http.end();
    }

    if (!message.isEmpty()) {
        EPDGUI_Switch *sw = new EPDGUI_Switch(1, 4, 100, 532, 61);
        sw->Canvas(0)->setTextSize(26);
        sw->Canvas(0)->setTextDatum(CC_DATUM);
        sw->Canvas(0)->setTextColor(15);
        sw->Canvas(0)->drawString(message, 266, 30);
        sw->SetEnable(false);
        _key_items.push_back(sw);
        EPDGUI_AddObject(sw);
        return;
    }

    for (size_t i = 0; i < _items.size(); i++) {
        String display_name = _items[i].name;
        if (display_name.length() > 30) {
            display_name = display_name.substring(0, 30) + "...";
        }

        EPDGUI_Switch *sw = new EPDGUI_Switch(2, 4, 100 + i * 60, 532, 61);
        sw->Canvas(0)->setTextSize(26);
        sw->Canvas(0)->setTextDatum(CL_DATUM);
        sw->Canvas(0)->setTextColor(15);
        sw->Canvas(0)->drawString(String("[ ] ") + display_name, 15, 30);
        sw->Canvas(1)->setTextSize(26);
        sw->Canvas(1)->setTextDatum(CL_DATUM);
        sw->Canvas(1)->setTextColor(15);
        sw->Canvas(1)->drawString(String("[X] ") + display_name, 15, 30);
        sw->setState(_items[i].complete ? 1 : 0);

        _key_items.push_back(sw);
        EPDGUI_AddObject(sw);
    }
}

int Frame_GroceryList::init(epdgui_args_vector_t &args) {
    _is_run = 1;
    // Reconnects (blocking briefly, like the boot-time connect) if Wi-Fi
    // power save had disconnected us since this screen was last open.
    WifiPower_Reconnect();

    M5.EPD.Clear();
    _canvas_title->pushCanvas(0, 8, UPDATE_MODE_NONE);

    FetchAndBuildList();

    EPDGUI_AddObject(_key_commit);
    EPDGUI_AddObject(_key_exit);
    return 3;
}

void Frame_GroceryList::exit(void) {
    // Restarts the idle countdown from a full 60 seconds at the moment
    // this screen closes, rather than from whenever the last HA request
    // happened to fire while it was still open.
    WifiPower_NoteActivity();
}
