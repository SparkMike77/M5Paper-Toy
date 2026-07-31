#include "upload_server.h"
#include "global_setting.h"
#include "wifi_power.h"
#include <WebServer.h>
#include <WiFi.h>
#include <SD.h>

static WebServer server(80);
static File upload_file;
static bool server_running = false;
static bool handlers_registered = false;

// Uploads, mkdir and move all accept paths with or without a leading '/'
// (curl callers tend to omit it), so normalize once instead of repeating
// this at every call site.
static String NormalizePath(String path) {
    if (!path.startsWith("/")) {
        path = "/" + path;
    }
    return path;
}

static void HandleIndex() {
    String html =
        "<!DOCTYPE html><html><head><title>M5Paper Upload</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>body{font-family:sans-serif;max-width:520px;margin:2em auto;padding:0 1em}"
        "li{margin:4px 0}form.inline{margin:8px 0}</style></head><body>"
        "<h2>M5Paper File Upload</h2>"
        "<form method='POST' action='/upload' enctype='multipart/form-data'>"
        "<input type='file' name='file'> "
        "<input type='submit' value='Upload'></form>"
        "<hr>"
        "<form class='inline' method='POST' action='/mkdir'>"
        "<input type='text' name='path' placeholder='/NewFolder'> "
        "<input type='submit' value='Create folder'></form>"
        "<form class='inline' method='POST' action='/move'>"
        "<input type='text' name='from' placeholder='/from/path'> "
        "<input type='text' name='to' placeholder='/to/path'> "
        "<input type='submit' value='Move / rename'></form>"
        "<hr><h3>Files on SD card (/)</h3><ul>";

    if (!GetInitStatus(0)) {
        html += "<li><em>SD card not available.</em></li>";
     } else {
        File root = SD.open("/");
        if (root) {
            File f = root.openNextFile();
            while (f) {
                if (f.isDirectory()) {
                    html += "<li>[" + String(f.name()) + "]</li>";
                 } else {
                    html += "<li>" + String(f.name()) + " (" + String(f.size() / 1024.0f, 1) + " KiB)</li>";
                }
                f = root.openNextFile();
            }
            root.close();
        }
    }
    html += "</ul></body></html>";
    server.send(200, "text/html", html);
}

static void HandleUploadData() {
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        // Silently fails (upload_file stays invalid, write below no-ops)
        // if the parent directory doesn't exist yet - create it first via
        // /mkdir.
        upload_file = SD.open(NormalizePath(upload.filename), FILE_WRITE);
     } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (upload_file) {
            upload_file.write(upload.buf, upload.currentSize);
        }
        // Keeps re-arming the idle-disconnect countdown for as long as
        // chunks keep arriving, so Wi-Fi power save never drops the
        // connection mid-transfer.
        WifiPower_NoteActivity();
     } else if (upload.status == UPLOAD_FILE_END) {
        if (upload_file) {
            upload_file.close();
        }
    }
}

static void HandleUploadDone() {
    server.sendHeader("Location", "/");
    server.send(303);
}

static void HandleMkdir() {
    if (server.hasArg("path") && !server.arg("path").isEmpty()) {
        SD.mkdir(NormalizePath(server.arg("path")));
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

static void HandleMove() {
    if (server.hasArg("from") && server.hasArg("to")
            && !server.arg("from").isEmpty() && !server.arg("to").isEmpty()) {
        SD.rename(NormalizePath(server.arg("from")), NormalizePath(server.arg("to")));
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

static void EnsureHandlersRegistered() {
    if (handlers_registered) {
        return;
    }
    server.on("/", HTTP_GET, HandleIndex);
    server.on("/upload", HTTP_POST, HandleUploadDone, HandleUploadData);
    server.on("/mkdir", HTTP_POST, HandleMkdir);
    server.on("/move", HTTP_POST, HandleMove);
    handlers_registered = true;
}

void UploadServer_Loop(void) {
    bool should_run = IsUploadServerEnabled() && (WiFi.status() == WL_CONNECTED);

    if (should_run && !server_running) {
        EnsureHandlersRegistered();
        server.begin();
        server_running = true;
     } else if (!should_run && server_running) {
        server.stop();
        server_running = false;
    }

    if (server_running) {
        server.handleClient();
    }
}
