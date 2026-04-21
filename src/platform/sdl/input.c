/**
 * @file input.c
 * @brief Keyboard + SDL_GameController input layer.
 *
 * Why SDL_GameController and not SDL_Joystick?
 *   On Windows, SDL's GameController backend transparently uses XInput
 *   for Xbox-compatible controllers (and DirectInput / HIDAPI for the
 *   rest). Using this API gives us "X-Input gamepad support" portably
 *   and consistently with how every modern SDL game does it.
 *
 * Each frame Port_InputPump() reads the SDL event queue, builds an
 * active-high 10-bit GBA-format key bitmask (matching the layout in
 * include/gba/io_reg.h: A=0x001, B=0x002, SELECT=0x004, START=0x008,
 * RIGHT=0x010, LEFT=0x020, UP=0x040, DOWN=0x080, R=0x100, L=0x200),
 * stashes it in s_key_mask, and mirrors `~s_key_mask & 0x3FF` into the
 * emulated REG_KEYINPUT slot (gPortIo + 0x130) so the existing
 * src/common.c::ReadKeyInput() works unchanged.
 */
#include "platform/port.h"

#include <SDL.h>

#include <stdio.h>
#include <string.h>

/* Bit layout matches include/gba/io_reg.h (A_BUTTON, etc.). Repeated
 * here to keep this file independent of the GBA headers — those headers
 * pull in agbcc-isms that we don't want in the platform layer. */
enum {
    PORT_KEY_A = 0x0001,
    PORT_KEY_B = 0x0002,
    PORT_KEY_SELECT = 0x0004,
    PORT_KEY_START = 0x0008,
    PORT_KEY_RIGHT = 0x0010,
    PORT_KEY_LEFT = 0x0020,
    PORT_KEY_UP = 0x0040,
    PORT_KEY_DOWN = 0x0080,
    PORT_KEY_R = 0x0100,
    PORT_KEY_L = 0x0200,
    PORT_KEYS_MASK = 0x03FF,
};

/* Offset of REG_KEYINPUT inside the emulated I/O register block (gPortIo).
 * Mirrors REG_OFFSET_KEYINPUT in include/gba/io_reg.h. Repeated here so
 * input.c stays free of the GBA headers (and their agbcc-isms).
 *
 * Note: src/platform/shared/port_headers_check.c validates the expected
 * KEYINPUT offset value separately, but it does not reference this local
 * PORT_REG_OFFSET_KEYINPUT macro directly. */
#define PORT_REG_OFFSET_KEYINPUT 0x130

static void write_keyinput_reg(uint16_t active_high_mask) {
    /* REG_KEYINPUT is active-low on real hardware: 1 = released,
     * 0 = pressed. Write the inverted mask into the slot that the
     * rewired REG_KEYINPUT macro aliases (gPortIo + 0x130) so the
     * existing src/common.c::ReadKeyInput() code path works unchanged. */
    uint16_t reg_value = (uint16_t)(~active_high_mask & PORT_KEYS_MASK);
    uint8_t* slot = gPortIo + PORT_REG_OFFSET_KEYINPUT;
    slot[0] = (uint8_t)(reg_value & 0xFF);
    slot[1] = (uint8_t)((reg_value >> 8) & 0xFF);
}

/* Analog-stick deadzone as a fraction of SDL's [-32768, 32767] range. */
#define PORT_STICK_DEADZONE 8000

static SDL_GameController* s_gamepad = NULL;
static SDL_JoystickID s_gamepad_id = -1;
static uint16_t s_key_mask = 0;
static uint16_t s_keyboard_mask = 0;

/* ------------------------------------------------------------------------ */
/* Keyboard mapping.                                                         */
/* ------------------------------------------------------------------------ */
static uint16_t key_for_scancode(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_X:
            return PORT_KEY_A;
        case SDL_SCANCODE_Z:
            return PORT_KEY_B;
        case SDL_SCANCODE_RSHIFT:
        case SDL_SCANCODE_BACKSPACE:
            return PORT_KEY_SELECT;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_RETURN2:
            return PORT_KEY_START;
        case SDL_SCANCODE_RIGHT:
            return PORT_KEY_RIGHT;
        case SDL_SCANCODE_LEFT:
            return PORT_KEY_LEFT;
        case SDL_SCANCODE_UP:
            return PORT_KEY_UP;
        case SDL_SCANCODE_DOWN:
            return PORT_KEY_DOWN;
        case SDL_SCANCODE_S:
            return PORT_KEY_R;
        case SDL_SCANCODE_A:
            return PORT_KEY_L;
        default:
            return 0;
    }
}

/* ------------------------------------------------------------------------ */
/* Gamepad helpers.                                                          */
/* ------------------------------------------------------------------------ */
#if TMC_ENABLE_GAMEPAD
static void open_first_controller(void) {
    if (s_gamepad != NULL) {
        return;
    }
    int n = SDL_NumJoysticks();
    for (int i = 0; i < n; ++i) {
        if (SDL_IsGameController(i)) {
            s_gamepad = SDL_GameControllerOpen(i);
            if (s_gamepad != NULL) {
                SDL_Joystick* joy = SDL_GameControllerGetJoystick(s_gamepad);
                s_gamepad_id = joy ? SDL_JoystickInstanceID(joy) : -1;
                fprintf(stderr, "[tmc_sdl] Gamepad opened: %s\n", SDL_GameControllerName(s_gamepad));
                return;
            }
        }
    }
}

static void close_controller(void) {
    if (s_gamepad != NULL) {
        SDL_GameControllerClose(s_gamepad);
        s_gamepad = NULL;
        s_gamepad_id = -1;
    }
}

