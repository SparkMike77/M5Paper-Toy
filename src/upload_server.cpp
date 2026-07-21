#include "upload_server.h"
#include "global_setting.h"
#include <WebServer.h>
#include <WiFi.h>
#include <SD.h>

static WebServer server(80);
static File upload_file;
static bool server_running = false;
static bool handlers_registered = false;

static void HandleIndex() {
    String html =
        "<!DOCTYPE html><html><head><title>M5Paper Upload</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>body{font-family:sans-serif;max-width:520px;margin:2em auto;padding:0 1em}"
        "li{margin:4px 0}</style></head><body>"
        "<h2>M5Paper File Upload</h2>"
        "<form method='POST' action='/upload' enctype='multipart/form-data'>"
        "<input type='file' name='file'> "
        "<input type='submit' value='Upload'></form>"
        "<hr><h3>Files on SD card (/)</h3><ul>";

    if (!GetInitStatus(0)) {
        html += "<li><em>SD card not available.</em></li>";
     } else {
        File root = SD.open("/");
        if (root) {
            File f = root.openNextFile();
            while (f) {
                if (!f.isDirectory()) {
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
        String filename = upload.filename;
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        upload_file = SD.open(filename, FILE_WRITE);
     } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (upload_file) {
            upload_file.write(upload.buf, upload.currentSize);
        }
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

static void EnsureHandlersRegistered() {
    if (handlers_registered) {
        return;
    }
    server.on("/", HTTP_GET, HandleIndex);
    server.on("/upload", HTTP_POST, HandleUploadDone, HandleUploadData);
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
