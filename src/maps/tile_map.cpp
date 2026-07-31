#include "tile_map.h"
#include <M5EPD_Canvas.h>
#include <algorithm>

static String BareName(File &f) {
    String name(f.name());
    int slash = name.lastIndexOf('/');
    if (slash >= 0) {
        name = name.substring(slash + 1);
    }
    return name;
}

static bool IsAllDigits(const String &s) {
    if (s.length() == 0) {
        return false;
    }
    for (size_t i = 0; i < s.length(); i++) {
        if (!isdigit((unsigned char)s.charAt(i))) {
            return false;
        }
    }
    return true;
}

bool TileMap::Open(const String &root) {
    _root = root;
    _zoomLevels.clear();

    File dir = SD.open(root);
    if (!dir || !dir.isDirectory()) {
        return false;
    }

    File entry = dir.openNextFile();
    while (entry) {
        if (entry.isDirectory()) {
            String name = BareName(entry);
            if (IsAllDigits(name)) {
                _zoomLevels.push_back(name.toInt());
            }
        }
        entry = dir.openNextFile();
    }
    std::sort(_zoomLevels.begin(), _zoomLevels.end());
    return _zoomLevels.size() > 0;
}

bool TileMap::FindCenterTile(int zoom, long &outX, long &outY) const {
    String zdir = _root + "/" + String(zoom);
    File dir = SD.open(zdir);
    if (!dir || !dir.isDirectory()) {
        return false;
    }

    std::vector<long> xs;
    File entry = dir.openNextFile();
    while (entry) {
        if (entry.isDirectory()) {
            String name = BareName(entry);
            if (IsAllDigits(name)) {
                xs.push_back(name.toInt());
            }
        }
        entry = dir.openNextFile();
    }
    if (xs.size() == 0) {
        return false;
    }
    std::sort(xs.begin(), xs.end());
    long midX = xs[xs.size() / 2];

    String xdir = zdir + "/" + String(midX);
    File xd = SD.open(xdir);
    if (!xd || !xd.isDirectory()) {
        return false;
    }
    std::vector<long> ys;
    File yentry = xd.openNextFile();
    while (yentry) {
        if (!yentry.isDirectory()) {
            String name = BareName(yentry);
            int dot = name.lastIndexOf('.');
            if (dot > 0) {
                name = name.substring(0, dot);
            }
            if (IsAllDigits(name)) {
                ys.push_back(name.toInt());
            }
        }
        yentry = xd.openNextFile();
    }
    if (ys.size() == 0) {
        return false;
    }
    std::sort(ys.begin(), ys.end());

    outX = midX;
    outY = ys[ys.size() / 2];
    return true;
}

bool TileMap::FindTilePath(int zoom, long x, long y, String &outPath) const {
    String base = _root + "/" + String(zoom) + "/" + String(x) + "/" + String(y);
    String jpg = base + ".jpg";
    if (SD.exists(jpg)) {
        outPath = jpg;
        return true;
    }
    String jpeg = base + ".jpeg";
    if (SD.exists(jpeg)) {
        outPath = jpeg;
        return true;
    }
    String png = base + ".png";
    if (SD.exists(png)) {
        outPath = png;
        return true;
    }
    return false;
}

void TileMap::DrawTile(M5EPD_Canvas *canvas, int zoom, long x, long y, int32_t px, int32_t py) const {
    String path;
    if ((x < 0) || (y < 0) || !FindTilePath(zoom, x, y, path)) {
        canvas->fillRect(px, py, kTileSize, kTileSize, 3);
        return;
    }

    // Note: the fs::FS&,String overloads of these two are declared in
    // M5EPD_Canvas.h but actually defined as free functions (missing the
    // M5EPD_Canvas:: qualifier) in the library's .cpp, so they never link -
    // use the const char* overloads instead.
    bool ok;
    if (path.endsWith(".png")) {
        ok = canvas->drawPngFile(SD, path.c_str(), px, py, kTileSize, kTileSize);
     } else {
        ok = canvas->drawJpgFile(SD, path.c_str(), px, py, kTileSize, kTileSize);
    }
    if (!ok) {
        canvas->fillRect(px, py, kTileSize, kTileSize, 3);
    }
}
