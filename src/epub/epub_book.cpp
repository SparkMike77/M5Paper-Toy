#include "epub_book.h"
#include "html_to_text.h"
#include "miniz.h"
#include <tinyxml2.h>

using namespace tinyxml2;

static String DirName(const String &path) {
    int slash = path.lastIndexOf('/');
    if (slash < 0) {
        return "";
    }
    return path.substring(0, slash + 1);
}

// Small path resolver: handles a relative path against a base directory,
// including a modest number of "../" segments. Covers the vast majority of
// real EPUB manifests, where content lives at or one level below the OPF.
static String ResolveRelative(const String &baseDir, const String &relPath) {
    if (relPath.startsWith("/")) {
        return relPath.substring(1);
    }
    String path = baseDir + relPath;
    while (true) {
        int up = path.indexOf("/../");
        if (up < 0) {
            break;
        }
        int prevSlash = path.lastIndexOf('/', up - 1);
        if (prevSlash < 0) {
            path = path.substring(up + 4);
         } else {
            path = path.substring(0, prevSlash + 1) + path.substring(up + 4);
        }
    }
    return path;
}

static void WalkNavPoints(XMLElement *parent, const String &tocDir, std::vector<EpubBook::Chapter> &chapters) {
    for (XMLElement *navPoint = parent->FirstChildElement("navPoint"); navPoint != NULL; navPoint = navPoint->NextSiblingElement("navPoint")) {
        XMLElement *navLabel = navPoint->FirstChildElement("navLabel");
        XMLElement *content = navPoint->FirstChildElement("content");
        if ((navLabel != NULL) && (content != NULL)) {
            XMLElement *text = navLabel->FirstChildElement("text");
            const char *src = content->Attribute("src");
            if ((text != NULL) && (text->GetText() != NULL) && (src != NULL)) {
                String href = ResolveRelative(tocDir, String(src));
                int hash = href.indexOf('#');
                String hrefNoFrag = (hash >= 0) ? href.substring(0, hash) : href;
                for (size_t i = 0; i < chapters.size(); i++) {
                    if ((chapters[i].title.length() == 0) && (chapters[i].href == hrefNoFrag)) {
                        chapters[i].title = String(text->GetText());
                        break;
                    }
                }
            }
        }
        WalkNavPoints(navPoint, tocDir, chapters);
    }
}

static size_t ZipReadCallback(void *pOpaque, mz_uint64 file_ofs, void *pBuf, size_t n) {
    File *f = (File*)pOpaque;
    if (!f->seek(file_ofs, SeekSet)) {
        return 0;
    }
    return f->read((uint8_t*)pBuf, n);
}

EpubBook::EpubBook() : _zip(NULL) {
}

EpubBook::~EpubBook() {
    Close();
}

void EpubBook::Close() {
    if (_zip != NULL) {
        mz_zip_reader_end((mz_zip_archive*)_zip);
        free(_zip);
        _zip = NULL;
    }
    if (_file) {
        _file.close();
    }
    _chapters.clear();
    _opfDir = "";
}

bool EpubBook::ReadEntryToString(const String &archivePath, String &out) {
    mz_zip_archive *zip = (mz_zip_archive*)_zip;
    mz_uint32 index;
    if (!mz_zip_reader_locate_file_v2(zip, archivePath.c_str(), NULL, 0, &index)) {
        log_e("EPUB: entry not found: %s", archivePath.c_str());
        return false;
    }
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(zip, index, &stat)) {
        return false;
    }
    size_t size = (size_t)stat.m_uncomp_size;
    char *buf = (char*)ps_malloc(size + 1);
    if (buf == NULL) {
        buf = (char*)malloc(size + 1);
    }
    if (buf == NULL) {
        log_e("EPUB: out of memory extracting %s (%u bytes)", archivePath.c_str(), (unsigned)size);
        return false;
    }
    if (!mz_zip_reader_extract_to_mem(zip, index, buf, size, 0)) {
        free(buf);
        return false;
    }
    buf[size] = '\0';
    out = String(buf);
    free(buf);
    return true;
}

bool EpubBook::ParseContainer(String &opfPath) {
    String xml;
    if (!ReadEntryToString("META-INF/container.xml", xml)) {
        return false;
    }
    XMLDocument doc;
    if (doc.Parse(xml.c_str()) != XML_SUCCESS) {
        return false;
    }
    XMLElement *root = doc.RootElement();
    if (root == NULL) {
        return false;
    }
    XMLElement *rootfiles = root->FirstChildElement("rootfiles");
    if (rootfiles == NULL) {
        return false;
    }
    XMLElement *rootfile = rootfiles->FirstChildElement("rootfile");
    if (rootfile == NULL) {
        return false;
    }
    const char *path = rootfile->Attribute("full-path");
    if (path == NULL) {
        return false;
    }
    opfPath = String(path);
    return true;
}

