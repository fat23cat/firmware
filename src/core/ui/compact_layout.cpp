#include "compact.h"

#ifdef UI_COMPACT

int uiListWindowFirst(int index, int n, int visible, int prevFirst, bool reset) {
    if (n <= 0 || visible <= 0) return 0;
    if (visible > n) visible = n;
    const int maxFirst = n - visible;
    int first = reset ? 0 : prevFirst;
    if (first < 0) first = 0;
    if (first > maxFirst) first = maxFirst;
    if (index < 0) index = 0;
    if (index >= n) index = n - 1;
    if (index < first) first = index;
    if (index >= first + visible) first = index - visible + 1;
    return first;
}

int uiWheelSlotCount(int n) { return n < 0 ? 0 : (n < 5 ? n : 5); }

int uiWheelOffset(int slot) {
    static const int offsets[5] = {0, 1, -1, 2, -2};
    return (slot >= 0 && slot < 5) ? offsets[slot] : 0;
}

int uiWheelItem(int index, int slot, int n) {
    if (n <= 0) return 0;
    return ((index + uiWheelOffset(slot)) % n + n) % n;
}

void uiProgressValues(int progress, size_t total, int barMaxW, int &pct, int &barW) {
    uint64_t p = progress < 0 ? 0 : (uint64_t)progress;
    if (total && p > total) p = total;
    pct = total ? (int)(p * 100 / total) : 100;
    barW = total ? (int)(p * (uint64_t)barMaxW / total) : barMaxW;
}

UiTextBlock uiTextBlockLayout(const String &text, int16_t maxW, int16_t maxH, uint8_t maxFmLines, uint8_t maxFpLines) {
    UiTextBlock b;
    const int16_t fmPitch = LH * FM, fpPitch = LH * FP + 2;
    b.lines = uiWrap(text, maxW, FM);
    int fmFit = fmPitch > 0 ? maxH / fmPitch : 0;
    if (fmFit > maxFmLines) fmFit = maxFmLines;
    if (!b.lines.empty() && (int)b.lines.size() <= fmFit) {
        b.size = FM;
        b.pitch = fmPitch;
        return b;
    }
    int fpFit = fpPitch > 0 ? maxH / fpPitch : 0;
    if (fpFit > maxFpLines) fpFit = maxFpLines;
    if (fpFit < 1) fpFit = 1;
    b.size = FP;
    b.pitch = fpPitch;
    b.lines = uiWrap(text, maxW, FP, fpFit);
    if (b.lines.empty()) b.lines.push_back("");
    return b;
}

UiInputLines uiInputLines(const String &text, bool mask, int16_t innerW, int maxLines) {
    UiInputLines r;
    String shown;
    if (mask) {
        for (unsigned i = 0; i < text.length(); i++) shown += '*';
    } else {
        shown = text;
    }
    shown += "_"; // caret
    const int16_t fmChars = innerW / (LW * FM);
    if ((int16_t)shown.length() <= fmChars) {
        r.size = FM;
        r.lines.push_back(shown);
        return r;
    }
    r.size = FP;
    const int16_t fpChars = innerW / (LW * FP);
    if (fpChars <= 0) return r;
    const int total = (shown.length() + fpChars - 1) / fpChars;
    const int first = (maxLines > 0 && total > maxLines) ? total - maxLines : 0;
    for (int l = first; l < total; l++) r.lines.push_back(shown.substring(l * fpChars, (l + 1) * fpChars));
    return r;
}

UiKeyResult uiApplyKeyStroke(const keyStroke &k, String &text, int maxSize) {
    UiKeyResult res = UI_KEY_NONE;
    String keyStr = "";
    for (auto c : k.word) {
        if (keyStr != "") keyStr = keyStr + "+" + c;
        else keyStr += c;
    }
    if ((int)text.length() < maxSize && !k.enter && !k.del) {
        text += keyStr;
        res = UI_KEY_CHANGED;
    }
    if (k.del && text.length() > 0) {
        text.remove(text.length() - 1);
        res = UI_KEY_CHANGED;
    }
    if (k.enter) res = UI_KEY_ENTER;
    return res;
}

uint16_t uiBlend565(uint16_t fg, uint16_t bg, uint16_t t) {
    if (t > 256) t = 256;
    const uint16_t u = 256 - t;
    uint16_t r = (((fg >> 11) & 0x1F) * t + ((bg >> 11) & 0x1F) * u) >> 8;
    uint16_t g = (((fg >> 5) & 0x3F) * t + ((bg >> 5) & 0x3F) * u) >> 8;
    uint16_t b = ((fg & 0x1F) * t + (bg & 0x1F) * u) >> 8;
    return (r << 11) | (g << 5) | b;
}

#endif // UI_COMPACT
