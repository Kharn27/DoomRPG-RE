#!/usr/bin/env python3
"""Guard SD-neutral palette, row-based three-stop gradients and full redraw."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/"src"/"esp_uac_menu_preview.cpp").read_text()
for marker in (
    '#include "esp_uac_font_theme.h"',
    'fontIsNeutral=fromSD && bmp.paletteCount==16U;',
    'EspUacFontTheme_isGrayRgb565(c)',
    'EspUacFontTheme_verticalTone(',
    'EspMainMenuSkin_letterTone(0,9)',
    'EspMainMenuSkin_letterTone(4,9)',
    'EspMainMenuSkin_letterTone(8,9)',
    'rowInk[k]=EspUacFontTheme_tintGray(fontSourcePalette[k],tone);',
    'fontIsNeutral?rowInk[ink]:kDoomFontInk[ink]',
    'paintedTheme==theme',
    'paintedTheme=theme;',
    'ramp=vertical-3stops',
): assert marker in s, marker
assert 'EspMainMenuSkin_uiInk()' not in s, "flat single-color UI ink resurrected"
assert 'fontActivePalette' not in s, "flat per-theme precomputed palette resurrected"
assert s.count('rowInk[k]=EspUacFontTheme_tintGray(')==1
assert 'const int previous=(current==paintedMode && paintedPage==page &&' in s
assert 'paintedTheme==theme)?' in s
print("UAC neutral theme source PASS: 3-stop smooth vertical ramps, SD-only, full redraw")
