#include "compact.h"

#ifdef UI_COMPACT
#include "core/display.h"
#include "core/utils.h"
#include "core/wifi/wg.h"
#include <WiFi.h>
#include <interface.h>

// ---------------------------------------------------------------------------------------------
// Status bar (0 .. SB_H), 240x135 landscape:
//   [time or "BRUCE vX"]      [SD][GPS][WiFi][Web][BLE][WG]      [ 87%][battery]
// ---------------------------------------------------------------------------------------------
namespace {
constexpr int16_t TEXT_Y = 5;              // FP text baseline row inside the bar
constexpr int16_t LEFT_W = 11 * LW;        // "HH:MM:SS PM" (longest timeStr) = 66 px
constexpr int16_t ICON = 16;               // status icons are 16x16
constexpr int16_t ICON_Y = 2;            // below the frame's top line (y = 1); icons end at y = 17
constexpr int16_t BAT_W = 24, BAT_H = 12;  // battery outline
constexpr int16_t BAT_Y = 3;
constexpr int16_t PCT_W = 4 * LW;          // "100%" / "CHG"
constexpr int16_t GAP = 4;
} // namespace

static void drawLeftSlot() {
    String text;
    if (clock_set) {
#if defined(HAS_RTC)
        updateTimeStr(_rtc.getTimeStruct());
#else
        updateTimeStr(rtc.getTimeStruct());
#endif
        text = timeStr;
    } else {
        text = "BRUCE " + String(BRUCE_VERSION);
    }
    tft.fillRect(cui::PAD, TEXT_Y, LEFT_W, LH, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    uiDrawText(uiTruncate(text, LEFT_W, FP), cui::PAD, TEXT_Y, TL_DATUM);
}

// Returns the x where the battery cluster starts (or the right padding edge if no battery).
static int16_t drawBattery() {
    const int16_t right = tftWidth - cui::PAD;
    uint8_t bat = getBattery();
    if (bat == 0) return right;
    if (bat > 100) bat = 100;

    const int16_t batX = right - BAT_W;
    const uint16_t color = bat < 16 ? TFT_RED : bruceConfig.priColor;

    tft.drawRoundRect(batX, BAT_Y, BAT_W, BAT_H, 2, color);
    tft.fillRect(batX + 2, BAT_Y + 2, BAT_W - 4, BAT_H - 4, bruceConfig.bgColor);
    tft.fillRect(batX + 2, BAT_Y + 2, (BAT_W - 4) * bat / 100, BAT_H - 4, color);

    const int16_t pctRight = batX - 3;
    tft.fillRect(pctRight - PCT_W, TEXT_Y, PCT_W, LH, bruceConfig.bgColor);
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.drawRightString(isCharging() ? String("CHG") : String(bat) + "%", pctRight, TEXT_Y, 1);
    return pctRight - PCT_W;
}

static void drawIcons(int16_t left, int16_t right) {
    typedef void (*IconFn)(int, int);
    IconFn icons[6];
    int n = 0;
    if (sdcardMounted) icons[n++] = drawSdSmall;
    if (gpsConnected) icons[n++] = drawGpsSmall;
    if (WiFi.getMode() != 0) icons[n++] = drawWifiSmall;
    if (isWebUIActive) icons[n++] = drawWebUISmall;
    if (BLEConnected) icons[n++] = drawBLESmall;
    if (isConnectedWireguard) icons[n++] = drawWireguardStatus;

    // Clear the whole icon zone so icons that went away don't linger.
    tft.fillRect(left, ICON_Y, right - left, ICON, bruceConfig.bgColor);
    if (n == 0) return;

    const int16_t avail = right - left;
    int16_t gap = GAP;
    while (gap > 0 && n * ICON + (n - 1) * gap > avail) gap--;
    int16_t total = n * ICON + (n - 1) * gap;
    if (total > avail) n = (avail + gap) / (ICON + gap); // not enough room: drop the last icons
    total = n * ICON + (n - 1) * gap;

    int16_t x = left + (avail - total) / 2;
    for (int i = 0; i < n; i++, x += ICON + gap) icons[i](x, ICON_Y);
}

void uiDrawStatusBar() {
    drawLeftSlot();
    int16_t batLeft = drawBattery();
    drawIcons(cui::PAD + LEFT_W + GAP, batLeft - GAP);

    // Frame and rule last, so no slot clear can erase them.
    const uint16_t pri = bruceConfig.priColor;
    if (bruceConfig.theme.border) {
        tft.drawRoundRect(1, 1, tftWidth - 2, tftHeight - 2, 4, pri);
        tft.drawFastHLine(1, cui::SB_H, tftWidth - 2, pri);
    } else {
        tft.drawFastHLine(0, cui::SB_H, tftWidth, pri);
    }
}

static int16_t s_titleBottom = cui::TOP; // where uiSubtitle() goes; reset by uiDrawMainBorder()

void uiDrawMainBorder(bool clear) {
    if (clear) {
        tft.drawPixel(0, 0, 0);
        tft.fillScreen(bruceConfig.bgColor);
        uiProgressReset(); // a new screen: the next progress bar starts clean
    }
    s_titleBottom = cui::TOP;
    tft.setTextDatum(0);
    uiDrawStatusBar();
    // Leave the cursor at the first content row, so callers that print right away start below the bar.
    setTftDisplay(cui::PAD, cui::TOP, bruceConfig.priColor, FP, bruceConfig.bgColor);

#if defined(HAS_TOUCH)
    TouchFooter();
#endif
}

// ---------------------------------------------------------------------------------------------
// Shared list geometry: rows from x = LIST_X to the scrollbar, content from TOP to the frame.
// ---------------------------------------------------------------------------------------------
namespace {
constexpr int16_t LIST_X = 3;   // inside the inset-1 frame
constexpr int16_t TEXT_INSET = 3; // text starts at LIST_X + TEXT_INSET = PAD

int16_t scrollbarX() { return tftWidth - 3 - cui::SCROLLBAR_W; }
int16_t listRight() { return scrollbarX() - 2; } // last x of a row
int16_t listW() { return listRight() - LIST_X + 1; }
int16_t textMaxW() { return listW() - 2 * TEXT_INSET; }
int16_t contentBottom() { return tftHeight - 3; } // last usable y (frame at h - 2)

void drawScrollbar(int first, int visible, int total) {
    const int16_t x = scrollbarX();
    const int16_t top = cui::TOP;
    const int16_t h = contentBottom() - top + 1;
    tft.fillRect(x, top, cui::SCROLLBAR_W, h, bruceConfig.bgColor);
    if (total <= visible || total <= 0) return;
    int16_t thumbH = max<int16_t>(6, h * visible / total);
    int16_t thumbY = top + (int32_t)(h - thumbH) * first / (total - visible);
    tft.fillRect(x, thumbY, cui::SCROLLBAR_W, thumbH, bruceConfig.priColor);
}
} // namespace

static bool s_marqueeReset = false;

bool uiMarqueeConsumeReset() {
    bool r = s_marqueeReset;
    s_marqueeReset = false;
    return r;
}

Opt_Coord uiDrawOptions(
    int index, std::vector<Option> &options, uint16_t fgcolor, uint16_t selcolor, uint16_t bgcolor,
    bool firstRender
) {
    Opt_Coord coord;
    const int n = options.size();
    if (n == 0) return coord;

    // One size for the whole list, decided on every call (the global `options` vector is reused).
    uint8_t size = FM;
    for (const Option &opt : options) {
        if (uiTextW(opt.label, FM) > textMaxW()) {
            size = FP;
            break;
        }
    }
    const int16_t pitch = size == FM ? cui::ROW_FM : cui::ROW_FP;
    const int rows = max(1, (contentBottom() - cui::TOP + 1) / pitch);
    const int visible = min(rows, n);
    const int16_t maxChars = textMaxW() / (LW * size);

    // Scroll window: keep the selection visible, move only when it leaves the window.
    static int first = 0;
    first = uiListWindowFirst(index, n, visible, first, firstRender);

    tft.setTextSize(size);
    for (int r = 0; r < visible; r++) {
        const int i = first + r;
        const Option &opt = options[i];
        const bool sel = (i == index);
        const int16_t y = cui::TOP + r * pitch;
        const int16_t textY = y + (pitch - 1 - LH * size) / 2;

        tft.fillRect(LIST_X, y, listW(), pitch, bgcolor);
        if (sel) tft.fillRoundRect(LIST_X, y, listW(), pitch - 1, 3, fgcolor);

        uint16_t color = fgcolor;
        if (opt.selected) color = selcolor;
        if (!opt.enabled) color = TFT_DARKGREY;
        if (sel) tft.setTextColor(bgcolor, fgcolor);
        else tft.setTextColor(color, bgcolor);

        const int16_t textX = LIST_X + TEXT_INSET;
        if (sel) {
            // Too long: show the start; loopOptions scrolls it with displayScrollingText().
            uiDrawText(opt.label.substring(0, maxChars), textX, textY, TL_DATUM);
            coord.x = textX;
            coord.y = textY;
            coord.size = maxChars + 1; // marquee shows size - 1 chars and scrolls when len >= size
            coord.fgcolor = fgcolor;
            coord.bgcolor = bgcolor;
            s_marqueeReset = true; // the row now shows the start of the label
        } else {
            uiDrawText(uiTruncate(opt.label, textMaxW(), size), textX, textY, TL_DATUM);
        }
    }
    // Clear what's left below the last row (the list may have shrunk or changed size).
    const int16_t usedBottom = cui::TOP + visible * pitch;
    if (usedBottom <= contentBottom())
        tft.fillRect(LIST_X, usedBottom, listW(), contentBottom() - usedBottom + 1, bgcolor);
    drawScrollbar(first, visible, n);

    tft.setTextColor(fgcolor, bgcolor);
    tft.setTextSize(size); // the marquee uses the current text size
    return coord;
}

void uiDrawSubmenu(int index, std::vector<Option> &options, const char *title) {
    uiDrawStatusBar();
    const int n = options.size();
    if (n == 0) return;
    const uint16_t bg = bruceConfig.bgColor;
    const int16_t rowX = LIST_X;
    const int16_t centerX = LIST_X + listW() / 2;

    // Title
    tft.fillRect(rowX, cui::TOP, listW(), LH, bg);
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bg);
    uiDrawText(uiTruncate(String(title), textMaxW(), FP), rowX + TEXT_INSET, cui::TOP, TL_DATUM);

    // Five slots, selected in the middle of the area below the title (offsets 0, +1, -1, +2, -2).
    const int16_t areaTop = cui::TOP + LH + 2;
    const int16_t mid = (areaTop + contentBottom()) / 2; // vertical centre of the selected item
    const int16_t selH = LH * FM, smallH = LH * FP;
    const int16_t y0 = mid - selH / 2;
    const int16_t y1 = y0 + selH + 4, y2 = y1 + smallH + 8;      // below
    const int16_t ym1 = y0 - 4 - smallH, ym2 = ym1 - 8 - smallH; // above
    const int16_t ys[5] = {y0, y1, ym1, y2, ym2};
    const int16_t hs[5] = {(int16_t)(selH + 2), smallH, smallH, smallH, smallH}; // selected: text + underline

    // Clear only each slot's strip (not the whole area) to avoid flicker; unused slots are cleared too.
    for (int k = 0; k < 5; k++) tft.fillRect(rowX, ys[k], listW(), hs[k], bg);

    const uint16_t dim = uiBlend565(bruceConfig.secColor, bg, 128); // always between secColor and bg
    const int slots = uiWheelSlotCount(n);
    for (int k = 0; k < slots; k++) {
        const Option &opt = options[uiWheelItem(index, k, n)];
        const int off = uiWheelOffset(k);
        const int d = off < 0 ? -off : off;
        uint16_t color;
        if (!opt.enabled) color = TFT_DARKGREY;
        else if (d == 0) color = bruceConfig.priColor;
        else if (d == 1) color = bruceConfig.secColor;
        else color = dim;

        uint8_t size = d == 0 ? uiFitSize(opt.label, textMaxW(), FM, FP) : FP;
        // centre the smaller FP text inside the selected row's height
        int16_t y = ys[k] + (d == 0 ? (selH - LH * size) / 2 : 0);
        String text = uiTruncate(opt.label, textMaxW(), size);
        tft.setTextSize(size);
        tft.setTextColor(color, bg);
        uiDrawText(text, centerX, y, TC_DATUM); // drawCentreString: centred in the WebUI mirror too
        if (d == 0) {
            int16_t w = uiTextW(text, size);
            tft.drawFastHLine(centerX - w / 2, ys[k] + selH + 1, w, color);
        }
    }
    drawScrollbar(n > 1 ? index : 0, 1, n);
}

