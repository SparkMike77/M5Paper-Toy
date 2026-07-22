#include "pdf_book.h"
#include "miniz.h"
#include <cctype>

static const size_t NPOS = (size_t)-1;

// Well over the size of any PDF this device is realistically going to be
// asked to open - the whole file is held in one PSRAM buffer while it's
// being read, so this keeps a runaway/corrupt file from exhausting memory.
static const size_t kMaxPdfSize = 8 * 1024 * 1024;

bool PdfBook::IsWs(uint8_t c) {
    return (c == 0x00) || (c == 0x09) || (c == 0x0A) || (c == 0x0C) || (c == 0x0D) || (c == 0x20);
}

bool PdfBook::IsDelim(uint8_t c) {
    switch (c) {
        case '(': case ')': case '<': case '>': case '[': case ']':
        case '{': case '}': case '/': case '%':
            return true;
        default:
            return false;
    }
}

void PdfBook::SkipWs(const uint8_t *buf, size_t size, size_t &pos) {
    while (pos < size) {
        if (IsWs(buf[pos])) {
            pos++;
         } else if (buf[pos] == '%') {
            while ((pos < size) && (buf[pos] != '\n') && (buf[pos] != '\r')) {
                pos++;
            }
         } else {
            break;
        }
    }
}

size_t PdfBook::FindLiteral(const uint8_t *buf, size_t size, size_t from, const char *needle) {
    size_t needleLen = strlen(needle);
    if (needleLen == 0 || size < needleLen) {
        return NPOS;
    }
    for (size_t i = from; i + needleLen <= size; i++) {
        if (memcmp(buf + i, needle, needleLen) == 0) {
            return i;
        }
    }
    return NPOS;
}

// `pos` must point at the opening '<' of a "<<" dict marker. Returns the
// position just after the matching closing "&gt;&gt;", correctly skipping
// over nested dicts/arrays and any literal/hex strings in between (which
// may themselves contain '<' or '>' bytes unrelated to dict nesting).
size_t PdfBook::SkipBalancedDict(const uint8_t *buf, size_t size, size_t pos) {
    int depth = 0;
    while (pos < size) {
        uint8_t c = buf[pos];
        if (c == '%') {
            while ((pos < size) && (buf[pos] != '\n') && (buf[pos] != '\r')) {
                pos++;
            }
         } else if (c == '(') {
            SkipLiteralStringRaw(buf, size, pos);
         } else if (c == '<') {
            if ((pos + 1 < size) && (buf[pos + 1] == '<')) {
                depth++;
                pos += 2;
             } else {
                pos++;
                while ((pos < size) && (buf[pos] != '>')) {
                    pos++;
                }
                if (pos < size) {
                    pos++;
                }
            }
         } else if (c == '>') {
            if ((pos + 1 < size) && (buf[pos + 1] == '>')) {
                depth--;
                pos += 2;
                if (depth == 0) {
                    return pos;
                }
             } else {
                pos++;
            }
         } else {
            pos++;
        }
    }
    return pos;
}

void PdfBook::SkipLiteralStringRaw(const uint8_t *buf, size_t size, size_t &pos) {
    // pos at '('
    int depth = 1;
    pos++;
    while ((pos < size) && (depth > 0)) {
        uint8_t c = buf[pos];
        if (c == '\\') {
            pos += 2;
            continue;
        }
        if (c == '(') {
            depth++;
         } else if (c == ')') {
            depth--;
        }
        pos++;
    }
}

bool PdfBook::ReadInt(const uint8_t *buf, size_t size, size_t &pos, long &out) {
    size_t start = pos;
    bool neg = false;
    if ((pos < size) && ((buf[pos] == '+') || (buf[pos] == '-'))) {
        neg = (buf[pos] == '-');
        pos++;
    }
    size_t digitsStart = pos;
    long v = 0;
    while ((pos < size) && isdigit(buf[pos])) {
        v = v * 10 + (buf[pos] - '0');
        pos++;
    }
    if (pos == digitsStart) {
        pos = start;
        return false;
    }
    out = neg ? -v : v;
    return true;
}

