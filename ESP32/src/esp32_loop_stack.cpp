#include <Arduino.h>

/*
 * Classic CYD has no PSRAM and the native BSP renderer legitimately comes very
 * close to Arduino-ESP32's default 8 KiB loopTask stack. Keep the renderer's
 * bounded automatic workspace (so it does not consume scarce permanent DRAM),
 * but give the loop task 1 KiB of explicit headroom for the recovered recursive
 * BSP walk and failure-only diagnostics.
 *
 * Arduino-ESP32 2.0.17 exposes getArduinoLoopTaskStackSize() as a weak symbol;
 * SET_LOOP_TASK_STACK_SIZE supplies the application override before loopTask is
 * created. 9 KiB costs only 1 KiB more heap than the framework default.
 */
SET_LOOP_TASK_STACK_SIZE(9 * 1024);
