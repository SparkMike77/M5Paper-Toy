#include "html_to_text.h"

static void AppendEntity(String &out, const String &entity) {
    if (entity == "amp")  { out += '&'; return; }
    if (entity == "lt")   { out += '<'; return; }
    if (entity == "gt")   { out += '>'; return; }
    if (entity == "quot") { out += '"'; return; }
    if ((entity == "apos") || (entity == "#39") || (entity == "#x27")) { out += '\''; return; }
    if (entity == "nbsp")  { out += ' '; return; }
    if ((entity == "mdash") || (entity == "ndash")) { out += '-'; return; }
    if ((entity == "ldquo") || (entity == "rdquo")) { out += '"'; return; }
    if ((entity == "lsquo") || (entity == "rsquo")) { out += '\''; return; }
    if ((entity.length() > 1) && (entity.charAt(0) == '#')) {
        long code;
        if ((entity.length() > 2) && ((entity.charAt(1) == 'x') || (entity.charAt(1) == 'X'))) {
            code = strtol(entity.c_str() + 2, NULL, 16);
        } else {
            code = strtol(entity.c_str() + 1, NULL, 10);
        }
        if ((code > 0) && (code < 128)) {
            out += (char)code;
        } else if (code >= 128) {
            out += '?'; // no non-ASCII font support in this reader yet
        }
        return;
    }
    // Unknown entity - drop it rather than let it corrupt the output.
}

static bool IsBlockCloseTag(const String &tag) {
    return (tag == "/p") || (tag == "/div") || (tag == "/li") || (tag == "/blockquote") ||
           (tag == "/tr") || (tag == "/h1") || (tag == "/h2") || (tag == "/h3") ||
           (tag == "/h4") || (tag == "/h5") || (tag == "/h6");
}

// Looks up `attrName="..."` (or '...') within a raw tag's attribute string.
// Matches the attribute name case-insensitively but returns the value with
// its original casing intact, since URLs are case-sensitive.
static bool ExtractAttr(const String &tag, const char *attrName, String &out) {
    String tagLower = tag;
    tagLower.toLowerCase();
    String needle = String(attrName) + "=";
    int idx = tagLower.indexOf(needle);
    if (idx < 0) {
        return false;
    }
    int valStart = idx + needle.length();
    if ((valStart >= (int)tag.length()) || ((tag.charAt(valStart) != '"') && (tag.charAt(valStart) != '\''))) {
        return false;
    }
    char quote = tag.charAt(valStart);
    valStart++;
    int valEnd = tag.indexOf(quote, valStart);
    if (valEnd < 0) {
        return false;
    }
    out = tag.substring(valStart, valEnd);
    return true;
}

String HtmlToPlainText(const String &html, std::vector<HtmlImageMarker> *images) {
    String out;
    out.reserve(html.length() / 2 + 64);

    int i = 0;
    int n = (int)html.length();
    bool lastWasNewline = true;
    bool lastWasSpace = true;

    while (i < n) {
        char c = html.charAt(i);

        if (c == '<') {
            int close = html.indexOf('>', i + 1);
            if (close < 0) {
                break; // malformed tail - stop rather than misparse it
            }
            String tag = html.substring(i + 1, close);
            tag.trim();
            int sp = tag.indexOf(' ');
            String tagName = (sp >= 0) ? tag.substring(0, sp) : tag;
            tagName.toLowerCase();
            if (tagName.endsWith("/")) {
                tagName = tagName.substring(0, tagName.length() - 1);
            }

            if ((tagName == "script") || (tagName == "style")) {
                String closeTag = "</" + tagName;
                int end = html.indexOf(closeTag, close + 1);
                int afterClose = (end >= 0) ? html.indexOf('>', end) : -1;
                i = (afterClose >= 0) ? (afterClose + 1) : n;
                continue;
            }

            if (tagName == "img") {
                if (images != NULL) {
                    String src;
                    if (ExtractAttr(tag, "src", src)) {
                        HtmlImageMarker marker;
                        marker.pos = (uint32_t)out.length();
                        marker.href = src;
                        marker.href.replace("&amp;", "&");
                        images->push_back(marker);
                    }
                }
                i = close + 1;
                continue;
            }

            if (tagName == "br") {
                if (!lastWasNewline) {
                    out += '\n';
                    lastWasNewline = true;
                    lastWasSpace = true;
                }
             } else if (IsBlockCloseTag(tagName)) {
                if (!lastWasNewline) {
                    out += "\n\n";
                    lastWasNewline = true;
                    lastWasSpace = true;
                }
            }
            i = close + 1;
            continue;
        }

        if (c == '&') {
            int semi = html.indexOf(';', i + 1);
            if ((semi >= 0) && (semi - i <= 12)) {
                String entity = html.substring(i + 1, semi);
                int before = out.length();
                AppendEntity(out, entity);
                if ((int)out.length() > before) {
                    lastWasNewline = false;
                    lastWasSpace = (out.charAt(out.length() - 1) == ' ');
                }
                i = semi + 1;
                continue;
            }
        }

        if ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n')) {
            if (!lastWasSpace && !lastWasNewline) {
                out += ' ';
                lastWasSpace = true;
            }
            i++;
            continue;
        }

        out += c;
        lastWasSpace = false;
        lastWasNewline = false;
        i++;
    }

    return out;
}