String PdfBook::ReadName(const uint8_t *buf, size_t size, size_t &pos) {
    // pos at '/'
    pos++;
    String out;
    while ((pos < size) && !IsWs(buf[pos]) && !IsDelim(buf[pos])) {
        out += (char)buf[pos];
        pos++;
    }
    return out;
}

// Scans a dict's direct (depth-0) keys for `key`, returning the byte
// position where its value begins, or NPOS if not present. dictStart
// points at '<<', dictEnd just after the matching '>>'.
size_t PdfBook::FindKeyValue(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key) const {
    if ((dictStart + 2 > dictEnd) || (dictEnd > size)) {
        return NPOS;
    }
    size_t pos = dictStart + 2;
    size_t contentEnd = dictEnd - 2;
    int depth = 0;
    String keyStr(key);

    while (pos < contentEnd) {
        SkipWs(buf, contentEnd, pos);
        if (pos >= contentEnd) {
            break;
        }
        uint8_t c = buf[pos];
        if (c == '(') {
            SkipLiteralStringRaw(buf, size, pos);
         } else if (c == '<') {
            if ((pos + 1 < size) && (buf[pos + 1] == '<')) {
                depth++;
                pos += 2;
             } else {
                pos++;
                while ((pos < size) && (buf[pos] != '>')) {
                    pos++;
                }
                if (pos < size) {
                    pos++;
                }
            }
         } else if (c == '>') {
            if ((pos + 1 < size) && (buf[pos + 1] == '>')) {
                depth--;
                pos += 2;
             } else {
                pos++;
            }
         } else if (c == '[') {
            depth++;
            pos++;
         } else if (c == ']') {
            depth--;
            pos++;
         } else if (c == '/') {
            String name = ReadName(buf, size, pos);
            if ((depth == 0) && (name == keyStr)) {
                SkipWs(buf, size, pos);
                return pos;
            }
         } else {
            // number, keyword (true/false/null), or reference "R" - skip as one token
            while ((pos < size) && !IsWs(buf[pos]) && !IsDelim(buf[pos])) {
                pos++;
            }
        }
    }
    return NPOS;
}

String PdfBook::GetName(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key) const {
    size_t pos = FindKeyValue(buf, size, dictStart, dictEnd, key);
    if ((pos == NPOS) || (pos >= size) || (buf[pos] != '/')) {
        return "";
    }
    return ReadName(buf, size, pos);
}

bool PdfBook::GetInt(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key, long &out) const {
    size_t pos = FindKeyValue(buf, size, dictStart, dictEnd, key);
    if (pos == NPOS) {
        return false;
    }
    return ReadInt(buf, size, pos, out);
}

bool PdfBook::GetIntOrRef(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key, long &out) const {
    size_t pos = FindKeyValue(buf, size, dictStart, dictEnd, key);
    if (pos == NPOS) {
        return false;
    }
    long first;
    if (!ReadInt(buf, size, pos, first)) {
        return false;
    }
    size_t save = pos;
    SkipWs(buf, size, pos);
    long gen;
    if (ReadInt(buf, size, pos, gen)) {
        SkipWs(buf, size, pos);
        if ((pos < size) && (buf[pos] == 'R') && ((pos + 1 >= size) || IsWs(buf[pos + 1]) || IsDelim(buf[pos + 1]))) {
            // Indirect reference - resolve to the (plain integer) target object.
            const uint8_t *refBuf; size_t refSize; size_t refValPos;
            if (ResolveObject((int)first, &refBuf, &refSize, &refValPos)) {
                size_t p = refValPos;
                long v;
                if (ReadInt(refBuf, refSize, p, v)) {
                    out = v;
                    return true;
                }
            }
            return false;
        }
    }
    pos = save;
    out = first;
    return true;
}

