#ifndef _UPLOAD_SERVER_H_
#define _UPLOAD_SERVER_H_

// Background HTTP file-upload server for getting files onto the SD card
// over WiFi. Call UploadServer_Loop() often (it's a cheap no-op unless
// actually running) - it starts/stops the underlying WebServer to track
// the "upload enabled" setting and current WiFi state, and pumps
// handleClient() while running.
void UploadServer_Loop(void);

#endif //_UPLOAD_SERVER_H_
