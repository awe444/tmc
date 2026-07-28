/* Spike 0 capture/replay tooling. See port_capture.h for the overview and
 * tools/capture/README.md for the script format. */
#include "port_capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define port_mkdir(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#define port_mkdir(p) mkdir(p, 0755)
#endif

#include "virtuappu.h"
#include "cpu/mode1.h"
#include "port_viewport.h"

/* port_bios.c */
extern void Port_RequestQuit(void);
/* port_debug_actions.c */
extern int Port_DebugAction_Warp(unsigned char area, unsigned char room,
                                 unsigned short x, unsigned short y,
                                 unsigned char layer);
extern void Port_DebugAction_GiveAllItems(void);
extern void Port_DebugAction_MaxHearts(void);
extern void Port_DebugAction_HealFull(void);
/* engine (src/subtask.c): opens a menu subtask the same way the game
 * does — used to reach pause/figurine menus without walking there. */
extern void MenuFadeIn(unsigned int state, unsigned int param);

/* ---- script model ---------------------------------------------------- */

typedef enum {
    CMD_KEYS,
    CMD_WARP,
    CMD_DUMP,
    CMD_QUIT,
    CMD_SUBTASK,
    CMD_ACTION,
} CmdType;

typedef struct {
    uint32_t frame;
    uint32_t seq; /* input-order tiebreak for same-frame commands */
    CmdType type;
    uint16_t keyMask;                    /* CMD_KEYS: GBA mask of held keys */
    unsigned char area, room, layer;     /* CMD_WARP */
    unsigned short x, y;                 /* CMD_WARP */
    unsigned int subtask, subtaskParam;  /* CMD_SUBTASK */
    char name[64];                       /* CMD_DUMP: basename; CMD_ACTION: action name */
} ScriptCmd;

static ScriptCmd* sCmds = NULL;
static size_t sCmdCount = 0;
static size_t sCmdCursor = 0;
static bool sScriptActive = false;

static char sDumpDir[512] = ".";
static bool sStatsEnabled = false;
static uint32_t sExitFrame = 0; /* 0 = no limit */
static bool sUncapped = false;
static bool sCaptureCanvas = false;

static uint32_t sFrame = 0;      /* frames presented so far */
static uint16_t sHeldMask = 0;   /* GBA key mask currently held by script */

/* Pending warp: retried each frame until Port_DebugAction_Warp accepts
 * (it refuses outside TASK_GAME), so scripts don't have to frame-guess
 * the moment gameplay becomes warpable after loading a save. */
static bool sWarpPending = false;
static ScriptCmd sPendingWarp;
static uint32_t sWarpAttempts = 0;
#define WARP_MAX_ATTEMPTS 600

/* ---- frame-time stats ------------------------------------------------ */

typedef struct {
    uint32_t* v;
    size_t n, cap;
} U32Vec;

static U32Vec sLogicNs, sPresentNs;

static void vec_push(U32Vec* vec, uint32_t x) {
    if (vec->n == vec->cap) {
        size_t ncap = vec->cap ? vec->cap * 2 : 4096;
        uint32_t* nv = (uint32_t*)realloc(vec->v, ncap * sizeof(uint32_t));
        if (!nv) {
            return; /* drop samples rather than crash */
        }
        vec->v = nv;
        vec->cap = ncap;
    }
    vec->v[vec->n++] = x;
}

static int cmp_u32(const void* a, const void* b) {
    uint32_t x = *(const uint32_t*)a, y = *(const uint32_t*)b;
    return (x > y) - (x < y);
}

static void print_stats_line(const char* label, U32Vec* vec) {
    if (vec->n == 0) {
        return;
    }
    double mean = 0.0;
    for (size_t i = 0; i < vec->n; i++) {
        mean += vec->v[i];
    }
    mean /= (double)vec->n;
    qsort(vec->v, vec->n, sizeof(uint32_t), cmp_u32);
    uint32_t p50 = vec->v[vec->n / 2];
    uint32_t p99 = vec->v[(size_t)((double)(vec->n - 1) * 0.99)];
    uint32_t mx = vec->v[vec->n - 1];
    fprintf(stderr,
            "[frame-stats] %s: n=%zu mean=%.3fms p50=%.3fms p99=%.3fms max=%.3fms\n",
            label, vec->n, mean / 1e6, p50 / 1e6, p99 / 1e6, mx / 1e6);
}