bool PdfBook::GetRef(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key, int &outNum) const {
    size_t pos = FindKeyValue(buf, size, dictStart, dictEnd, key);
    if (pos == NPOS) {
        return false;
    }
    long num;
    if (!ReadInt(buf, size, pos, num)) {
        return false;
    }
    SkipWs(buf, size, pos);
    long gen;
    if (!ReadInt(buf, size, pos, gen)) {
        return false;
    }
    SkipWs(buf, size, pos);
    if ((pos < size) && (buf[pos] == 'R') && ((pos + 1 >= size) || IsWs(buf[pos + 1]) || IsDelim(buf[pos + 1]))) {
        outNum = (int)num;
        return true;
    }
    return false;
}

bool PdfBook::GetArrayRange(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, const char *key, size_t &outStart, size_t &outEnd) const {
    size_t pos = FindKeyValue(buf, size, dictStart, dictEnd, key);
    if ((pos == NPOS) || (pos >= size) || (buf[pos] != '[')) {
        return false;
    }
    size_t p = pos + 1;
    int depth = 1;
    while ((p < size) && (depth > 0)) {
        uint8_t c = buf[p];
        if (c == '(') {
            SkipLiteralStringRaw(buf, size, p);
         } else if (c == '<') {
            if ((p + 1 < size) && (buf[p + 1] == '<')) {
                size_t e = SkipBalancedDict(buf, size, p);
                p = e;
             } else {
                p++;
                while ((p < size) && (buf[p] != '>')) {
                    p++;
                }
                if (p < size) {
                    p++;
                }
            }
         } else if (c == '[') {
            depth++;
            p++;
         } else if (c == ']') {
            depth--;
            p++;
         } else {
            p++;
        }
    }
    outStart = pos + 1;
    outEnd = (p > 0) ? (p - 1) : p; // p-1 excludes the closing ']'
    return true;
}

void PdfBook::ParseRefList(const uint8_t *buf, size_t size, size_t rangeStart, size_t rangeEnd, std::vector<int> &out) const {
    size_t pos = rangeStart;
    while (pos < rangeEnd) {
        SkipWs(buf, rangeEnd, pos);
        if (pos >= rangeEnd) {
            break;
        }
        long num;
        size_t before = pos;
        if (!ReadInt(buf, rangeEnd, pos, num)) {
            pos = before + 1;
            continue;
        }
        SkipWs(buf, rangeEnd, pos);
        long gen;
        size_t genPos = pos;
        if (!ReadInt(buf, rangeEnd, pos, gen)) {
            continue;
        }
        SkipWs(buf, rangeEnd, pos);
        if ((pos < rangeEnd) && (buf[pos] == 'R') && ((pos + 1 >= rangeEnd) || IsWs(buf[pos + 1]) || IsDelim(buf[pos + 1]))) {
            out.push_back((int)num);
            pos++;
         } else {
            pos = genPos;
        }
    }
}

bool PdfBook::GetContentsRefs(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, std::vector<int> &out) const {
    size_t pos = FindKeyValue(buf, size, dictStart, dictEnd, "Contents");
    if (pos == NPOS) {
        return false;
    }
    if (buf[pos] == '[') {
        size_t arrStart, arrEnd;
        if (!GetArrayRange(buf, size, dictStart, dictEnd, "Contents", arrStart, arrEnd)) {
            return false;
        }
        ParseRefList(buf, size, arrStart, arrEnd, out);
        return true;
    }
    long num;
    if (!ReadInt(buf, size, pos, num)) {
        return false;
    }
    SkipWs(buf, size, pos);
    long gen;
    if (!ReadInt(buf, size, pos, gen)) {
        return false;
    }
    SkipWs(buf, size, pos);
    if ((pos < size) && (buf[pos] == 'R')) {
        out.push_back((int)num);
        return true;
    }
    return false;
}

