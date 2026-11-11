#!/usr/bin/env python3
"""SDL-F16: retire last inert SDL fatal cleanup facades and desktop video header.

Prove whole-file pre-F16 Git source parity, compare compiled real DoomRPG_Error
C-body stdout / teardown / exit for old vs new with exact no-op close stubs,
and reject retired symbols from all native source and three linked Xtensa ELFs.
"""
from pathlib import Path
import hashlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
OWNER = ROOT / "ESP32/src/engine_stubs.c"
PRE_F16_BLOB = "d20b081200a75226d88a7b5279142eab178fd508"
INC = '#include "SDL_Video.h"\n'
CALLS = '    SDL_CloseAudio();\n    SDL_Close();\n'
DEFINITIONS = 'void SDL_Close(void) {}\n\nvoid SDL_CloseAudio(void) {}\n\n'
RETIRED = ("SDL_Close", "SDL_CloseAudio")


def restore_pre_f16(current: str) -> str:
    include_anchor = '#include "Render.h"\n'
    fatal_anchor = '    DoomRPG_FreeAppData(doomRpg);\n    exit(0);\n'
    defs_anchor = 'void Sound_stopSounds(Sound_t* sound)'
    for anchor in (include_anchor, fatal_anchor, defs_anchor):
        assert current.count(anchor) == 1, anchor
    return (current.replace(include_anchor, include_anchor + INC, 1)
                   .replace(fatal_anchor, '    DoomRPG_FreeAppData(doomRpg);\n' + CALLS + '    exit(0);\n', 1)
                   .replace(defs_anchor, DEFINITIONS + defs_anchor, 1))


def git_blob(text: str) -> str:
    raw = text.encode("utf-8")
    return hashlib.sha1(b"blob " + str(len(raw)).encode() + b"\0" + raw).hexdigest()


def fatal_function(source: str) -> str:
    signature = 'void DoomRPG_Error(const char* fmt, ...)'
    assert source.count(signature) == 1
    start = source.index(signature)
    brace = source.index('{', start)
    level = 0
    for pos in range(brace, len(source)):
        if source[pos] == '{':
            level += 1
        elif source[pos] == '}':
            level -= 1
            if level == 0:
                return source[start:pos + 1]
    raise AssertionError('DoomRPG_Error body not found')


