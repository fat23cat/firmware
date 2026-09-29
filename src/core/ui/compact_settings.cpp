#include "compact.h"

#ifdef UI_COMPACT

bool uiCompactSetting() {
    return bruceConfig.uiCompact < 0 ? UI_COMPACT_DEFAULT : bruceConfig.uiCompact == 1;
}

bool uiCompact() { return uiCompactSetting() && tftWidth > tftHeight; }

void uiToggleCompact() { bruceConfig.setUiCompact(uiCompactSetting() ? 0 : 1); }

#endif // UI_COMPACT