static uint16_t poll_gamepad_mask(void) {
    if (s_gamepad == NULL || !SDL_GameControllerGetAttached(s_gamepad)) {
        return 0;
    }

    uint16_t mask = 0;

    /* Face buttons: GBA A maps to gamepad A (Xbox A / Nintendo B), GBA B
     * maps to gamepad B (Xbox B / Nintendo A). */
    if (SDL_GameControllerGetButton(s_gamepad, SDL_CONTROLLER_BUTTON_A))
        mask |= PORT_KEY_A;
    if (SDL_GameControllerGetButton(s_gamepad, SDL_CONTROLLER_BUTTON_B))
        mask |= PORT_KEY_B;

    if (SDL_GameControllerGetButton(s_gamepad, SDL_CONTROLLER_BUTTON_BACK))
        mask |= PORT_KEY_SELECT;
    if (SDL_GameControllerGetButton(s_gamepad, SDL_CONTROLLER_BUTTON_START))
        mask |= PORT_KEY_START;

    if (SDL_GameControllerGetButton(s_gamepad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
        mask |= PORT_KEY_L;
    if (SDL_GameControllerGetButton(s_gamepad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
        mask |= PORT_KEY_R;

    /* Triggers double as L/R for users without bumpers. */
    if (SDL_GameControllerGetAxis(s_gamepad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16384)
        mask |= PORT_KEY_L;
    if (SDL_GameControllerGetAxis(s_gamepad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16384)
        mask |= PORT_KEY_R;

    /* D-pad. */
    if (SDL_GameControllerGetButton(s_gamepad, SDL_CONTROLLER_BUTTON_DPAD_UP))
        mask |= PORT_KEY_UP;
    if (SDL_GameControllerGetButton(s_gamepad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
        mask |= PORT_KEY_DOWN;
    if (SDL_GameControllerGetButton(s_gamepad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
        mask |= PORT_KEY_LEFT;
    if (SDL_GameControllerGetButton(s_gamepad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
        mask |= PORT_KEY_RIGHT;

    /* Left analog stick → digital D-pad with deadzone. */
    Sint16 ax = SDL_GameControllerGetAxis(s_gamepad, SDL_CONTROLLER_AXIS_LEFTX);
    Sint16 ay = SDL_GameControllerGetAxis(s_gamepad, SDL_CONTROLLER_AXIS_LEFTY);
    if (ax > PORT_STICK_DEADZONE)
        mask |= PORT_KEY_RIGHT;
    if (ax < -PORT_STICK_DEADZONE)
        mask |= PORT_KEY_LEFT;
    if (ay > PORT_STICK_DEADZONE)
        mask |= PORT_KEY_DOWN;
    if (ay < -PORT_STICK_DEADZONE)
        mask |= PORT_KEY_UP;

    return mask;
}
#else
static void open_first_controller(void) {
}
static void close_controller(void) {
}
static uint16_t poll_gamepad_mask(void) {
    return 0;
}
#endif /* TMC_ENABLE_GAMEPAD */

/* ------------------------------------------------------------------------ */
/* Public API.                                                               */
/* ------------------------------------------------------------------------ */
int Port_InputInit(void) {
#if TMC_ENABLE_GAMEPAD
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "[tmc_sdl] SDL_INIT_GAMECONTROLLER failed: %s\n", SDL_GetError());
        /* Non-fatal — keyboard still works. */
    } else {
        /* Optional mappings file shipped next to the binary. */
        const char* db = "gamecontrollerdb.txt";
        if (SDL_GameControllerAddMappingsFromFile(db) > 0) {
            fprintf(stderr, "[tmc_sdl] Loaded controller mappings from %s\n", db);
        }
        open_first_controller();
    }
#endif
    s_key_mask = 0;
    s_keyboard_mask = 0;
    /* Prime the emulated REG_KEYINPUT slot to "no keys pressed" before any
     * game code runs. ReadKeyInput() may sample the register before the
     * first Port_InputPump() — for example, src/main.c::AgbMain runs early
     * init before entering the frame loop — and on real hardware the
     * register defaults to 0x3FF (all keys released, active-low). */
    write_keyinput_reg(0);
    return 0;
}

void Port_InputShutdown(void) {
#if TMC_ENABLE_GAMEPAD
    close_controller();
#endif
}

void Port_InputPump(void) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT:
                Port_RequestQuit();
                break;

            case SDL_KEYDOWN:
                if (!ev.key.repeat) {
                    if (ev.key.keysym.sym == SDLK_ESCAPE) {
                        Port_RequestQuit();
                    } else {
                        s_keyboard_mask |= key_for_scancode(ev.key.keysym.scancode);
                    }
                }
                break;

            case SDL_KEYUP:
                s_keyboard_mask &= (uint16_t)~key_for_scancode(ev.key.keysym.scancode);
                break;

#if TMC_ENABLE_GAMEPAD
            case SDL_CONTROLLERDEVICEADDED:
                open_first_controller();
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                if (s_gamepad != NULL && ev.cdevice.which == s_gamepad_id) {
                    close_controller();
                    /* Try to fall back to whichever controller is still
                     * plugged in, if any. */
                    open_first_controller();
                }
                break;
#endif

            default:
                break;
        }
    }

    s_key_mask = (uint16_t)((s_keyboard_mask | poll_gamepad_mask()) & PORT_KEYS_MASK);

    /* Mirror the active-low view of s_key_mask into the emulated
     * REG_KEYINPUT slot (gPortIo + 0x130). The macro REG_KEYINPUT in
     * include/gba/io_reg.h is rewired to that same address under
     * __PORT__ (see PR #2a in docs/sdl_port.md), so the existing
     * src/common.c::ReadKeyInput() works unchanged on the host. */
    write_keyinput_reg(s_key_mask);
}

uint16_t Port_GetKeyMask(void) {
    return s_key_mask;
}
