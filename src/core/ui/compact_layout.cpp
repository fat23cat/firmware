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

uint16_t uiBlend565(uint16_t fg, uint16_t bg, uint16_t t) {
    if (t > 256) t = 256;
    const uint16_t u = 256 - t;
    uint16_t r = (((fg >> 11) & 0x1F) * t + ((bg >> 11) & 0x1F) * u) >> 8;
    uint16_t g = (((fg >> 5) & 0x3F) * t + ((bg >> 5) & 0x3F) * u) >> 8;
    uint16_t b = ((fg & 0x1F) * t + (bg & 0x1F) * u) >> 8;
    return (r << 11) | (g << 5) | b;
}

#endif // UI_COMPACT