void uiMenuTitle(const String &name, int16_t centerX, int16_t y) {
    tft.fillRect(cui::PAD, y, tftWidth - 2 * cui::PAD, LH * FM, bruceConfig.bgColor);
    // Middle datum at the row's centre: an FP fallback is centred vertically in the FM row too.
    uiDrawFit(
        name, centerX, y + LH * FM / 2, tftWidth - 2 * cui::PAD, MC_DATUM, FM, bruceConfig.priColor,
        bruceConfig.bgColor
    );
}

// ---------------------------------------------------------------------------------------------
// Titles, message boxes, dialogs, progress, footnotes
// ---------------------------------------------------------------------------------------------
namespace {
int16_t contentW() { return tftWidth - 2 * cui::PAD; }
} // namespace

void uiTitle(const String &title) {
    String t = title;
    t.toUpperCase();
    const uint16_t bg = bruceConfig.bgColor;
    tft.fillRect(cui::PAD, cui::TOP, contentW(), LH * FM, bg);
    uint8_t size = uiDrawFit(t, tftWidth / 2, cui::TOP, contentW(), TC_DATUM, FM, bruceConfig.priColor, bg);
    s_titleBottom = cui::TOP + LH * size;
    tft.setCursor(0, s_titleBottom + 2);
    tft.setTextSize(FP);
}