bool PdfBook::ResolveObject(int num, const uint8_t **outBuf, size_t *outSize, size_t *outValuePos) const {
    std::map<int, ObjLoc>::const_iterator it = _objects.find(num);
    if (it == _objects.end()) {
        return false;
    }
    const ObjLoc &loc = it->second;
    if (loc.inStream) {
        if ((loc.streamBufIndex < 0) || (loc.streamBufIndex >= (int)_objStreamBuffers.size())) {
            return false;
        }
        const ObjStreamBuf &sb = _objStreamBuffers[loc.streamBufIndex];
        *outBuf = sb.data;
        *outSize = sb.size;
        *outValuePos = loc.offset;
     } else {
        *outBuf = _data;
        *outSize = _size;
        *outValuePos = loc.offset;
    }
    return true;
}

// Given a stream object's dict range, locates and (if needed) decompresses
// its stream data. *outOwned tells the caller whether *outData needs
// mz_free()'d (FlateDecode - a fresh heap buffer) or not (stored filter,
// points directly into `buf`).
bool PdfBook::ResolveStream(const uint8_t *buf, size_t size, size_t dictStart, size_t dictEnd, uint8_t **outData, size_t *outSize, bool *outOwned) const {
    size_t p = dictEnd;
    SkipWs(buf, size, p);
    if ((p + 6 > size) || (memcmp(buf + p, "stream", 6) != 0)) {
        return false;
    }
    p += 6;
    if ((p < size) && (buf[p] == '\r')) {
        p++;
    }
    if ((p < size) && (buf[p] == '\n')) {
        p++;
    }

    long length;
    if (!GetIntOrRef(buf, size, dictStart, dictEnd, "Length", length)) {
        return false;
    }
    if ((length < 0) || (p + (size_t)length > size)) {
        return false;
    }

    const uint8_t *raw = buf + p;
    size_t rawLen = (size_t)length;
    String filter = GetName(buf, size, dictStart, dictEnd, "Filter");

    if (filter == "FlateDecode") {
        size_t outLen = 0;
        void *decompressed = tinfl_decompress_mem_to_heap(raw, rawLen, &outLen, TINFL_FLAG_PARSE_ZLIB_HEADER);
        if (decompressed == NULL) {
            return false;
        }
        *outData = (uint8_t*)decompressed;
        *outSize = outLen;
        *outOwned = true;
        return true;
    }
    if (filter.length() == 0) {
        *outData = (uint8_t*)raw;
        *outSize = rawLen;
        *outOwned = false;
        return true;
    }
    return false; // unsupported filter (e.g. DCTDecode, LZWDecode, a filter array, ...)
}

PdfBook::PdfBook() : _data(NULL), _size(0) {
}

PdfBook::~PdfBook() {
    Close();
}

void PdfBook::Close() {
    if (_data != NULL) {
        free(_data);
        _data = NULL;
    }
    _size = 0;
    for (size_t i = 0; i < _objStreamBuffers.size(); i++) {
        mz_free(_objStreamBuffers[i].data);
    }
    _objStreamBuffers.clear();
    _objects.clear();
    _pages.clear();
}

