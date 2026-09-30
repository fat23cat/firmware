#ifndef __UI_COMPACT_H__
#define __UI_COMPACT_H__
// Compact UI layout for small landscape screens (Cardputer / Cardputer ADV).
// Enabled per board with -DUI_COMPACT=1; toggled at runtime in Config > Display & UI.
//
// Nothing outside #ifdef UI_COMPACT except the UIC fallback, so on other boards this header
// adds no includes and cannot change the generated code of the files that include it.
// UIC(legacy, compact) may only replace a value expression, and `legacy` must be the exact
// original expression.

#ifdef UI_COMPACT
#include <Arduino.h>
#include <globals.h>
#include <vector>

// Build default used while the user has never touched the toggle (bruceConfig.uiCompact == -1).
#ifndef UI_COMPACT_DEFAULT
#define UI_COMPACT_DEFAULT 0
#endif

bool uiCompactSetting(); // effective user setting: -1 -> UI_COMPACT_DEFAULT, else 0/1
bool uiCompact();        // uiCompactSetting() && landscape
void uiToggleCompact();  // stores an explicit 0/1

#define UIC(legacy, compact) (uiCompact() ? (compact) : (legacy))

// ---- layout tokens (240x135 landscape). Namespace `cui` (compact UI): `ui` is too generic, and
// modules already use it (e.g. a local `UILayout ui` in audio_player.cpp). ----
namespace cui {
constexpr int16_t SB_H = 18;      // status bar height; 1px rule drawn at y = SB_H
constexpr int16_t TOP = SB_H + 3; // first content row
constexpr int16_t PAD = 6;        // side padding
constexpr int16_t ROW_FM = 18;    // list row pitch at FM
constexpr int16_t ROW_FP = 11;    // list row pitch at FP
constexpr int16_t SCROLLBAR_W = 3;
} // namespace cui

// ---- text helpers (compact_text.cpp). Widths use the monospace GLCD model: len * LW * size ----
int32_t uiTextW(const String &s, uint8_t size);
uint8_t uiFitSize(const String &s, int16_t maxW, uint8_t pref = FM, uint8_t min = FP);
String uiTruncate(const String &s, int16_t maxW, uint8_t size);       // "abcdef.."
String uiTruncateMiddle(const String &s, int16_t maxW, uint8_t size); // "long_na..me.sub"
// Word-wraps into lines of at most maxW pixels; maxLines == 0 means unlimited.
// When lines are dropped, the last kept line ends with "..".
std::vector<String> uiWrap(const String &s, int16_t maxW, uint8_t size, uint8_t maxLines = 0);
// Draws `s` at the current text size, aligned by `datum` (TL/TC/TR, ML/MC/MR, BL/BC/BR, baselines).
// Always emits drawString (left, with TL_DATUM), drawCentreString or drawRightString, because the
// WebUI mirror (tft_logger) records only those three and ignores the datum.
void uiDrawText(const String &s, int16_t x, int16_t y, uint8_t datum);
// Draws one line: tries `pref`, then FP, then truncates with "..". Returns the size used.
uint8_t uiDrawFit(
    const String &s, int16_t x, int16_t y, int16_t maxW, uint8_t datum, uint8_t pref, uint16_t fg,
    uint16_t bg
);

// ---- pure layout logic (compact_layout.cpp); no drawing, host-testable ----
// First visible row of a list window: keeps `index` visible and moves only when it leaves the window.
// `reset` (first render of a new list) starts from 0. Result is always in [0, max(0, n - visible)].
int uiListWindowFirst(int index, int n, int visible, int prevFirst, bool reset);
// Item shown in a submenu wheel slot. Slots are filled in the order of offsets 0, +1, -1, +2, -2
// (see uiWheelOffset); only the first min(5, n) slots are used, so no item appears twice.
int uiWheelSlotCount(int n);
int uiWheelOffset(int slot); // 0, +1, -1, +2, -2
int uiWheelItem(int index, int slot, int n);
// Mixes `fg` towards `bg`: t = 0 -> bg, t = 256 -> fg (RGB565).
uint16_t uiBlend565(uint16_t fg, uint16_t bg, uint16_t t);

// ---- widgets (compact_widgets.cpp) ----
struct Opt_Coord; // core/display.h

void uiDrawStatusBar();
void uiDrawMainBorder(bool clear);
// Full-width list: one text size for the whole list (FM if every label fits, else FP), every visible
// row repainted on each call. Leaves the list's text size set, for the marquee in loopOptions.
Opt_Coord uiDrawOptions(
    int index, std::vector<Option> &options, uint16_t fgcolor, uint16_t selcolor, uint16_t bgcolor,
    bool firstRender
);
// 5-item vertical wheel: selected item FM (FP / ".." if long), neighbours FP.
void uiDrawSubmenu(int index, std::vector<Option> &options, const char *title);
// Main-menu label: FM -> FP -> "..", centred on centerX and vertically inside the FM row at y.
void uiMenuTitle(const String &name, int16_t centerX, int16_t y);
// uiDrawOptions() requests a marquee restart whenever it redraws the selected row (it shows the
// start of the label); displayScrollingText() consumes the request.
bool uiMarqueeConsumeReset();

#else
#define UIC(legacy, compact) (legacy)
#endif

#endif // __UI_COMPACT_H__
