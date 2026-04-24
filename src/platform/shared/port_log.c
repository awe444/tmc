/**
 * @file port_log.c
 * @brief Implementation of `Port_LogEvent` (declared in
 *        `include/platform/port.h`).
 *
 * The shipping GBA ROM has no stderr, so this file is host-only:
 * `PORT_LOG_EVENT(...)` is defined as a no-op when `__PORT__` is not
 * set, and the corresponding `Port_LogEvent()` function lives only in
 * the SDL port build. See port.h for the macro and intended use.
 *
 * The output format is intentionally simple and stable so the headless
 * scripted-input smoke test (`tmc_sdl --frames=N --input-script=...`)
 * can grep / diff event timelines:
 *
 *     [tmc f=  350] task: TASK_TITLE -> TASK_FILE_SELECT
 *     [tmc f=  600] cutscene: start (idx=4)
 *     [tmc f=  812] cutscene: stop  (idx=4)
 *
 * The frame number comes from `Port_GetFrameCount()` (the host-side
 * VBlank counter incremented in src/platform/shared/interrupts.c) so
 * lines are anchored to the same clock the `--press=`/`--input-script=`
 * machinery uses, making it easy to correlate cause (input) with
 * effect (engine state change).
 */
#include "platform/port.h"

#include <stdarg.h>
#include <stdio.h>

void Port_LogEvent(const char* category, const char* fmt, ...) {
    /* Defensive: a NULL category or fmt should never crash logging. */
    if (category == NULL) {
        category = "?";
    }
    if (fmt == NULL) {
        return;
    }
    fprintf(stderr, "[tmc f=%6u] %s: ", (unsigned)Port_GetFrameCount(), category);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}