# Compile two real production function bodies using historical no-op closers.
# Both must print identically and perform root teardown once before exit(0).
C_FIXTURE = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct DoomRPG_s { int identity; } DoomRPG_t;
static DoomRPG_t liveRoot = { 117 };
static DoomRPG_t* doomRpg = &liveRoot;
static int teardownCount = 0;
static int exitStatus = -1;
static jmp_buf jumpOnExit;
void DoomRPG_FreeAppData(DoomRPG_t* root) {
    assert(root == &liveRoot && root == doomRpg);
    teardownCount++;
    doomRpg = NULL;
}
void SDL_CloseAudio(void) {}
void SDL_Close(void) {}
static void record_exit(int status) {
    exitStatus = status;
    longjmp(jumpOnExit, 1);
}
#define exit record_exit
__ACTUAL_FATAL_FUNCTION__
#undef exit
int main(void) {
    char longText[600];
    memset(longText, 'q', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';
    for (volatile int i=0; i<5; i++) {
        doomRpg = &liveRoot;
        teardownCount = 0;
        exitStatus = -1;
        if (setjmp(jumpOnExit) == 0) {
            switch (i) {
            case 0: DoomRPG_Error("short failure"); break;
            case 1: DoomRPG_Error("id=%d item=%s", 23, "pak"); break;
            case 2: DoomRPG_Error("%s", longText); break;
            case 3: DoomRPG_Error("percent 100%% code 0x%x", 0xabc); break;
            default: DoomRPG_Error("%.8s", "truncate-this"); break;
            }
            return 95; /* Fatal must not return. */
        }
        assert(teardownCount == 1 && exitStatus == 0 && doomRpg == NULL);
        printf("\nFATAL-CASE=%d TEARDOWN=%d EXIT=%d\n", i, teardownCount, exitStatus);
    }
    return 0;
}
'''


def check_fatal_parity(current: str, old: str) -> None:
    new_fatal = fatal_function(current)
    old_fatal = fatal_function(old)
    assert old_fatal.replace(CALLS, '') == new_fatal
    assert 'DoomRPG_FreeAppData(doomRpg);' in new_fatal
    assert new_fatal.index('DoomRPG_FreeAppData(doomRpg);') < new_fatal.index('exit(0);')
    with tempfile.TemporaryDirectory(prefix='sdl-f16-fatal-') as tmp:
        outputs = []
        for version, function in (('legacy-noop', old_fatal), ('native', new_fatal)):
            cfile = Path(tmp) / (version + '.c')
            binary = Path(tmp) / version
            cfile.write_text(C_FIXTURE.replace('__ACTUAL_FATAL_FUNCTION__', function),
                             encoding='utf-8')
            built = subprocess.run(['cc', '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                                    str(cfile), '-o', str(binary)],
                                   capture_output=True, text=True)
            assert built.returncode == 0, (version, built.stderr)
            run = subprocess.run([str(binary)], capture_output=True)
            assert run.returncode == 0, (version, run.returncode, run.stderr)
            assert run.stdout.count(b'FATAL-CASE=') == 5, (version, run.stdout)
            outputs.append(run.stdout)
        assert outputs[0] == outputs[1], 'DoomRPG_Error runtime parity diverged'
    print('PASS SDL-F16 compiled old/new real DoomRPG_Error: 5 fatal cases, '
          'identical output, teardown-before-exit, no return')


def check_sources() -> None:
    current = OWNER.read_text(encoding='utf-8')
    old = restore_pre_f16(current)
    sha = git_blob(old)
    assert sha == PRE_F16_BLOB, (sha, PRE_F16_BLOB)
    assert INC in old and INC not in current
    assert CALLS in old and CALLS not in current
    assert DEFINITIONS in old and DEFINITIONS not in current
    for preserved in ('void DoomRPG_FreeAppData(DoomRPG_t* doomrpg)',
                      'DoomRPG_FreeAppData(doomRpg);',
                      'Esp32PlatformVideo_framebuffer()',
                      'SDL_Rect screenRect;', 'void Z_Init(void) {}',
                      'Sound_stopSounds(Sound_t* sound)', 'exit(0);'):
        assert preserved in current, preserved
    legacy = (ROOT / 'src/SDL_Video.h').read_text(encoding='latin-1')
    assert 'void SDL_Close(void);' in legacy
    assert 'void SDL_CloseAudio(void);' in legacy
    assert '-<esp32_sdl.cpp>' in (ROOT / 'ESP32/platformio.ini').read_text()
    count = 0
    for folder in (ROOT / 'ESP32/src', ROOT / 'ESP32/include'):
        for path in folder.rglob('*'):
            if path.suffix not in ('.c', '.cpp', '.h', '.hpp') or path.name == 'esp32_sdl.cpp':
                continue
            count += 1
            source = path.read_text(encoding='latin-1')
            assert not re.search(r'#\s*include\s*[<"]SDL_Video\.h[>"]', source), path
            active = re.sub(r'/\*[\s\S]*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', ' ', source)
            for name in RETIRED:
                assert not re.search(r'\b' + name + r'\b', active), (path, name)
    assert count >= 300, count
    check_fatal_parity(current, old)
    print(f'PASS SDL-F16 {count} native source files: fatal no-op closes + desktop '
          f'SDL_Video include absent; whole pre-F16 Git blob {PRE_F16_BLOB}')


def check_elfs() -> None:
    nm = shutil.which('xtensa-esp32-elf-nm')
    nm = nm or str(Path.home() / '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm')
    assert Path(nm).exists(), 'Xtensa nm missing'
    for profile in ('esp32-cyd', 'esp32-cyd-bringup', 'esp32-cyd-trace'):
        elf = ROOT / 'ESP32/.pio/build' / profile / 'firmware.elf'
        assert elf.is_file(), elf
        r = subprocess.run([str(nm), '--defined-only', str(elf)],
                           capture_output=True, text=True, check=True)
        names = {ln.split()[-1] for ln in r.stdout.splitlines() if ln.split()}
        remaining = sorted(names.intersection(RETIRED))
        assert not remaining, (profile, remaining)
        print(f'PASS SDL-F16 {profile} ELF: no SDL fatal close facades')


if __name__ == '__main__':
    check_sources()
    if len(sys.argv) > 1:
        assert sys.argv[1:] == ['--elf'], sys.argv
        check_elfs()
