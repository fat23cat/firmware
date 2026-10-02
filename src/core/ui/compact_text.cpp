#include "compact.h"

#ifdef UI_COMPACT

static int16_t maxChars(int16_t maxW, uint8_t size) {
    if (size == 0 || maxW <= 0) return 0;
    return maxW / (LW * size);
}

int32_t uiTextW(const String &s, uint8_t size) { return (int32_t)s.length() * LW * size; }

uint8_t uiFitSize(const String &s, int16_t maxW, uint8_t pref, uint8_t min) {
    for (uint8_t size = pref; size > min; size--) {
        if (uiTextW(s, size) <= maxW) return size;
    }
    return min;
}

String uiTruncate(const String &s, int16_t maxW, uint8_t size) {
    int16_t n = maxChars(maxW, size);
    if ((int16_t)s.length() <= n) return s;
    if (n <= 2) return s.substring(0, n > 0 ? n : 0);
    return s.substring(0, n - 2) + "..";
}

String uiTruncateMiddle(const String &s, int16_t maxW, uint8_t size) {
    int16_t n = maxChars(maxW, size);
    int16_t len = s.length();
    if (len <= n) return s;
    if (n <= 4) return uiTruncate(s, maxW, size);

    int16_t avail = n - 2; // room left after ".."
    int dot = s.lastIndexOf('.');
    int16_t extLen = (dot > 0 && len - dot <= 5) ? len - dot : 0;
    // keep the extension plus a couple of characters before it, but never more than half
    int16_t tail = extLen ? min<int16_t>(extLen + 2, avail / 2) : avail / 2;
    int16_t head = avail - tail;
    return s.substring(0, head) + ".." + s.substring(len - tail);
}

std::vector<String> uiWrap(const String &s, int16_t maxW, uint8_t size, uint8_t maxLines) {
    std::vector<String> lines;
    int16_t n = maxChars(maxW, size);
    if (n <= 0) return lines;

    bool dropped = false;
    int start = 0;
    const int len = s.length();
    while (start <= len) {
        int nl = s.indexOf('\n', start);
        String para = s.substring(start, nl < 0 ? len : nl);
        start = (nl < 0) ? len + 1 : nl + 1;

        // Greedy wrap of one paragraph. Break opportunities match wrapText() in display.cpp: at a space
        // (dropped) or after '-' / '_' (kept on the first line; wrapText drops those, losing characters).
        // Words longer than a line are hard-split.
        do {
            if (maxLines && lines.size() >= maxLines) {
                dropped = true;
                break;
            }
            if ((int16_t)para.length() <= n) {
                lines.push_back(para);
                para = "";
                break;
            }
            int cut = -1, skip = 0; // line = [0, cut), rest starts at cut + skip
            for (int i = n; i > 0; i--) {
                if (para[i] == ' ') {
                    cut = i;
                    skip = 1;
                    break;
                }
                if (para[i - 1] == '-' || para[i - 1] == '_') {
                    cut = i;
                    break;
                }
            }
            if (cut <= 0) {
                lines.push_back(para.substring(0, n));
                para = para.substring(n);
            } else {
                lines.push_back(para.substring(0, cut));
                para = para.substring(cut + skip);
            }
        } while (para.length() > 0);
        if (dropped) break;
    }

    if (dropped && !lines.empty()) {
        String &last = lines.back();
        if ((int16_t)last.length() + 2 > n) last = last.substring(0, n > 2 ? n - 2 : 0);
        last += "..";
    }
    return lines;
}

void uiDrawText(const String &s, int16_t x, int16_t y, uint8_t datum) {
    // TFT_eSPI datums: 0-8 = rows (top/middle/bottom) x columns (left/centre/right), 9-11 = baselines.
    // The GLCD font has no descender below its cell, so baselines are treated as bottom.
    uint8_t d = datum <= 8 ? datum : (datum <= 11 ? datum - 3 : 0);
    const int16_t h = LH * tft.getTextSize();
    const int16_t top = y - (d / 3 == 1 ? h / 2 : d / 3 == 2 ? h : 0);

    uint8_t oldDatum = tft.getTextDatum();
    tft.setTextDatum(TL_DATUM);
    switch (d % 3) {
        case 0: tft.drawString(s, x, top, 1); break;
        case 1: tft.drawCentreString(s, x, top, 1); break;
        default: tft.drawRightString(s, x, top, 1); break;
    }
    tft.setTextDatum(oldDatum);
}

uint8_t uiDrawFit(
    const String &s, int16_t x, int16_t y, int16_t maxW, uint8_t datum, uint8_t pref, uint16_t fg,
    uint16_t bg
) {
    uint8_t size = uiFitSize(s, maxW, pref, FP);
    tft.setTextSize(size);
    tft.setTextColor(fg, bg);
    uiDrawText(uiTruncate(s, maxW, size), x, y, datum);
    return size;
}

#endif // UI_COMPACT