static void stats_atexit(void) {
    if (!sStatsEnabled) {
        return;
    }
    /* First frame's logic sample includes startup; drop it. */
    if (sLogicNs.n > 1) {
        memmove(sLogicNs.v, sLogicNs.v + 1, (sLogicNs.n - 1) * sizeof(uint32_t));
        sLogicNs.n--;
    }
    print_stats_line("logic  ", &sLogicNs);
    print_stats_line("present", &sPresentNs);
}

/* ---- script parsing -------------------------------------------------- */

static const struct {
    const char* name;
    uint16_t mask;
} kKeyNames[] = {
    { "A", 0x0001 },     { "B", 0x0002 },      { "SELECT", 0x0004 },
    { "START", 0x0008 }, { "RIGHT", 0x0010 },  { "LEFT", 0x0020 },
    { "UP", 0x0040 },    { "DOWN", 0x0080 },   { "R", 0x0100 },
    { "L", 0x0200 },     { "NONE", 0x0000 },
};

static bool parse_keys(const char* spec, uint16_t* outMask) {
    uint16_t mask = 0;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", spec);
    for (char* tok = strtok(buf, "+"); tok; tok = strtok(NULL, "+")) {
        bool found = false;
        for (size_t i = 0; i < sizeof(kKeyNames) / sizeof(kKeyNames[0]); i++) {
            if (strcmp(tok, kKeyNames[i].name) == 0) {
                mask |= kKeyNames[i].mask;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    *outMask = mask;
    return true;
}

static int cmp_cmd(const void* a, const void* b) {
    const ScriptCmd* x = (const ScriptCmd*)a;
    const ScriptCmd* y = (const ScriptCmd*)b;
    if (x->frame != y->frame) {
        return (x->frame > y->frame) ? 1 : -1;
    }
    return (x->seq > y->seq) - (x->seq < y->seq);
}

static bool load_script(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "[capture] cannot open script '%s'\n", path);
        return false;
    }
    char line[256];
    size_t cap = 0;
    uint32_t seq = 0;
    int lineNo = 0;
    while (fgets(line, sizeof(line), f)) {
        lineNo++;
        char* p = line;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '#' || *p == '\n' || *p == '\0' || *p == '\r') {
            continue;
        }
        ScriptCmd c;
        memset(&c, 0, sizeof(c));
        c.seq = seq++;
        char cmd[32], arg1[96];
        long a2, a3, a4, a5;
        if (sscanf(p, "%u %31s", &c.frame, cmd) != 2) {
            fprintf(stderr, "[capture] %s:%d: unparseable line\n", path, lineNo);
            fclose(f);
            return false;
        }
        if (strcmp(cmd, "keys") == 0) {
            if (sscanf(p, "%*u %*s %95s", arg1) != 1 || !parse_keys(arg1, &c.keyMask)) {
                fprintf(stderr, "[capture] %s:%d: bad keys spec\n", path, lineNo);
                fclose(f);
                return false;
            }
            c.type = CMD_KEYS;
        } else if (strcmp(cmd, "warp") == 0) {
            char sa[32], sr[32], sx[32], sy[32], sl[32];
            if (sscanf(p, "%*u %*s %31s %31s %31s %31s %31s", sa, sr, sx, sy, sl) != 5) {
                fprintf(stderr, "[capture] %s:%d: warp needs area room x y layer\n", path, lineNo);
                fclose(f);
                return false;
            }
            a2 = strtol(sa, NULL, 0);
            a3 = strtol(sr, NULL, 0);
            a4 = strtol(sx, NULL, 0);
            a5 = strtol(sy, NULL, 0);
            c.area = (unsigned char)a2;
            c.room = (unsigned char)a3;
            c.x = (unsigned short)a4;
            c.y = (unsigned short)a5;
            c.layer = (unsigned char)strtol(sl, NULL, 0);
            c.type = CMD_WARP;
        } else if (strcmp(cmd, "dump") == 0) {
            if (sscanf(p, "%*u %*s %63s", c.name) != 1) {
                fprintf(stderr, "[capture] %s:%d: dump needs a name\n", path, lineNo);
                fclose(f);
                return false;
            }
            c.type = CMD_DUMP;
        } else if (strcmp(cmd, "quit") == 0) {
            c.type = CMD_QUIT;
        } else if (strcmp(cmd, "subtask") == 0) {
            char ss[32], sp[32];
            if (sscanf(p, "%*u %*s %31s %31s", ss, sp) != 2) {
                fprintf(stderr, "[capture] %s:%d: subtask needs state param\n", path, lineNo);
                fclose(f);
                return false;
            }
            c.subtask = (unsigned int)strtol(ss, NULL, 0);
            c.subtaskParam = (unsigned int)strtol(sp, NULL, 0);
            c.type = CMD_SUBTASK;
        } else if (strcmp(cmd, "action") == 0) {
            if (sscanf(p, "%*u %*s %63s", c.name) != 1) {
                fprintf(stderr, "[capture] %s:%d: action needs a name\n", path, lineNo);
                fclose(f);
                return false;
            }
            c.type = CMD_ACTION;
        } else {
            fprintf(stderr, "[capture] %s:%d: unknown command '%s'\n", path, lineNo, cmd);
            fclose(f);
            return false;
        }
        if (sCmdCount == cap) {
            cap = cap ? cap * 2 : 64;
            ScriptCmd* n = (ScriptCmd*)realloc(sCmds, cap * sizeof(ScriptCmd));
            if (!n) {
                fclose(f);
                return false;
            }
            sCmds = n;
        }
        sCmds[sCmdCount++] = c;
    }
    fclose(f);
    qsort(sCmds, sCmdCount, sizeof(ScriptCmd), cmp_cmd);
    fprintf(stderr, "[capture] loaded %zu script commands from %s\n", sCmdCount, path);
    return true;
}

