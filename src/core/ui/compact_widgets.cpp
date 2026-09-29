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

void uiDrawMainBorder(bool clear) {
    if (clear) {
        tft.drawPixel(0, 0, 0);
        tft.fillScreen(bruceConfig.bgColor);
    }
    tft.setTextDatum(0);
    uiDrawStatusBar();
    // Leave the cursor at the first content row, so callers that print right away start below the bar.
    setTftDisplay(cui::PAD, cui::TOP, bruceConfig.priColor, FP, bruceConfig.bgColor);

#if defined(HAS_TOUCH)
    TouchFooter();
#endif
}

#endif // UI_COMPACT