void uiSubtitle(const String &subtitle, bool withLine) {
    // Right under the title if nothing was printed since uiTitle() left the cursor there; otherwise (stale
    // title from another screen, or no title) assume an FM title, which matches the common layout.
    const bool afterTitle = tft.getCursorY() == s_titleBottom + 2;
    const int16_t y = (afterTitle ? s_titleBottom : cui::TOP + LH * FM) + 2;
    tft.setTextSize(FP);
    tft.setTextColor(bruceConfig.priColor, bruceConfig.bgColor);
    tft.fillRect(cui::PAD, y, contentW(), LH, bruceConfig.bgColor);
    uiDrawText(uiTruncate(subtitle, contentW(), FP), tftWidth / 2, y, TC_DATUM);
    int16_t next = y + LH + 1;
    if (withLine) {
        tft.drawFastHLine(cui::PAD, next, contentW(), bruceConfig.priColor);
        next += 3;
    }
    tft.setCursor(0, next);
}

void uiStripe(const String &text, uint16_t fgcolor, uint16_t bgcolor) {
    if (fgcolor == bgcolor && fgcolor == TFT_WHITE) fgcolor = TFT_BLACK; // as legacy

    const int16_t boxX = cui::PAD, boxW = contentW();
    const int16_t textW = boxW - 12;
    const int16_t areaTop = cui::TOP, areaBottom = tftHeight - 3;

    // FM for up to 3 lines (a toast, not a page), else FP up to 6 lines.
    const UiTextBlock block = uiTextBlockLayout(text, textW, areaBottom - areaTop + 1 - 10, 3, 6);
    const uint8_t size = block.size;
    const std::vector<String> &lines = block.lines;
    const int16_t pitch = block.pitch;
    const int16_t boxH = lines.size() * pitch + 10;
    const int16_t boxY = (areaTop + areaBottom + 1) / 2 - boxH / 2;

    tft.drawPixel(0, 0, 0);
    tft.fillRoundRect(boxX, boxY, boxW, boxH, 7, bgcolor);
    tft.setTextSize(size);
    tft.setTextColor(fgcolor, bgcolor);
    int16_t y = boxY + 5 + (size == FP ? 1 : 0);
    for (const String &line : lines) {
        uiDrawText(line, tftWidth / 2, y, TC_DATUM);
        y += pitch;
    }
}