/* ---- framebuffer dump ------------------------------------------------ */

/* Source surfaces are ABGR8888 as SDL sees it: byte order in memory is
 * R,G,B,A. Written as binary PPM (P6), which the tools/capture scripts
 * convert to PNG and diff.
 *
 * `ppu` dumps the raw PPU output (240x160) — the Spike 0 reference format.
 * `canvas` dumps the composed presentation canvas (320x240, borders
 * included), which is what Spike 1 onward needs to verify centring and
 * border fill. Both are pre-upscale and pre-filter, so captures are
 * independent of window scale, xBRZ and CRT/LCD settings. */
static void dump_frame(const char* name) {
    char path[640];
    const int w = sCaptureCanvas ? PORT_VIEW_WIDTH : MODE1_GBA_WIDTH;
    const int h = sCaptureCanvas ? PORT_VIEW_HEIGHT : MODE1_GBA_HEIGHT;
    const uint32_t* src = sCaptureCanvas ? Port_Viewport_Canvas() : virtuappu_frame_buffer;

    snprintf(path, sizeof(path), "%s/%s.ppm", sDumpDir, name);
    FILE* f = fopen(path, "wb");
    if (!f) {
        port_mkdir(sDumpDir);
        f = fopen(path, "wb");
    }
    if (!f) {
        fprintf(stderr, "[capture] cannot write %s\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t px = src[(size_t)y * w + x];
            unsigned char rgb[3] = { (unsigned char)(px & 0xFF),
                                     (unsigned char)((px >> 8) & 0xFF),
                                     (unsigned char)((px >> 16) & 0xFF) };
            fwrite(rgb, 1, 3, f);
        }
    }
    fclose(f);
    fprintf(stderr, "[capture] frame %u -> %s\n", sFrame, path);
}

/* ---- public API ------------------------------------------------------ */

