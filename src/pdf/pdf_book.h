#ifndef _PDF_BOOK_H_
#define _PDF_BOOK_H_

#include <Arduino.h>
#include <SD.h>
#include <vector>
#include <map>

// Extracts plain text from a PDF, page by page, for display on the same
// character-based reader used for EPUB/TXT. This is NOT a PDF renderer -
// it ignores original layout, fonts, vector graphics, and embedded images
// entirely, pulling only the string operands of each page's text-showing
// operators (Tj/TJ/'/") out of its content stream(s). Extraction quality
// depends heavily on how the PDF was produced:
//  - Simple, standard/WinAnsi-encoded text (the common case for text
//    exported from word processors, plain documents, etc.) extracts
//    cleanly, since those encodings map bytes 0x20-0x7E to plain ASCII.
//  - Documents using embedded CID-keyed fonts with custom (e.g.
//    Identity-H) encodings - common output from some PDF producers,
//    including some LaTeX/browser "print to PDF" paths - have no
//    relationship between their byte codes and ASCII/Unicode without a
//    ToUnicode CMap lookup, which isn't implemented here. Expect garbled
//    or missing text from those.
//  - Scanned/image-only pages yield nothing - there's no OCR.
//
// The whole file is read into one heap (PSRAM) buffer, and object lookup
// works by scanning for "N G obj" markers directly rather than parsing the
// xref table/stream. This tolerates malformed or incrementally-updated
// files (later "N G obj" occurrences always come after earlier ones, so
// keeping the last-scanned occurrence of a given object number reproduces
// "latest revision wins" correctly) without needing to understand classic
// xref tables or the newer xref-stream format at all. Objects packed into
// compressed object streams (/Type /ObjStm), as many modern producers do
// even for the page tree itself, are unpacked and indexed the same way.
class PdfBook {
public:
    PdfBook();
    ~PdfBook();

    bool Open(const String &path);
    void Close();

    int PageCount() const { return (int)_pages.size(); }
    String GetPageText(int index) const;

private:
    // Where an indirect object's value bytes live: either directly in the
    // main file buffer, or inside one of this book's decompressed object
    // stream buffers (when the object was packed via /Type /ObjStm).
    struct ObjLoc {
        bool inStream = false;
        size_t offset = 0;       // byte offset of the value, within whichever buffer applies
        int streamBufIndex = -1; // index into _objStreamBuffers when inStream
    };
    struct ObjStreamBuf {
        uint8_t *data;
        size_t size;
    };

    // --- low-level byte scanning helpers (operate on any buffer, not just _data) ---
    static bool IsWs(uint8_t c);
    static bool IsDelim(uint8_t c);
    static void SkipWs(const uint8_t *buf, size_t size, size_t &pos);
    static size_t FindLiteral(const uint8_t *buf, size_t size, size_t from, const char *needle);
    static size_t SkipBalancedDict(const uint8_t *buf, size_t size, size_t pos);
    static bool ReadInt(const uint8_t *buf, size_t size, size_t &pos, long &out);
    static String ReadName(const uint8_t *buf, size_t size, size_t &pos);

    // --- dict key lookups: dictStart points at '<<', dictEnd just after the matching '>>' ---
    size_t FindKeyValue(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key) const;
    String GetName(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key) const;
    bool GetInt(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key, long &out) const;
    bool GetIntOrRef(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key, long &out) const;
    bool GetRef(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key, int &outNum) const;
    bool GetArrayRange(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key, size_t &outStart, size_t &outEnd) const;
    void ParseRefList(const uint8_t *buf, size_t size, size_t rangeStart, size_t rangeEnd, std::vector<int> &out) const;
    bool GetContentsRefs(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, std::vector<int> &out) const;

    bool ResolveObject(int num, const uint8_t **outBuf, size_t *outSize, size_t *outValuePos) const;
    bool ResolveStream(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, uint8_t **outData, size_t *outSize, bool *outOwned) const;

    void ScanObjects();
    void UnpackObjectStreams();
    bool FindRootPages(int &outPagesNum) const;
    void CollectPages(int objNum, int depth);

    // --- content-stream text extraction ---
    void ExtractTextFromContentStream(const uint8_t *buf, size_t size, String &out) const;
    static String DecodeLiteralString(const uint8_t *buf, size_t size, size_t &pos);
    static String DecodeHexString(const uint8_t *buf, size_t size, size_t &pos);
    static void SkipLiteralStringRaw(const uint8_t *buf, size_t size, size_t &pos);

    std::map<int, ObjLoc> _objects;
    std::vector<ObjStreamBuf> _objStreamBuffers;
    std::vector<int> _pages; // object numbers of each page, in document order

    uint8_t *_data;
    size_t _size;
};

#endif //_PDF_BOOK_H_
