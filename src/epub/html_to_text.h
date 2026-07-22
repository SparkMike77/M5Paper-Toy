#ifndef _HTML_TO_TEXT_H_
#define _HTML_TO_TEXT_H_

#include <Arduino.h>
#include <vector>

// An <img> tag found while scanning: `pos` is the character offset into the
// returned plain text at which the tag occurred (images consume no text of
// their own), and `href` is its raw, HTML-entity-decoded `src` attribute -
// still relative to the chapter file, not yet resolved against the EPUB's
// directory layout.
struct HtmlImageMarker {
    uint32_t pos;
    String href;
};

// Strips an XHTML chapter body down to plain text: drops all tags and
// <script>/<style> contents, decodes common HTML entities, and turns
// block-level closing tags (</p>, </div>, <br>, headings, etc.) into line
// breaks so paragraph structure survives for M5EPD_Canvas's plain-text
// pagination. Not a real HTML parser - EPUB chapters are XHTML in theory,
// but real-world files are inconsistent enough that a strict XML DOM parse
// of chapter bodies is more likely to reject valid-looking content than a
// permissive linear scan.
//
// If `images` is non-NULL, every <img> tag encountered is recorded into it
// in document order.
String HtmlToPlainText(const String &html, std::vector<HtmlImageMarker> *images = NULL);

#endif //_HTML_TO_TEXT_H_
