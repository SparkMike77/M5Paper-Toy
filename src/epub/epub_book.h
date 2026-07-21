#ifndef _EPUB_BOOK_H_
#define _EPUB_BOOK_H_

#include <Arduino.h>
#include <SD.h>
#include <vector>

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

    EpubBook();
    ~EpubBook();

    bool Open(const String &path);
    void Close();

    int ChapterCount() const { return (int)_chapters.size(); }
    String ChapterTitle(int index) const;

    // Extracts and HTML-strips chapter `index` into plain text.
    String GetChapterText(int index);

private:
    bool ReadEntryToString(const String &archivePath, String &out);
    bool ParseContainer(String &opfPath);
    bool ParseOpf(const String &opfPath);
    void ParseToc(const String &tocPath);

    void *_zip; // mz_zip_archive*, kept opaque so miniz.h isn't required here
    File _file;
    std::vector<Chapter> _chapters;
    String _opfDir;
};

#endif //_EPUB_BOOK_H_
