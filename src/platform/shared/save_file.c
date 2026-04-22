/**
 * @file save_file.c
 * @brief Host-side Flash backend. Reads and writes a `tmc.sav` file
 *        (64 KiB, mirroring the on-cart Flash chip) so save data persists
 *        across runs.
 *
 * Wired into `src/eeprom.c` as of PR #6 of the roadmap: `EEPROMRead` and
 * `EEPROMWrite` short-circuit under `__PORT__` and call
 * `Port_SaveReadByte` / `Port_SaveWriteByte` directly against the buffer
 * below; each successful write triggers a `Port_SaveFlush` so changes
 * survive a process exit. The 64 KiB layout is more than enough for
 * TMC's 8 KiB EEPROM (`EEPROMConfigure(0x40)`).
 */
#include "platform/port.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT_SAVE_FILENAME "tmc.sav"

static uint8_t s_save_buf[PORT_SAVE_SIZE];
static char s_save_path[1024];
static int s_save_dirty = 0;

static void build_save_path(const char* save_dir) {
    if (save_dir == NULL || save_dir[0] == '\0') {
        snprintf(s_save_path, sizeof(s_save_path), "%s", PORT_SAVE_FILENAME);
    } else {
        /* POSIX-only path joining; Microsoft Windows builds are not
         * supported (see docs/sdl_port.md). */
        const char sep = '/';
        size_t len = strlen(save_dir);
        if (len > 0 && save_dir[len - 1] == '/') {
            snprintf(s_save_path, sizeof(s_save_path), "%s%s", save_dir, PORT_SAVE_FILENAME);
        } else {
            snprintf(s_save_path, sizeof(s_save_path), "%s%c%s", save_dir, sep, PORT_SAVE_FILENAME);
        }
    }
}

int Port_SaveLoad(const char* save_dir) {
    build_save_path(save_dir);
    /* Default to all-0xFF, which matches a freshly-erased Flash chip. */
    memset(s_save_buf, 0xFF, sizeof(s_save_buf));
    s_save_dirty = 0;

    FILE* f = fopen(s_save_path, "rb");
    if (f == NULL) {
        /* Missing file is fine on first run; we'll create it on flush. */
        return (errno == ENOENT) ? 0 : -1;
    }
    size_t read = fread(s_save_buf, 1, sizeof(s_save_buf), f);
    int had_error = ferror(f);
    int hit_eof = (read < sizeof(s_save_buf)) && feof(f);
    fclose(f);
    /* Files smaller than PORT_SAVE_SIZE are tolerated (the rest stays
     * 0xFF) so we can interoperate with emulator save formats. */
    return (had_error || (read < sizeof(s_save_buf) && !hit_eof)) ? -1 : 0;
}

int Port_SaveFlush(void) {
    if (!s_save_dirty) {
        return 0;
    }
    if (s_save_path[0] == '\0') {
        build_save_path(NULL);
    }
    FILE* f = fopen(s_save_path, "wb");
    if (f == NULL) {
        return -1;
    }
    size_t written = fwrite(s_save_buf, 1, sizeof(s_save_buf), f);
    fclose(f);
    if (written != sizeof(s_save_buf)) {
        return -1;
    }
    s_save_dirty = 0;
    return 0;
}

uint8_t Port_SaveReadByte(uint32_t offset) {
    if (offset >= PORT_SAVE_SIZE) {
        return 0xFF;
    }
    return s_save_buf[offset];
}

void Port_SaveWriteByte(uint32_t offset, uint8_t value) {
    if (offset >= PORT_SAVE_SIZE) {
        return;
    }
    if (s_save_buf[offset] != value) {
        s_save_buf[offset] = value;
        s_save_dirty = 1;
    }
}
