#!/usr/bin/env python3
"""Guard UAC neutral-SD provenance, theme refresh and forced full repaint."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/"src"/"esp_uac_menu_preview.cpp").read_text()
for marker in (
    '#include "esp_uac_font_theme.h"',
    'fontIsNeutral=fromSD && bmp.paletteCount==16U;',
    'EspUacFontTheme_isGrayRgb565(c)',
    'EspUacFontTheme_tintGray(fontSourcePalette[i],inkTone)',
    'const uint16_t inkTone=EspMainMenuSkin_uiInk();',
    'fontPaletteTheme=theme;',
    'refreshLargerPalette();',
    'uint16_t color=fontActivePalette[ink];',
    'paintedTheme==theme',
    'paintedTheme=theme;',
    'palette=%s',
): assert marker in s, marker
assert s.count('fontActivePalette[ink]')==1
assert s.count('uint16_t color=kDoomFontInk[ink];')==0
assert 'const int previous=(current==paintedMode && paintedPage==page &&' in s
assert 'paintedTheme==theme)?' in s
print("UAC neutral theme integration source PASS: SD detection, 3 themes, full repaint")
