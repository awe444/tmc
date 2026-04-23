/**
 * @file scripted_input.c
 * @brief Test-harness input scripting for the SDL port.
 *
 * Lets the headless CI smoke test drive button presses on specific frames
 * (e.g., press START at frame 60 for 10 frames to dismiss the title
 * screen). The mask is OR'd into the keyboard / gamepad mask by
 * `src/platform/sdl/input.c::Port_InputPump()` so scripted presses appear
 * in the emulated `REG_KEYINPUT` slot through the same code path real
 * input does.
 *
 * The CLI front-end is in `src/platform/sdl/main.c` (`--press=`,
 * `--input-script=`); the runtime API is declared in
 * `include/platform/port.h`.
 */
#include "platform/port.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bound chosen high enough for any plausible CI script (most scripts will
 * have under a dozen entries) but small enough to keep the table in BSS
 * cheap. Each entry is 12 bytes. */
#define PORT_SCRIPTED_INPUT_MAX_ENTRIES 256

typedef struct {
    uint32_t start_frame;
    uint32_t end_frame; /* exclusive */
    uint16_t mask;
} ScriptedEntry;

static ScriptedEntry s_entries[PORT_SCRIPTED_INPUT_MAX_ENTRIES];
static size_t s_entry_count = 0;
/* When non-zero, suppress diagnostic prints from the parser. Used by the
 * self-check to keep deliberate parse-error tests from littering CI logs. */
static int s_quiet = 0;

static void diag(const char* fmt, ...) {
    if (s_quiet) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void Port_ScriptedInputReset(void) {
    s_entry_count = 0;
}

int Port_ScriptedInputAdd(uint32_t start_frame, uint32_t duration, uint16_t mask) {
    if (duration == 0) {
        return -1;
    }
    if ((mask & ~PORT_SCRIPTED_KEYS_MASK) != 0) {
        return -1;
    }
    if (s_entry_count >= PORT_SCRIPTED_INPUT_MAX_ENTRIES) {
        diag("[tmc_sdl] scripted-input table full (max %d entries)\n", PORT_SCRIPTED_INPUT_MAX_ENTRIES);
        return -1;
    }
    /* Guard against UINT32_MAX overflow when computing end_frame. */
    uint32_t end_frame;
    if (duration > UINT32_MAX - start_frame) {
        end_frame = UINT32_MAX;
    } else {
        end_frame = start_frame + duration;
    }
    s_entries[s_entry_count].start_frame = start_frame;
    s_entries[s_entry_count].end_frame = end_frame;
    s_entries[s_entry_count].mask = mask;
    s_entry_count++;
    return 0;
}

uint16_t Port_ScriptedInputCurrentMask(void) {
    return Port_ScriptedInputMaskForFrame(Port_GetFrameCount());
}

uint16_t Port_ScriptedInputMaskForFrame(uint32_t frame) {
    if (s_entry_count == 0) {
        return 0;
    }
    uint16_t mask = 0;
    for (size_t i = 0; i < s_entry_count; ++i) {
        if (frame >= s_entries[i].start_frame && frame < s_entries[i].end_frame) {
            mask |= s_entries[i].mask;
        }
    }
    return mask;
}

/* ------------------------------------------------------------------------ */
/* Spec parser                                                              */
/* ------------------------------------------------------------------------ */

typedef struct {
    const char* name;
    uint16_t bit;
} KeyName;

static const KeyName k_key_names[] = {
    { "A", PORT_SCRIPTED_KEY_A },           { "B", PORT_SCRIPTED_KEY_B },         { "START", PORT_SCRIPTED_KEY_START },
    { "SELECT", PORT_SCRIPTED_KEY_SELECT }, { "UP", PORT_SCRIPTED_KEY_UP },       { "DOWN", PORT_SCRIPTED_KEY_DOWN },
    { "LEFT", PORT_SCRIPTED_KEY_LEFT },     { "RIGHT", PORT_SCRIPTED_KEY_RIGHT }, { "L", PORT_SCRIPTED_KEY_L },
    { "R", PORT_SCRIPTED_KEY_R },
};

static int strieq_n(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (toupper(ca) != toupper(cb)) {
            return 0;
        }
        if (ca == 0) {
            return 1;
        }
    }
    return 1;
}

