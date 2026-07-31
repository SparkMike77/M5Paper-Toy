#ifndef _TILE_MAP_H_
#define _TILE_MAP_H_

#include <Arduino.h>
#include <SD.h>
#include <vector>

class M5EPD_Canvas;

// A directory of raster map tiles laid out as the standard "XYZ" slippy-map
// pyramid: <root>/<zoom>/<x>/<y>.jpg (or .png/.jpeg) - the format most
// offline-map tools (e.g. MOBAC) export. There's no metadata file read
// here; whatever integer-named subdirectories exist directly under `root`
// define the available zoom levels.
//
// Tiles are only ever drawn fully within the destination canvas - never
// partially off its edge - because M5EPD_Canvas's pushImage() does not
// clip a partially out-of-bounds source image; it indexes straight into
// the framebuffer; a source rect that starts or ends outside the canvas
// corrupts memory instead of clipping. Callers must keep whatever grid
// they build tile-aligned and fully inside the canvas they draw into.
class TileMap {
public:
    static const int kTileSize = 256;

    bool Open(const String &root);

    int ZoomLevelCount() const { return (int)_zoomLevels.size(); }
    int ZoomLevelAt(int index) const { return _zoomLevels[index]; }

    // Picks a starting tile roughly in the middle of whatever region is
    // actually present at the given zoom level (found by listing that
    // zoom's subdirectories/files), since there's no known real-world
    // location to center on without GPS.
    bool FindCenterTile(int zoom, long &outX, long &outY) const;

    // Draws the tile at (zoom, x, y) into `canvas` at pixel (px, py) -
    // which must be fully within the canvas - or a blank placeholder rect
    // if that tile isn't present (expected at the edges of the downloaded
    // region, not an error).
    void DrawTile(M5EPD_Canvas *canvas, int zoom, long x, long y, int32_t px, int32_t py) const;

private:
    bool FindTilePath(int zoom, long x, long y, String &outPath) const;

    String _root;
    std::vector<int> _zoomLevels;
};

#endif //_TILE_MAP_H_
