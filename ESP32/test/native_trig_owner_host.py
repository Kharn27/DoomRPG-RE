#!/usr/bin/env python3
"""Compile the native trig owner and prove exact 256-entry LE bitwise parity."""
from pathlib import Path
import shutil
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
header=(root/"src/Render.h").read_text(encoding="latin-1")
assert "#ifndef DOOMRPG_ESP32\n\tint sinTable[256];\n#endif" in header
for name in ("esp_native_first_frame.c","esp_native_bsp_visibility.c","esp_native_sprite_renderer.c"):
    s=(root/"ESP32/src"/name).read_text()
    assert "EspNativeTrig_view()" in s and "->sinTable" not in s
c=r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_native_trig.h"
int main(void){
 uint8_t p[ESP_NATIVE_TRIG_BYTES];
 for(uint32_t i=0;i<256;++i){
  uint32_t u=(i*0x01030507U)^0x80000000U;
  p[4*i]=(uint8_t)u;p[4*i+1]=(uint8_t)(u>>8);
  p[4*i+2]=(uint8_t)(u>>16);p[4*i+3]=(uint8_t)(u>>24);
 }
 assert(EspNativeTrig_view()==NULL);
 assert(!EspNativeTrig_installLe(NULL,sizeof(p)));
 assert(!EspNativeTrig_installLe(p,sizeof(p)-1U));
 assert(EspNativeTrig_view()==NULL);
 for(int n=0;n<4096;++n){
  assert(EspNativeTrig_installLe(p,sizeof(p)));
  const int32_t* table=EspNativeTrig_view();
  assert(table!=NULL);
  for(uint32_t i=0;i<256;++i){
   uint32_t actual,expected=(i*0x01030507U)^0x80000000U;
   memcpy(&actual,&table[i],sizeof(actual));assert(actual==expected);
  }
  EspNativeTrig_reset();assert(EspNativeTrig_view()==NULL);
 }
 puts("PASS native sine table 4096 exact full-table installs and fail-closed");
 return 0;
}
"""
with tempfile.TemporaryDirectory(prefix="native-trig-") as d:
 p=Path(d)/"t.c";p.write_text(c)
 exe=Path(d)/"t"
 subprocess.run([shutil.which("cc") or "cc","-std=c11","-O2","-Wall","-Wextra","-Werror","-IESP32/include",str(p),"ESP32/src/esp_native_trig.c","-o",str(exe)],cwd=root,check=True)
 subprocess.run([str(exe)],check=True)