namespace {
void drawDialogButton(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, const char *text, bool inverted) {
    const uint16_t bg = bruceConfig.bgColor;
    if (inverted) {
        tft.fillRoundRect(x, y, w, h, 4, color);
    } else {
        tft.fillRoundRect(x, y, w, h, 4, bg);
        tft.drawRoundRect(x, y, w, h, 4, color);
    }
    tft.setTextSize(FP);
    tft.setTextColor(inverted ? bg : color, inverted ? color : bg);
    uiDrawText(uiTruncate(String(text), w - 4, FP), x + w / 2, y + (h - LH) / 2, TC_DATUM);
}
} // namespace

int8_t uiMessage(
    const char *message, const char *leftButton, const char *centerButton, const char *rightButton,
    uint16_t color
) {
    const char *labels[3];
    int8_t total = 0;
    if (leftButton) labels[total++] = leftButton;
    if (centerButton) labels[total++] = centerButton;
    if (rightButton) labels[total++] = rightButton;

    const int16_t btnH = 16;
    const int16_t btnY = tftHeight - 3 - btnH + 1;
    const int16_t areaTop = cui::TOP, areaBottom = (total ? btnY - 4 : tftHeight - 3);
    const int16_t areaH = areaBottom - areaTop + 1;
    const int16_t textW = contentW();

    // Message: FM if every wrapped line fits the area, else FP (truncated with ".." if still too long).
    const UiTextBlock block = uiTextBlockLayout(String(message), textW, areaH, 255, 255);
    const std::vector<String> &lines = block.lines;
    const int16_t pitch = block.pitch;
    int16_t y = areaTop + (areaH - (int16_t)lines.size() * pitch) / 2;
    tft.setTextSize(block.size);
    tft.setTextColor(color, bruceConfig.bgColor);
    for (const String &line : lines) {
        uiDrawText(line, tftWidth / 2, y, TC_DATUM);
        y += pitch;
    }

    const int16_t gap = 4;
    const int16_t btnW = total ? (contentW() - (total - 1) * gap) / total : 0;
    int8_t selected = 0;
    bool redraw = true;
    while (true) {
        if (total > 0 && (check(PrevPress) || check(EscPress))) {
            selected = (selected - 1 + total) % total;
            redraw = true;
        }
        if (total > 0 && check(NextPress)) {
            selected = (selected + 1) % total;
            redraw = true;
        }
        if (check(SelPress)) break;

        if (redraw) {
            for (int8_t i = 0; i < total; i++) {
                drawDialogButton(cui::PAD + i * (btnW + gap), btnY, btnW, btnH, color, labels[i], selected == i);
            }
            redraw = false;
        }
        delay(10);
    }
    return selected;
}

