#!/usr/bin/env python3
"""ARCH-RC6 input-reset boundary: real C body and strong/weak link contracts.

The original --wrap called Transition_reset() before the input intent reset.
Check that production always makes a strong call, while standalone host builds
are linkable without the larger transition module. Host != CYD hardware test.
"""
from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ESP = Path(__file__).resolve().parents[1]
INPUT = ESP / "src/esp_native_gameplay_input.c"
TRANSITION = ESP / "src/esp_native_gameplay_transition.c"
INI = ESP / "platformio.ini"


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise AssertionError(reason)


def extract(text: str, signature: str) -> str:
    require(text.count(signature) == 1, "missing or duplicate real C body " + signature)
    start = text.index(signature)
    begin = text.find("{", start)
    require(begin >= 0, "missing opening brace " + signature)
    depth = 0
    for i in range(begin, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise AssertionError("unclosed function " + signature)


PREFIX = r'''
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "esp_native_gameplay_input.h"
static EspNativeGameplayInputState inputState;
#if defined(ARDUINO_ARCH_ESP32)
extern void EspNativeGameplayTransition_reset(void);
#else
extern void EspNativeGameplayTransition_reset(void) __attribute__((weak));
#endif
static int transitionResets = 0;
static int originalPendingWitness = 0;
static int expectedPending = 0;
'''
MOCK = r'''
void EspNativeGameplayTransition_reset(void) {
    ++transitionResets;
    if ((int)inputState.pending == expectedPending) {
        ++originalPendingWitness;
    }
}
'''
POST = r'''
static int allZero(void) {
    EspNativeGameplayInputState zero;
    memset(&zero, 0, sizeof(zero));
    return memcmp(&inputState, &zero, sizeof(zero)) == 0;
}
int main(void) {
    memset(&inputState, 0xa5, sizeof(inputState));
    inputState.pending = 1U;
    expectedPending = 1;
    EspNativeGameplayInput_reset();
    assert(allZero());
#ifdef HAVE_TRANSITION_OWNER
    assert(transitionResets == 1 && originalPendingWitness == 1);
#else
    assert(transitionResets == 0);
#endif

    /* Even an empty input lease still resets the transition owner. */
    expectedPending = 0;
    EspNativeGameplayInput_reset();
    assert(allZero());
#ifdef HAVE_TRANSITION_OWNER
    assert(transitionResets == 2 && originalPendingWitness == 2);
#else
    assert(transitionResets == 0);
#endif

    /* A dirty high field must be cleared, not only pending/action. */
    memset(&inputState, 0xf3, sizeof(inputState));
    inputState.pending = 1;
    expectedPending = 1;
    EspNativeGameplayInput_reset();
    assert(allZero());
#ifdef HAVE_TRANSITION_OWNER
    assert(transitionResets == 3 && originalPendingWitness == 3);
#endif
    return 0;
}
'''


def harness(body: str, owner: bool) -> str:
    return "\n".join([PREFIX, MOCK if owner else "", body, POST])


def run(body: str, native: bool, owner: bool, expect_success: bool) -> None:
    compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    require(compiler is not None, "no host C compiler available")
    with tempfile.TemporaryDirectory(prefix="arch-rc6-input-") as temp:
        src = Path(temp) / "boundary.c"
        exe = Path(temp) / "boundary"
        src.write_text(harness(body, owner))
        args = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ESP / "include"), str(src), "-o", str(exe)]
        if native:
            args.insert(1, "-DARDUINO_ARCH_ESP32=1")
        if owner:
            args.insert(1, "-DHAVE_TRANSITION_OWNER=1")
        result = subprocess.run(args, capture_output=True, text=True)
        require(result.returncode == 0, "real source fixture compile: " + result.stderr)
        outcome = subprocess.run([str(exe)], capture_output=True, text=True)
        require((outcome.returncode == 0) == expect_success,
                f"unexpected reset outcome native={native} owner={owner} "
                f"expected={expect_success} exit={outcome.returncode}")


def main() -> None:
    inp = INPUT.read_text()
    trn = TRANSITION.read_text()
    ini = INI.read_text()
    reset = extract(inp, "void EspNativeGameplayInput_reset(void)")
    reset_core = extract(trn, "void EspNativeGameplayTransition_reset(void)")
    require("memset(&transitionState, 0, sizeof(transitionState))" in reset_core,
            "actual transition owner reset changed")
    require("#if defined(ARDUINO_ARCH_ESP32)\n"
            "#include \"esp_native_gameplay_transition.h\"\n"
            "#else\n"
            "extern void EspNativeGameplayTransition_reset(void)"
            " __attribute__((weak));\n"
            "#endif" in inp, "strong ESP32 and weak host-only separation lost")
    require(re.search(r"(?m)^\s*-Wl,--wrap=EspNativeGameplayInput_reset\s*$",
                      ini) is None, "retired GNU ld input reset wrap returned")
    require("__wrap_EspNativeGameplayInput_reset" not in trn and
            "__real_EspNativeGameplayInput_reset" not in trn,
            "retired transition-wrapper alias still linked")
    require(reset.index("EspNativeGameplayTransition_reset();") <
            reset.index("memset(&inputState, 0, sizeof(inputState));"),
            "transition-first order regressed")
    require(reset.count("EspNativeGameplayTransition_reset();") == 2,
            "strong/weak branch reset expectation changed")
    # Three independently linked modes, same real C reset body:
    # ESP32 strong owner, host with owner, and host without owner.
    run(reset, True, True, True)
    run(reset, False, True, True)
    run(reset, False, False, True)

    mutants = [
        reset.replace("    EspNativeGameplayTransition_reset();\n", "", 1),
        reset.replace("    memset(&inputState, 0, sizeof(inputState));\n", ""),
        reset.replace("    memset(&inputState, 0, sizeof(inputState));",
                      "    memset(&inputState, 0, sizeof(inputState) - 1U);"),
        reset.replace("    EspNativeGameplayTransition_reset();\n",
                      "    EspNativeGameplayTransition_reset();\n"
                      "    EspNativeGameplayTransition_reset();\n", 1),
        reset.replace("    EspNativeGameplayTransition_reset();\n",
                      "    if (inputState.pending) EspNativeGameplayTransition_reset();\n", 1),
        reset.replace("    memset(&inputState, 0, sizeof(inputState));",
                      "    memset(&inputState, 0, sizeof(inputState));\n"
                      "    EspNativeGameplayTransition_reset();"),
        reset.replace("    memset(&inputState, 0, sizeof(inputState));",
                      "    memset(&inputState, 0, sizeof(inputState));\n"
                      "    inputState.pending = 1U;"),
    ]
    for i, changed in enumerate(mutants, 1):
        require(changed != reset, f"inert mutant {i}")
        run(changed, True, True, False)
    print("PASS ARCH-RC6 real-source C: strong ESP32 + weak host linked/unlinked, "
          "3 reset epochs each; 7 adverse guards, transition-first preserved")


if __name__ == "__main__":
    main()
