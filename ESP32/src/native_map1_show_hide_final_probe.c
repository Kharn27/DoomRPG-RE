#include <SDL.h>
#include "DoomRPG.h"

/*
 * Keep the legacy boolean enum visible before ESP-IDF heap headers pull in
 * stdbool-style false/true macros. The implementation body remains byte-for-byte
 * identical to the previously reviewed final probe and lives in the .inc file.
 */
#include "native_map1_show_hide_final_probe_impl.inc"
