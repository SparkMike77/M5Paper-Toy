#ifndef _HTML_TO_TEXT_H_
#define _HTML_TO_TEXT_H_

#include <Arduino.h>

// Strips an XHTML chapter body down to plain text: drops all tags and
// <script>/<style> contents, decodes common HTML entities, and turns
// block-level closing tags (</p>, </div>, <br>, headings, etc.) into line
// breaks so paragraph structure survives for M5EPD_Canvas's plain-text
// pagination. Not a real HTML parser - EPUB chapters are XHTML in theory,
// but real-world files are inconsistent enough that a strict XML DOM parse
// of chapter bodies is more likely to reject valid-looking content than a
// permissive linear scan.
String HtmlToPlainText(const String &html);

#endif //_HTML_TO_TEXT_H_