void PdfBook::ScanObjects() {
    size_t pos = 0;
    while (pos + 3 <= _size) {
        size_t objPos = NPOS;
        for (size_t i = pos; i + 3 <= _size; i++) {
            if ((_data[i] == 'o') && (_data[i + 1] == 'b') && (_data[i + 2] == 'j')) {
                bool precededByWs = (i > 0) && IsWs(_data[i - 1]);
                bool followedOk = (i + 3 >= _size) || IsWs(_data[i + 3]) || IsDelim(_data[i + 3]);
                if (precededByWs && followedOk) {
                    objPos = i;
                    break;
                }
            }
        }
        if (objPos == NPOS) {
            break;
        }

        size_t p = objPos;
        while ((p > 0) && IsWs(_data[p - 1])) {
            p--;
        }
        size_t genEnd = p;
        while ((p > 0) && isdigit(_data[p - 1])) {
            p--;
        }
        size_t genStart = p;
        while ((p > 0) && IsWs(_data[p - 1])) {
            p--;
        }
        size_t numEnd = p;
        while ((p > 0) && isdigit(_data[p - 1])) {
            p--;
        }
        size_t numStart = p;

        if ((genStart == genEnd) || (numStart == numEnd)) {
            pos = objPos + 3;
            continue;
        }

        long objNum = 0;
        for (size_t i = numStart; i < numEnd; i++) {
            objNum = objNum * 10 + (_data[i] - '0');
        }

        size_t valuePos = objPos + 3;
        SkipWs(_data, _size, valuePos);

        ObjLoc loc;
        loc.inStream = false;
        loc.offset = valuePos;
        _objects[(int)objNum] = loc;

        size_t streamKw = FindLiteral(_data, _size, valuePos, "stream");
        size_t endobjKw = FindLiteral(_data, _size, valuePos, "endobj");

        if ((streamKw != NPOS) && ((endobjKw == NPOS) || (streamKw < endobjKw))) {
            size_t afterKw = streamKw + 6;
            if ((afterKw < _size) && (_data[afterKw] == '\r')) {
                afterKw++;
            }
            if ((afterKw < _size) && (_data[afterKw] == '\n')) {
                afterKw++;
            }
            size_t endstreamKw = FindLiteral(_data, _size, afterKw, "endstream");
            if (endstreamKw != NPOS) {
                pos = endstreamKw + 9;
             } else {
                pos = _size;
            }
         } else if (endobjKw != NPOS) {
            pos = endobjKw + 6;
         } else {
            pos = _size;
        }
    }
}

void PdfBook::UnpackObjectStreams() {
    std::vector<int> objNums;
    for (std::map<int, ObjLoc>::iterator it = _objects.begin(); it != _objects.end(); ++it) {
        objNums.push_back(it->first);
    }

    for (size_t oi = 0; oi < objNums.size(); oi++) {
        const uint8_t *buf; size_t size; size_t valPos;
        if (!ResolveObject(objNums[oi], &buf, &size, &valPos)) {
            continue;
        }
        if ((valPos + 2 > size) || (buf[valPos] != '<') || (buf[valPos + 1] != '<')) {
            continue;
        }
        size_t dictEnd = SkipBalancedDict(buf, size, valPos);

        if (GetName(buf, size, valPos, dictEnd, "Type") != "ObjStm") {
            continue;
        }

        long n, first;
        if (!GetInt(buf, size, valPos, dictEnd, "N", n) || !GetInt(buf, size, valPos, dictEnd, "First", first)) {
            continue;
        }

        uint8_t *decoded; size_t decodedLen; bool owned;
        if (!ResolveStream(buf, size, valPos, dictEnd, &decoded, &decodedLen, &owned)) {
            continue;
        }

        uint8_t *ownedBuf;
        if (owned) {
            ownedBuf = decoded;
         } else {
            ownedBuf = (uint8_t*)ps_malloc(decodedLen);
            if (ownedBuf == NULL) {
                continue;
            }
            memcpy(ownedBuf, decoded, decodedLen);
        }

        int bufIndex = (int)_objStreamBuffers.size();
        ObjStreamBuf entry;
        entry.data = ownedBuf;
        entry.size = decodedLen;
        _objStreamBuffers.push_back(entry);

        size_t hp = 0;
        for (long i = 0; i < n; i++) {
            SkipWs(ownedBuf, decodedLen, hp);
            long onum;
            if (!ReadInt(ownedBuf, decodedLen, hp, onum)) {
                break;
            }
            SkipWs(ownedBuf, decodedLen, hp);
            long ooff;
            if (!ReadInt(ownedBuf, decodedLen, hp, ooff)) {
                break;
            }

            if (_objects.find((int)onum) == _objects.end()) {
                size_t objOffset = (size_t)first + (size_t)ooff;
                if (objOffset < decodedLen) {
                    ObjLoc loc;
                    loc.inStream = true;
                    loc.offset = objOffset;
                    loc.streamBufIndex = bufIndex;
                    _objects[(int)onum] = loc;
                }
            }
        }
    }
}