static int lookup_key(const char* name, size_t len, uint16_t* out_bit) {
    for (size_t i = 0; i < sizeof(k_key_names) / sizeof(k_key_names[0]); ++i) {
        const char* kn = k_key_names[i].name;
        if (strlen(kn) == len && strieq_n(kn, name, len)) {
            *out_bit = k_key_names[i].bit;
            return 1;
        }
    }
    return 0;
}

/* Skip ASCII whitespace. */
static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    return p;
}

/* Parse an unsigned decimal integer, advancing *pp past it. Returns 0 on
 * success. */
static int parse_uint(const char** pp, uint32_t* out) {
    const char* p = *pp;
    if (!isdigit((unsigned char)*p)) {
        return -1;
    }
    char* end = NULL;
    errno = 0;
    unsigned long v = strtoul(p, &end, 10);
    if (errno != 0 || end == p || v > UINT32_MAX) {
        return -1;
    }
    *out = (uint32_t)v;
    *pp = end;
    return 0;
}

/* Parse a single ENTRY := KEYS '@' START [ '+' DURATION ]
 * Sub-string runs from `begin` to `end` (exclusive). */
static int parse_entry(const char* begin, const char* end) {
    /* Copy the entry into a local NUL-terminated buffer so the helpers
     * (`skip_ws`, `parse_uint` / strtoul) are free to scan past their
     * current position without bleeding into the next entry's bytes. */
    size_t len = (size_t)(end - begin);
    if (len >= 256) {
        diag("[tmc_sdl] scripted-input: entry too long (>= 256 chars)\n");
        return -1;
    }
    char buf[256];
    memcpy(buf, begin, len);
    buf[len] = '\0';

    const char* p = skip_ws(buf);
    char* tail = buf + len;
    while (tail > p && (tail[-1] == ' ' || tail[-1] == '\t')) {
        --tail;
        *tail = '\0';
    }
    if (p == tail) {
        return 0; /* empty entry between commas — tolerate */
    }
    const char* entry_end = tail;

    /* KEYS up to '@' */
    const char* at = strchr(p, '@');
    if (at == NULL || at >= entry_end) {
        diag("[tmc_sdl] scripted-input: missing '@' in entry: %s\n", p);
        return -1;
    }

    /* Parse KEYS as KEY ('|' KEY)* */
    uint16_t mask = 0;
    const char* keys_begin = p;
    const char* keys_end = at;
    while (keys_begin < keys_end) {
        keys_begin = skip_ws(keys_begin);
        if (keys_begin >= keys_end) {
            break;
        }
        const char* k_end = keys_begin;
        while (k_end < keys_end && *k_end != '|' && *k_end != ' ' && *k_end != '\t') {
            ++k_end;
        }
        if (k_end == keys_begin) {
            diag("[tmc_sdl] scripted-input: empty key name in entry: %s\n", p);
            return -1;
        }
        uint16_t bit = 0;
        if (!lookup_key(keys_begin, (size_t)(k_end - keys_begin), &bit)) {
            diag("[tmc_sdl] scripted-input: unknown key '%.*s' in entry: %s\n", (int)(k_end - keys_begin), keys_begin,
                 p);
            return -1;
        }
        mask |= bit;
        keys_begin = skip_ws(k_end);
        if (keys_begin < keys_end && *keys_begin == '|') {
            ++keys_begin;
        } else if (keys_begin < keys_end) {
            diag("[tmc_sdl] scripted-input: unexpected character before '@' in entry: %s\n", p);
            return -1;
        }
    }
    if (mask == 0) {
        diag("[tmc_sdl] scripted-input: missing key name in entry: %s\n", p);
        return -1;
    }

    /* START [ '+' DURATION ] */
    const char* sp = skip_ws(at + 1);
    uint32_t start_frame = 0;
    if (parse_uint(&sp, &start_frame) != 0) {
        diag("[tmc_sdl] scripted-input: bad START frame in entry: %s\n", p);
        return -1;
    }
    uint32_t duration = 1;
    sp = skip_ws(sp);
    if (sp < entry_end && *sp == '+') {
        ++sp;
        sp = skip_ws(sp);
        if (parse_uint(&sp, &duration) != 0) {
            diag("[tmc_sdl] scripted-input: bad DURATION in entry: %s\n", p);
            return -1;
        }
    }
    sp = skip_ws(sp);
    if (sp != entry_end) {
        diag("[tmc_sdl] scripted-input: trailing characters in entry: %s\n", p);
        return -1;
    }
    if (duration == 0) {
        diag("[tmc_sdl] scripted-input: DURATION must be > 0 in entry: %s\n", p);
        return -1;
    }

    return Port_ScriptedInputAdd(start_frame, duration, mask);
}

