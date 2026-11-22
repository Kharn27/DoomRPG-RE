#!/usr/bin/env python3
"""Guard SD-neutral palette, row-based three-stop gradients and full redraw."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/"src"/"esp_uac_menu_preview.cpp").read_text()
for marker in (
    '#include "esp_uac_font_theme.h"',
    'fontIsNeutral=fromSD && bmp.paletteCount==16U;',
    'EspUacFontTheme_tintColor(fontSourcePalette[k],tone);',
    'EspUacFontTheme_scale565(color,2U,5U)',
    'EspUacMenuPreview_needsBlink(void)',
    'kBlinkHalfPeriodMs=450U',
    'EspUacFontTheme_isGrayRgb565(c)',
    'EspUacFontTheme_verticalTone(',
    'EspMainMenuSkin_letterTone(0,9)',
    'EspMainMenuSkin_letterTone(4,9)',
    'EspMainMenuSkin_letterTone(8,9)',
    'rowInk[k]=fontIsNeutral?',
    'EspUacFontTheme_tintGray(fontSourcePalette[k],tone):',
    'uint16_t color=rowInk[ink];',
    'paintedTheme==theme',
    'paintedTheme=theme;',
    'ramp=vertical-3stops',
): assert marker in s, marker
assert 'EspMainMenuSkin_uiInk()' not in s, "flat single-color UI ink resurrected"
assert 'fontActivePalette' not in s, "flat per-theme precomputed palette resurrected"
assert s.count('rowInk[k]=fontIsNeutral?')==1
assert 'kDoomFontInk' not in s, 'old colored-font non-theme fallback resurrected'
assert 'index!=selected || !optionsArmed' not in s, 'inactive labels dimmed again'
assert 'const int previous=(current==paintedMode && paintedPage==page &&' in s
assert 'paintedTheme==theme)?' in s
print("UAC theme/blink source PASS: 3-stop ramps for neutral + colored SD/PAK, selected-only pulse")
