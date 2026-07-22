#ifndef _EPUB_BOOK_H_
#define _EPUB_BOOK_H_

#include <Arduino.h>
#include <SD.h>
#include <vector>

class M5EPD_Canvas; // forward-declared to avoid pulling the display stack into this header

// Parses an EPUB (a zip archive containing OPF manifest/spine + optional
// NCX table of contents) and extracts chapter bodies as plain text.
// Zip access goes through miniz's custom I/O callback interface bound to
// an Arduino File, rather than miniz's stdio file path, since it's not
// certain that libc fopen() reaches files mounted via the SD.h/SPI driver
// on every arduino-esp32 core version - the File-based path is guaranteed
// to work because the rest of this codebase already relies on it.
class EpubBook {
public:
    struct Chapter {
        String href;  // path within the zip archive
        String title; // from the NCX toc if available, else derived later
    };

    // An <img> found in a chapter: `pos` is the character offset into that
    // chapter's plain text at which it occurred (images consume no text of
    // their own), and `href` is its path within the zip archive, already
    // resolved against the chapter's directory.
    struct ImageMarker {
        uint32_t pos;
        String href;
    };

    EpubBook();
    ~EpubBook();

    bool Open(const String &path);
    void Close();

    int ChapterCount() const { return (int)_chapters.size(); }
    String ChapterTitle(int index) const;

    // Extracts and HTML-strips chapter `index` into plain text. If `images`
    // is non-NULL, every <img> in the chapter is appended to it in order.
    String GetChapterText(int index, std::vector<ImageMarker> *images = NULL);

    // Decodes the image at zip-relative path `href` (as produced by
    // GetChapterText's ImageMarker) and draws it into `canvas` at (x, y),
    // clipped to (maxW, maxH). Supports JPEG and PNG. Returns false if the
    // entry is missing or isn't a decodable format.
    bool DrawImage(const String &href, M5EPD_Canvas *canvas, uint16_t x, uint16_t y, uint16_t maxW, uint16_t maxH);

private:
    bool ReadEntryToString(const String &archivePath, String &out);
    bool ReadEntryBinary(const String &archivePath, uint8_t **outBuf, size_t *outSize);
    bool ParseContainer(String &opfPath);
    bool ParseOpf(const String &opfPath);
    void ParseToc(const String &tocPath);

    void *_zip; // mz_zip_archive*, kept opaque so miniz.h isn't required here
    File _file;
    std::vector<Chapter> _chapters;
    String _opfDir;
};

#endif //_EPUB_BOOK_H_