namespace {
struct ProgressState {
    bool active = false;
    String msg;
    size_t total = 0;
    int pct = -1;
    int barW = 0;
} s_progress;
} // namespace

void uiProgressReset() { s_progress.active = false; }

void uiProgress(int progress, size_t total, const String &message) {
    const uint16_t pri = bruceConfig.priColor, bg = bruceConfig.bgColor;
    const int16_t barX = cui::PAD, barY = tftHeight - 22, barH = 10, barOuterW = contentW();
    const int barMaxW = barOuterW - 4;
    const int16_t textY = tftHeight - 34;
    const int16_t pctW = 4 * LW; // "100%"

    int pct, barW; // overflow-safe: byte counts can exceed 21 MB, total can be 0
    uiProgressValues(progress, total, barMaxW, pct, barW);

    const bool start = !s_progress.active || message != s_progress.msg || total != s_progress.total ||
                       pct < s_progress.pct;
    if (start) {
        tft.fillRect(cui::PAD - 3, cui::TOP, contentW() + 6, tftHeight - 3 - cui::TOP + 1, bg);
        tft.setTextSize(FP);
        tft.setTextColor(pri, bg);
        uiDrawText(uiTruncate(message, contentW() - pctW - 4, FP), barX, textY, TL_DATUM);
        tft.drawRect(barX, barY, barOuterW, barH, pri);
        s_progress.active = true;
        s_progress.msg = message;
        s_progress.total = total;
        s_progress.pct = -1;
        s_progress.barW = 0;
    }
    if (barW > s_progress.barW) {
        tft.fillRect(barX + 2 + s_progress.barW, barY + 2, barW - s_progress.barW, barH - 4, pri);
        s_progress.barW = barW;
    }
    if (pct != s_progress.pct) {
        tft.setTextSize(FP);
        tft.setTextColor(pri, bg);
        tft.fillRect(barX + barOuterW - pctW, textY, pctW, LH, bg);
        uiDrawText(String(pct) + "%", barX + barOuterW, textY, TR_DATUM);
        s_progress.pct = pct;
    }
}

void uiFootnote(const String &text, bool centred) {
    const int16_t y = tftHeight - 3 - LH + 1;
    tft.setTextSize(FP);
    tft.fillRect(cui::PAD, y, contentW(), LH, bruceConfig.bgColor); // a shorter text must not leave old chars
    if (centred) {
        uiDrawText(uiTruncate(text, contentW(), FP), tftWidth / 2, y, TC_DATUM);
    } else {
        uiDrawText(uiTruncate(text, contentW(), FP), tftWidth - cui::PAD, y, TR_DATUM);
    }
}

#endif // UI_COMPACT