bool PdfBook::FindRootPages(int &outPagesNum) const {
    int rootNum = -1;

    size_t lastTrailer = NPOS;
    size_t searchFrom = 0;
    while (true) {
        size_t found = FindLiteral(_data, _size, searchFrom, "trailer");
        if (found == NPOS) {
            break;
        }
        lastTrailer = found;
        searchFrom = found + 7;
    }

    if (lastTrailer != NPOS) {
        size_t pos = lastTrailer + 7;
        SkipWs(_data, _size, pos);
        if ((pos + 2 <= _size) && (_data[pos] == '<') && (_data[pos + 1] == '<')) {
            size_t dictEnd = SkipBalancedDict(_data, _size, pos);
            int num;
            if (GetRef(_data, _size, pos, dictEnd, "Root", num)) {
                rootNum = num;
            }
        }
    }

    if (rootNum < 0) {
        for (std::map<int, ObjLoc>::const_iterator it = _objects.begin(); it != _objects.end(); ++it) {
            const uint8_t *buf; size_t size; size_t valPos;
            if (!ResolveObject(it->first, &buf, &size, &valPos)) {
                continue;
            }
            if ((valPos + 2 > size) || (buf[valPos] != '<') || (buf[valPos + 1] != '<')) {
                continue;
            }
            size_t dictEnd = SkipBalancedDict(buf, size, valPos);
            if (GetName(buf, size, valPos, dictEnd, "Type") == "Catalog") {
                rootNum = it->first;
                break;
            }
        }
    }

    if (rootNum < 0) {
        return false;
    }

    const uint8_t *buf; size_t size; size_t valPos;
    if (!ResolveObject(rootNum, &buf, &size, &valPos)) {
        return false;
    }
    if ((valPos + 2 > size) || (buf[valPos] != '<') || (buf[valPos + 1] != '<')) {
        return false;
    }
    size_t dictEnd = SkipBalancedDict(buf, size, valPos);

    int pagesNum;
    if (!GetRef(buf, size, valPos, dictEnd, "Pages", pagesNum)) {
        return false;
    }
    outPagesNum = pagesNum;
    return true;
}

void PdfBook::CollectPages(int objNum, int depth) {
    if (depth > 64) {
        return;
    }
    const uint8_t *buf; size_t size; size_t valPos;
    if (!ResolveObject(objNum, &buf, &size, &valPos)) {
        return;
    }
    if ((valPos + 2 > size) || (buf[valPos] != '<') || (buf[valPos + 1] != '<')) {
        return;
    }
    size_t dictEnd = SkipBalancedDict(buf, size, valPos);

    size_t kidsStart, kidsEnd;
    if (GetArrayRange(buf, size, valPos, dictEnd, "Kids", kidsStart, kidsEnd)) {
        std::vector<int> kids;
        ParseRefList(buf, size, kidsStart, kidsEnd, kids);
        for (size_t i = 0; i < kids.size(); i++) {
            CollectPages(kids[i], depth + 1);
        }
     } else {
        _pages.push_back(objNum);
    }
}

bool PdfBook::Open(const String &path) {
    Close();

    File file = SD.open(path, FILE_READ);
    if (!file) {
        return false;
    }
    size_t fileSize = file.size();
    if ((fileSize == 0) || (fileSize > kMaxPdfSize)) {
        file.close();
        return false;
    }

    uint8_t *buf = (uint8_t*)ps_malloc(fileSize);
    if (buf == NULL) {
        file.close();
        return false;
    }
    size_t readBytes = file.read(buf, fileSize);
    file.close();
    if (readBytes != fileSize) {
        free(buf);
        return false;
    }

    _data = buf;
    _size = fileSize;

    ScanObjects();
    UnpackObjectStreams();

    int pagesNum;
    if (!FindRootPages(pagesNum)) {
        Close();
        return false;
    }
    CollectPages(pagesNum, 0);

    if (_pages.size() == 0) {
        Close();
        return false;
    }
    return true;
}