int Port_ScriptedInputParse(const char* spec) {
    if (spec == NULL) {
        return -1;
    }
    const char* p = spec;
    const char* end = spec + strlen(spec);
    while (p < end) {
        const char* comma = p;
        while (comma < end && *comma != ',') {
            ++comma;
        }
        if (parse_entry(p, comma) != 0) {
            return -1;
        }
        p = (comma < end) ? comma + 1 : comma;
    }
    return 0;
}

int Port_ScriptedInputLoadFile(const char* path) {
    if (path == NULL) {
        return -1;
    }
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        diag("[tmc_sdl] scripted-input: cannot open %s: %s\n", path, strerror(errno));
        return -1;
    }
    char line[512];
    int rc = 0;
    while (fgets(line, (int)sizeof(line), f) != NULL) {
        /* Strip newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        /* Skip leading whitespace */
        char* p = line;
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
        if (*p == '\0' || *p == '#') {
            continue;
        }
        if (Port_ScriptedInputParse(p) != 0) {
            rc = -1;
            break;
        }
    }
    fclose(f);
    return rc;
}

/* ------------------------------------------------------------------------ */
/* Self-check (mirrors Port_RendererSelfCheck / Port_HeadersSelfCheck).     */
/*                                                                          */
/* Runs at startup from `src/platform/sdl/main.c` so headless CI catches   */
/* any regression in the scripted-input parser or table even when the     */
/* binary is invoked without `--press=` / `--input-script=`.               */
/* ------------------------------------------------------------------------ */
static int sc_fail(const char* msg) {
    fprintf(stderr, "[tmc_sdl] scripted-input self-check failed: %s\n", msg);
    return -1;
}

