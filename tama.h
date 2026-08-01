#pragma once

#include <input/input.h>
#include <tamalib.h>

#define TAG "TamaP1"
#define TAMA_ROM_PATH EXT_PATH("tama_p1/rom.bin")
#define TAMA_SAVE_PATH EXT_PATH("tama_p1/save.bin")
#define TAMA_SCREEN_SCALE_FACTOR 3
#define TAMA_LCD_ICON_SIZE 14
#define TAMA_LCD_ICON_MARGIN 1

typedef struct {
    FuriThread* thread;
    hal_t hal;
    uint8_t* rom;
    // 32x16 screen, perfectly represented through uint32_t
    uint32_t framebuffer[16];
    uint8_t icons;
    bool halted;
    bool fast_forward_done;
    bool buzzer_on;
    float frequency;
    uint32_t ff_ticks; // remaining catch-up emulation ticks (32768/s)
    uint32_t ff_total;
    bool turbo;
    uint8_t volume; // 0=mute, 1=low, 2=high
    bool vibrate;
} TamaApp;

typedef enum {
    EventTypeInput,
    EventTypeTick,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} TamaEvent;

extern TamaApp* g_ctx;
extern FuriMutex* g_state_mutex;

void tamagotchi_p1_hal_init(hal_t* hal);