// --- content-stream text extraction ---

static char PassthroughByte(uint8_t b) {
    if ((b >= 0x20) && (b <= 0x7E)) {
        return (char)b;
    }
    if ((b == '\n') || (b == '\r')) {
        return '\n';
    }
    return '?';
}

String PdfBook::DecodeLiteralString(const uint8_t *buf, size_t size, size_t &pos) {
    // pos at '('
    pos++;
    String out;
    int depth = 1;
    while ((pos < size) && (depth > 0)) {
        uint8_t c = buf[pos];
        if (c == '\\') {
            pos++;
            if (pos >= size) {
                break;
            }
            uint8_t e = buf[pos];
            switch (e) {
                case 'n': out += '\n'; pos++; break;
                case 'r': out += '\n'; pos++; break;
                case 't': out += ' '; pos++; break;
                case 'b': pos++; break;
                case 'f': pos++; break;
                case '(': out += '('; pos++; break;
                case ')': out += ')'; pos++; break;
                case '\\': out += '\\'; pos++; break;
                case '\r':
                    pos++;
                    if ((pos < size) && (buf[pos] == '\n')) {
                        pos++;
                    }
                    break;
                case '\n':
                    pos++;
                    break;
                default:
                    if (isdigit(e)) {
                        int val = 0;
                        int n = 0;
                        while ((pos < size) && isdigit(buf[pos]) && (n < 3)) {
                            val = val * 8 + (buf[pos] - '0');
                            pos++;
                            n++;
                        }
                        out += PassthroughByte((uint8_t)val);
                     } else {
                        out += PassthroughByte(e);
                        pos++;
                    }
                    break;
            }
            continue;
        }
        if (c == '(') {
            depth++;
            out += '(';
            pos++;
            continue;
        }
        if (c == ')') {
            depth--;
            pos++;
            if (depth > 0) {
                out += ')';
            }
            continue;
        }
        out += PassthroughByte(c);
        pos++;
    }
    return out;
}

String PdfBook::DecodeHexString(const uint8_t *buf, size_t size, size_t &pos) {
    // pos at '<' (already confirmed not '<<')
    pos++;
    String out;
    int hi = -1;
    while ((pos < size) && (buf[pos] != '>')) {
        uint8_t c = buf[pos];
        int digit = -1;
        if ((c >= '0') && (c <= '9')) digit = c - '0';
        else if ((c >= 'a') && (c <= 'f')) digit = c - 'a' + 10;
        else if ((c >= 'A') && (c <= 'F')) digit = c - 'A' + 10;
        if (digit >= 0) {
            if (hi < 0) {
                hi = digit;
             } else {
                out += PassthroughByte((uint8_t)((hi << 4) | digit));
                hi = -1;
            }
        }
        pos++;
    }
    if (hi >= 0) {
        out += PassthroughByte((uint8_t)(hi << 4));
    }
    if (pos < size) {
        pos++; // consume '>'
    }
    return out;
}