int Port_ScriptedInputSelfCheck(void) {
    /* Save and restore the live table so the self-check is side-effect-
     * free (nothing else has populated it yet at boot, but be defensive
     * for future callers). */
    ScriptedEntry saved[PORT_SCRIPTED_INPUT_MAX_ENTRIES];
    size_t saved_count = s_entry_count;
    if (saved_count > 0) {
        memcpy(saved, s_entries, saved_count * sizeof(saved[0]));
    }

    int rc = 0;
    Port_ScriptedInputReset();

    /* Per-key parse + frame-indexed lookup */
    if (Port_ScriptedInputParse("START@10+5,A@20,UP|B@30+3") != 0) {
        rc = sc_fail("parse 1");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(5) != 0) {
        rc = sc_fail("frame=5 should be empty");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(10) != PORT_SCRIPTED_KEY_START) {
        rc = sc_fail("frame=10 START");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(14) != PORT_SCRIPTED_KEY_START) {
        rc = sc_fail("frame=14 START");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(15) != 0) {
        rc = sc_fail("frame=15 empty");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(20) != PORT_SCRIPTED_KEY_A) {
        rc = sc_fail("frame=20 A");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(21) != 0) {
        rc = sc_fail("frame=21 empty");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(30) != (PORT_SCRIPTED_KEY_UP | PORT_SCRIPTED_KEY_B)) {
        rc = sc_fail("frame=30 UP|B");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(33) != 0) {
        rc = sc_fail("frame=33 empty");
        goto done;
    }

    /* Whitespace + case insensitivity */
    Port_ScriptedInputReset();
    if (Port_ScriptedInputParse("  start @ 5 + 2 ,  a | b @ 7 ") != 0) {
        rc = sc_fail("parse whitespace+case");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(5) != PORT_SCRIPTED_KEY_START) {
        rc = sc_fail("ws frame=5 START");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(7) != (PORT_SCRIPTED_KEY_A | PORT_SCRIPTED_KEY_B)) {
        rc = sc_fail("ws frame=7 A|B");
        goto done;
    }

    /* Error paths must not leave partial state behind. Suppress diagnostic
     * output during these intentional failures so CI logs stay clean. */
    s_quiet = 1;
    Port_ScriptedInputReset();
    if (Port_ScriptedInputParse("BOGUS@10") == 0) {
        s_quiet = 0;
        rc = sc_fail("expected parse error: BOGUS");
        goto done;
    }
    Port_ScriptedInputReset();
    if (Port_ScriptedInputParse("START") == 0) {
        s_quiet = 0;
        rc = sc_fail("expected parse error: missing @");
        goto done;
    }
    Port_ScriptedInputReset();
    if (Port_ScriptedInputParse("START@10+0") == 0) {
        s_quiet = 0;
        rc = sc_fail("expected parse error: zero duration");
        goto done;
    }
    Port_ScriptedInputReset();
    if (Port_ScriptedInputParse("START@abc") == 0) {
        s_quiet = 0;
        rc = sc_fail("expected parse error: bad frame");
        goto done;
    }
    Port_ScriptedInputReset();
    if (Port_ScriptedInputParse("@5") == 0) {
        s_quiet = 0;
        rc = sc_fail("expected parse error: missing key");
        goto done;
    }
    s_quiet = 0;

    /* Reset must clear table */
    Port_ScriptedInputReset();
    if (Port_ScriptedInputMaskForFrame(0) != 0) {
        rc = sc_fail("reset should empty table");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(0xFFFFFFFFu) != 0) {
        rc = sc_fail("reset should empty table @ max");
        goto done;
    }

    /* Direct add API */
    if (Port_ScriptedInputAdd(100, 5, PORT_SCRIPTED_KEY_R) != 0) {
        rc = sc_fail("Add R failed");
        goto done;
    }
    if (Port_ScriptedInputAdd(100, 5, PORT_SCRIPTED_KEY_L) != 0) {
        rc = sc_fail("Add L failed");
        goto done;
    }
    if (Port_ScriptedInputMaskForFrame(102) != (PORT_SCRIPTED_KEY_R | PORT_SCRIPTED_KEY_L)) {
        rc = sc_fail("frame=102 R|L");
        goto done;
    }
    s_quiet = 1;
    if (Port_ScriptedInputAdd(0, 0, PORT_SCRIPTED_KEY_A) == 0) {
        s_quiet = 0;
        rc = sc_fail("Add zero duration must fail");
        goto done;
    }
    if (Port_ScriptedInputAdd(0, 1, 0xF000) == 0) {
        s_quiet = 0;
        rc = sc_fail("Add bad mask must fail");
        goto done;
    }
    s_quiet = 0;

done:
    s_quiet = 0;
    /* Restore the saved table. */
    s_entry_count = saved_count;
    if (saved_count > 0) {
        memcpy(s_entries, saved, saved_count * sizeof(saved[0]));
    }
    return rc;
}