bool EpubBook::ParseOpf(const String &opfPath) {
    String xml;
    if (!ReadEntryToString(opfPath, xml)) {
        return false;
    }
    _opfDir = DirName(opfPath);

    XMLDocument doc;
    if (doc.Parse(xml.c_str()) != XML_SUCCESS) {
        return false;
    }
    XMLElement *root = doc.RootElement();
    if (root == NULL) {
        return false;
    }
    XMLElement *manifest = root->FirstChildElement("manifest");
    XMLElement *spine = root->FirstChildElement("spine");
    if ((manifest == NULL) || (spine == NULL)) {
        return false;
    }

    std::vector<std::pair<String, String>> idToHref;
    String tocHref;
    for (XMLElement *item = manifest->FirstChildElement("item"); item != NULL; item = item->NextSiblingElement("item")) {
        const char *id = item->Attribute("id");
        const char *href = item->Attribute("href");
        if ((id == NULL) || (href == NULL)) {
            continue;
        }
        idToHref.push_back(std::make_pair(String(id), String(href)));

        const char *mediaType = item->Attribute("media-type");
        if ((mediaType != NULL) && (String(mediaType) == "application/x-dtbncx+xml")) {
            tocHref = String(href);
        }
    }

    for (XMLElement *itemref = spine->FirstChildElement("itemref"); itemref != NULL; itemref = itemref->NextSiblingElement("itemref")) {
        const char *idref = itemref->Attribute("idref");
        if (idref == NULL) {
            continue;
        }
        String idrefStr(idref);
        const String *href = NULL;
        for (size_t i = 0; i < idToHref.size(); i++) {
            if (idToHref[i].first == idrefStr) {
                href = &idToHref[i].second;
                break;
            }
        }
        if (href == NULL) {
            continue;
        }
        Chapter ch;
        ch.href = ResolveRelative(_opfDir, *href);
        ch.title = "";
        _chapters.push_back(ch);
    }

    if (tocHref.length() > 0) {
        ParseToc(ResolveRelative(_opfDir, tocHref));
    }

    return _chapters.size() > 0;
}

void EpubBook::ParseToc(const String &tocPath) {
    String xml;
    if (!ReadEntryToString(tocPath, xml)) {
        return;
    }
    XMLDocument doc;
    if (doc.Parse(xml.c_str()) != XML_SUCCESS) {
        return;
    }
    XMLElement *root = doc.RootElement();
    if (root == NULL) {
        return;
    }
    XMLElement *navMap = root->FirstChildElement("navMap");
    if (navMap == NULL) {
        return;
    }
    WalkNavPoints(navMap, DirName(tocPath), _chapters);
}

bool EpubBook::Open(const String &path) {
    Close();

    _file = SD.open(path, FILE_READ);
    if (!_file) {
        return false;
    }

    mz_zip_archive *zip = (mz_zip_archive*)calloc(1, sizeof(mz_zip_archive));
    if (zip == NULL) {
        _file.close();
        return false;
    }
    zip->m_pRead = ZipReadCallback;
    zip->m_pIO_opaque = &_file;
    if (!mz_zip_reader_init(zip, _file.size(), 0)) {
        log_e("EPUB: mz_zip_reader_init failed: %s", mz_zip_get_error_string(zip->m_last_error));
        free(zip);
        _file.close();
        return false;
    }
    _zip = zip;

    String opfPath;
    if (!ParseContainer(opfPath)) {
        log_e("EPUB: could not parse META-INF/container.xml");
        Close();
        return false;
    }
    if (!ParseOpf(opfPath)) {
        log_e("EPUB: could not parse OPF manifest/spine: %s", opfPath.c_str());
        Close();
        return false;
    }
    return true;
}

String EpubBook::ChapterTitle(int index) const {
    if ((index < 0) || (index >= (int)_chapters.size())) {
        return "";
    }
    if (_chapters[index].title.length() > 0) {
        return _chapters[index].title;
    }
    String href = _chapters[index].href;
    int slash = href.lastIndexOf('/');
    String name = (slash >= 0) ? href.substring(slash + 1) : href;
    int dot = name.lastIndexOf('.');
    if (dot > 0) {
        name = name.substring(0, dot);
    }
    return name;
}

String EpubBook::GetChapterText(int index) {
    if ((index < 0) || (index >= (int)_chapters.size())) {
        return "";
    }
    String html;
    if (!ReadEntryToString(_chapters[index].href, html)) {
        return "";
    }
    return HtmlToPlainText(html);
}