void PdfBook::ExtractTextFromContentStream(const uint8_t *buf, size_t size, String &out) const {
    size_t pos = 0;
    std::vector<String> pending;

    while (pos < size) {
        SkipWs(buf, size, pos);
        if (pos >= size) {
            break;
        }
        uint8_t c = buf[pos];

        if (c == '(') {
            pending.clear();
            pending.push_back(DecodeLiteralString(buf, size, pos));
            continue;
        }
        if (c == '<') {
            if ((pos + 1 < size) && (buf[pos + 1] == '<')) {
                pos = SkipBalancedDict(buf, size, pos);
             } else {
                pending.clear();
                pending.push_back(DecodeHexString(buf, size, pos));
            }
            continue;
        }
        if (c == '[') {
            pos++;
            std::vector<String> collected;
            while ((pos < size) && (buf[pos] != ']')) {
                SkipWs(buf, size, pos);
                if ((pos < size) && (buf[pos] == '(')) {
                    collected.push_back(DecodeLiteralString(buf, size, pos));
                 } else if ((pos < size) && (buf[pos] == '<') && !((pos + 1 < size) && (buf[pos + 1] == '<'))) {
                    collected.push_back(DecodeHexString(buf, size, pos));
                 } else if (pos < size) {
                    while ((pos < size) && !IsWs(buf[pos]) && (buf[pos] != ']')) {
                        pos++;
                    }
                }
            }
            if (pos < size) {
                pos++; // consume ']'
            }
            pending = collected;
            continue;
        }
        if (c == '/') {
            ReadName(buf, size, pos);
            continue;
        }
        if ((c == '{') || (c == '}')) {
            pos++;
            continue;
        }

        size_t tokenStart = pos;
        while ((pos < size) && !IsWs(buf[pos]) && !IsDelim(buf[pos])) {
            pos++;
        }
        if (pos == tokenStart) {
            pos++;
            continue;
        }
        size_t tokenLen = pos - tokenStart;

        bool isTj = (tokenLen == 2) && (buf[tokenStart] == 'T') && (buf[tokenStart + 1] == 'j');
        bool isTJ = (tokenLen == 2) && (buf[tokenStart] == 'T') && (buf[tokenStart + 1] == 'J');
        bool isQuote = (tokenLen == 1) && ((buf[tokenStart] == '\'') || (buf[tokenStart] == '"'));
        bool isNewLineOp = (tokenLen >= 2) && (buf[tokenStart] == 'T') &&
                           ((buf[tokenStart + 1] == 'd') || (buf[tokenStart + 1] == 'D') ||
                            (buf[tokenStart + 1] == '*') || (buf[tokenStart + 1] == 'm'));

        if (isTj || isTJ || isQuote) {
            for (size_t i = 0; i < pending.size(); i++) {
                out += pending[i];
            }
         } else if (isNewLineOp) {
            if ((out.length() > 0) && (out.charAt(out.length() - 1) != '\n')) {
                out += '\n';
            }
        }
        pending.clear();
    }
}

String PdfBook::GetPageText(int index) const {
    if ((index < 0) || (index >= (int)_pages.size())) {
        return "";
    }
    int pageObjNum = _pages[index];
    const uint8_t *buf; size_t size; size_t valPos;
    if (!ResolveObject(pageObjNum, &buf, &size, &valPos)) {
        return "";
    }
    if ((valPos + 2 > size) || (buf[valPos] != '<') || (buf[valPos + 1] != '<')) {
        return "";
    }
    size_t dictEnd = SkipBalancedDict(buf, size, valPos);

    std::vector<int> contentRefs;
    if (!GetContentsRefs(buf, size, valPos, dictEnd, contentRefs)) {
        return "";
    }

    String text;
    for (size_t i = 0; i < contentRefs.size(); i++) {
        const uint8_t *cbuf; size_t csize; size_t cvalPos;
        if (!ResolveObject(contentRefs[i], &cbuf, &csize, &cvalPos)) {
            continue;
        }
        if ((cvalPos + 2 > csize) || (cbuf[cvalPos] != '<') || (cbuf[cvalPos + 1] != '<')) {
            continue;
        }
        size_t cdictEnd = SkipBalancedDict(cbuf, csize, cvalPos);

        uint8_t *decoded; size_t decodedLen; bool owned;
        if (!ResolveStream(cbuf, csize, cvalPos, cdictEnd, &decoded, &decodedLen, &owned)) {
            continue;
        }
        ExtractTextFromContentStream(decoded, decodedLen, text);
        if (owned) {
            mz_free(decoded);
        }
        if ((text.length() > 0) && (text.charAt(text.length() - 1) != '\n')) {
            text += '\n';
        }
    }
    return text;
}
