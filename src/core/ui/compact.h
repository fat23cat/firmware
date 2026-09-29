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

// ---- widgets (compact_widgets.cpp) ----
void uiDrawStatusBar();
void uiDrawMainBorder(bool clear);

#else
#define UIC(legacy, compact) (legacy)
#endif

#endif // __UI_COMPACT_H__