bool Port_Capture_HandleArg(const char* arg) {
    if (strncmp(arg, "--script=", 9) == 0) {
        if (!load_script(arg + 9)) {
            exit(1);
        }
        sScriptActive = sCmdCount > 0;
        sStatsEnabled = true;
        atexit(stats_atexit);
        return true;
    }
    if (strncmp(arg, "--dump-dir=", 11) == 0) {
        snprintf(sDumpDir, sizeof(sDumpDir), "%s", arg + 11);
        return true;
    }
    if (strcmp(arg, "--frame-stats") == 0) {
        sStatsEnabled = true;
        atexit(stats_atexit);
        return true;
    }
    if (strcmp(arg, "--capture-canvas") == 0) {
        sCaptureCanvas = true;
        return true;
    }
    if (strcmp(arg, "--uncapped") == 0) {
        sUncapped = true;
        return true;
    }
    if (strncmp(arg, "--exit-frame=", 13) == 0) {
        sExitFrame = (uint32_t)strtoul(arg + 13, NULL, 0);
        return true;
    }
    return false;
}

void Port_Capture_PrintUsage(void) {
    fprintf(stderr,
            "  --script=<file>:        Drive input from a capture script (deterministic replay).\n"
            "  --dump-dir=<dir>:       Directory for framebuffer dumps (default '.').\n"
            "  --frame-stats:          Print frame-time mean/p50/p99/max on exit.\n"
            "  --exit-frame=<n>:       Hard-quit after n frames (safety for scripted runs).\n"
            "  --uncapped:             Disable frame pacing (fast scripted/headless runs).\n"
            "  --capture-canvas:       Dump the composed 320x240 canvas instead of raw PPU output.\n");
}

bool Port_Capture_ScriptActive(void) {
    return sScriptActive;
}

bool Port_Capture_Uncapped(void) {
    return sUncapped;
}

void Port_Capture_OverrideInput(volatile uint16_t* keyinput) {
    if (!sScriptActive) {
        return;
    }
    /* GBA KEYINPUT is active-low: 1 = released. */
    *keyinput = (uint16_t)(0x03FF & ~sHeldMask);
}

void Port_Capture_OnVBlank(uint64_t logicNs, uint64_t presentNs) {
    if (sStatsEnabled) {
        vec_push(&sLogicNs, logicNs > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)logicNs);
        vec_push(&sPresentNs, presentNs > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)presentNs);
    }

    sFrame++;

    if (sWarpPending) {
        if (Port_DebugAction_Warp(sPendingWarp.area, sPendingWarp.room,
                                  sPendingWarp.x, sPendingWarp.y,
                                  sPendingWarp.layer)) {
            fprintf(stderr, "[capture] warp armed at frame %u (area 0x%02X room 0x%02X)\n",
                    sFrame, sPendingWarp.area, sPendingWarp.room);
            sWarpPending = false;
        } else if (++sWarpAttempts >= WARP_MAX_ATTEMPTS) {
            fprintf(stderr, "[capture] warp gave up after %u frames\n", sWarpAttempts);
            sWarpPending = false;
        }
    }

    while (sCmdCursor < sCmdCount && sCmds[sCmdCursor].frame <= sFrame) {
        ScriptCmd* c = &sCmds[sCmdCursor++];
        switch (c->type) {
            case CMD_KEYS:
                sHeldMask = c->keyMask;
                break;
            case CMD_WARP:
                sPendingWarp = *c;
                sWarpPending = true;
                sWarpAttempts = 0;
                break;
            case CMD_DUMP:
                dump_frame(c->name);
                break;
            case CMD_QUIT:
                Port_RequestQuit();
                break;
            case CMD_SUBTASK:
                MenuFadeIn(c->subtask, c->subtaskParam);
                fprintf(stderr, "[capture] subtask %u opened at frame %u\n", c->subtask, sFrame);
                break;
            case CMD_ACTION:
                if (strcmp(c->name, "giveallitems") == 0) {
                    Port_DebugAction_GiveAllItems();
                } else if (strcmp(c->name, "maxhearts") == 0) {
                    Port_DebugAction_MaxHearts();
                } else if (strcmp(c->name, "healfull") == 0) {
                    Port_DebugAction_HealFull();
                } else {
                    fprintf(stderr, "[capture] unknown action '%s'\n", c->name);
                }
                break;
        }
    }

    if (sExitFrame != 0 && sFrame >= sExitFrame) {
        fprintf(stderr, "[capture] exit-frame %u reached\n", sExitFrame);
        Port_RequestQuit();
    }
}
