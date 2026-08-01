#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <stm32wbxx_ll_tim.h>
#include <tamalib.h>
#include "tama.h"
#include "tamagotchi_p1_icons.h"

TamaApp* g_ctx;
FuriMutex* g_state_mutex;

#define TAMA_SAVE_MAGIC 0x50314D54u // "TM1P"
#define TAMA_SAVE_VERSION 2u
#define TAMA_V1_TAIL_SIZE 8u // v2 appended fields not present in v1 saves
#define TAMA_TICK_FREQ 32768u
#define TAMA_CATCHUP_MAX_SECONDS (6u * 60u * 60u)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint16_t pc;
    uint16_t x;
    uint16_t y;
    uint8_t a;
    uint8_t b;
    uint8_t np;
    uint8_t sp;
    uint8_t flags;
    uint32_t tick_counter;
    uint32_t clk_timer_timestamp;
    uint32_t prog_timer_timestamp;
    uint8_t prog_timer_enabled;
    uint8_t prog_timer_data;
    uint8_t prog_timer_rld;
    uint32_t call_depth;
    uint8_t int_factor_flag[INT_SLOT_NUM];
    uint8_t int_mask[INT_SLOT_NUM];
    uint8_t int_triggered[INT_SLOT_NUM];
    uint8_t int_vector[INT_SLOT_NUM];
    MEM_BUFFER_TYPE memory[MEM_BUFFER_SIZE];
    // v2 fields (must stay at the end; TAMA_V1_TAIL_SIZE covers them)
    uint32_t rtc_epoch;
    uint8_t volume;
    uint8_t vibrate;
    uint8_t reserved[2];
} TamaSaveState;

static void tamagotchi_p1_save_state(void) {
    state_t* s = tamalib_get_state();
    TamaSaveState* save = malloc(sizeof(TamaSaveState));
    save->magic = TAMA_SAVE_MAGIC;
    save->version = TAMA_SAVE_VERSION;
    save->pc = *s->pc;
    save->x = *s->x;
    save->y = *s->y;
    save->a = *s->a;
    save->b = *s->b;
    save->np = *s->np;
    save->sp = *s->sp;
    save->flags = *s->flags;
    save->tick_counter = *s->tick_counter;
    save->clk_timer_timestamp = *s->clk_timer_timestamp;
    save->prog_timer_timestamp = *s->prog_timer_timestamp;
    save->prog_timer_enabled = *s->prog_timer_enabled;
    save->prog_timer_data = *s->prog_timer_data;
    save->prog_timer_rld = *s->prog_timer_rld;
    save->call_depth = *s->call_depth;
    for(size_t i = 0; i < INT_SLOT_NUM; i++) {
        save->int_factor_flag[i] = s->interrupts[i].factor_flag_reg;
        save->int_mask[i] = s->interrupts[i].mask_reg;
        save->int_triggered[i] = s->interrupts[i].triggered;
        save->int_vector[i] = s->interrupts[i].vector;
    }
    memcpy(save->memory, s->memory, sizeof(save->memory));
    save->rtc_epoch = furi_hal_rtc_get_timestamp();
    save->volume = g_ctx->volume;
    save->vibrate = g_ctx->vibrate ? 1 : 0;
    save->reserved[0] = 0;
    save->reserved[1] = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, TAMA_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint16_t written = storage_file_write(file, save, sizeof(TamaSaveState));
        FURI_LOG_I(TAG, "Saved state: %u bytes", written);
    } else {
        FURI_LOG_E(TAG, "Failed to open save file for writing");
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    free(save);
}

static void tamagotchi_p1_load_state(void) {
    TamaSaveState* save = malloc(sizeof(TamaSaveState));
    bool loaded = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool is_v2 = false;
    if(storage_file_open(file, TAMA_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint16_t bytes = storage_file_read(file, save, sizeof(TamaSaveState));
        if(save->magic == TAMA_SAVE_MAGIC) {
            if(save->version == 2 && bytes == sizeof(TamaSaveState)) {
                loaded = true;
                is_v2 = true;
            } else if(save->version == 1 && bytes == sizeof(TamaSaveState) - TAMA_V1_TAIL_SIZE) {
                loaded = true;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(loaded) {
        state_t* s = tamalib_get_state();
        *s->pc = save->pc;
        *s->x = save->x;
        *s->y = save->y;
        *s->a = save->a;
        *s->b = save->b;
        *s->np = save->np;
        *s->sp = save->sp;
        *s->flags = save->flags;
        *s->tick_counter = save->tick_counter;
        *s->clk_timer_timestamp = save->clk_timer_timestamp;
        *s->prog_timer_timestamp = save->prog_timer_timestamp;
        *s->prog_timer_enabled = save->prog_timer_enabled;
        *s->prog_timer_data = save->prog_timer_data;
        *s->prog_timer_rld = save->prog_timer_rld;
        *s->call_depth = save->call_depth;
        for(size_t i = 0; i < INT_SLOT_NUM; i++) {
            s->interrupts[i].factor_flag_reg = save->int_factor_flag[i];
            s->interrupts[i].mask_reg = save->int_mask[i];
            s->interrupts[i].triggered = save->int_triggered[i];
            s->interrupts[i].vector = save->int_vector[i];
        }
        memcpy(s->memory, save->memory, sizeof(save->memory));
        cpu_sync_ref_timestamp();
        cpu_refresh_hw();
        FURI_LOG_I(TAG, "Loaded saved state");

        if(is_v2) {
            g_ctx->volume = (save->volume <= 2) ? save->volume : 2;
            g_ctx->vibrate = save->vibrate != 0;

            // Real-time catch-up: emulate the time that passed while closed
            uint32_t now = furi_hal_rtc_get_timestamp();
            if(now > save->rtc_epoch) {
                uint32_t elapsed = now - save->rtc_epoch;
                if(elapsed > TAMA_CATCHUP_MAX_SECONDS) elapsed = TAMA_CATCHUP_MAX_SECONDS;
                if(elapsed > 5) {
                    g_ctx->ff_ticks = elapsed * TAMA_TICK_FREQ;
                    g_ctx->ff_total = g_ctx->ff_ticks;
                    tamalib_set_speed(0);
                    FURI_LOG_I(TAG, "Catching up %lu seconds", elapsed);
                }
            }
        }
    } else {
        FURI_LOG_I(TAG, "No valid save found, starting fresh");
    }
    free(save);
}

static const Icon* icons_list[] = {
    &I_icon_0,
    &I_icon_1,
    &I_icon_2,
    &I_icon_3,
    &I_icon_4,
    &I_icon_5,
    &I_icon_6,
    &I_icon_7,
};

static void tamagotchi_p1_draw_callback(Canvas* const canvas, void* cb_ctx) {
    furi_assert(cb_ctx);

    FuriMutex* const mutex = cb_ctx;
    if(furi_mutex_acquire(mutex, 25) != FuriStatusOk) return;

    if(g_ctx->rom == NULL) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 30, 30, "No ROM");
    } else if(g_ctx->halted) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 30, 30, "Halted");
    } else {
        // 3x scale layout: 96x48 LCD at left, icons in two columns at right,
        // status line in the strip under the LCD
        uint16_t lcd_matrix_scaled_height = 16 * TAMA_SCREEN_SCALE_FACTOR;
        uint16_t lcd_matrix_top = 0;
        uint16_t lcd_matrix_left = 0;

        uint16_t y = lcd_matrix_top;
        for(uint8_t row = 0; row < 16; ++row) {
            uint16_t x = lcd_matrix_left;
            uint32_t row_pixels = g_ctx->framebuffer[row];
            for(uint8_t col = 0; col < 32; ++col) {
                if(row_pixels & 1) {
                    canvas_draw_box(
                        canvas, x, y, TAMA_SCREEN_SCALE_FACTOR, TAMA_SCREEN_SCALE_FACTOR);
                }
                x += TAMA_SCREEN_SCALE_FACTOR;
                row_pixels >>= 1;
            }
            y += TAMA_SCREEN_SCALE_FACTOR;
        }

        // Draw icons: two vertical columns to the right of the LCD
        uint8_t lcd_icons = g_ctx->icons;
        uint16_t icon_col1 = 32 * TAMA_SCREEN_SCALE_FACTOR + 2;
        uint16_t icon_col2 = icon_col1 + TAMA_LCD_ICON_SIZE + 2;
        for(uint8_t i = 0; i < 8; ++i) {
            if(lcd_icons & 1) {
                uint16_t x_ic = (i < 4) ? icon_col1 : icon_col2;
                uint16_t y_ic = 1 + (i % 4) * (TAMA_LCD_ICON_SIZE + 2);
                canvas_draw_icon(canvas, x_ic, y_ic, icons_list[i]);
            }
            lcd_icons >>= 1;
        }

        // Status line under the LCD
        canvas_set_font(canvas, FontSecondary);
        char status[32];
        if(g_ctx->ff_ticks) {
            uint8_t pct =
                100 - (uint8_t)((uint64_t)g_ctx->ff_ticks * 100 /
                                (g_ctx->ff_total ? g_ctx->ff_total : 1));
            snprintf(status, sizeof(status), "Catching up... %u%%", pct);
        } else {
            snprintf(
                status,
                sizeof(status),
                "%s%s%s",
                g_ctx->turbo ? ">> " : "",
                (g_ctx->volume == 2) ? "Vol:Hi" : ((g_ctx->volume == 1) ? "Vol:Lo" : "Mute"),
                g_ctx->vibrate ? " +Vib" : "");
        }
        canvas_draw_str(canvas, 1, lcd_matrix_top + lcd_matrix_scaled_height + 10, status);
    }

    furi_mutex_release(mutex);
}

static void tamagotchi_p1_input_callback(InputEvent* input_event, void* context) {
    furi_assert(context);
    FuriMessageQueue* event_queue = context;

    TamaEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void tamagotchi_p1_update_timer_callback(void* context) {
    furi_assert(context);
    FuriMessageQueue* event_queue = context;

    TamaEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

static int32_t tamagotchi_p1_worker(void* context) {
    bool running = true;
    FuriMutex* mutex = context;
    while(furi_mutex_acquire(mutex, FuriWaitForever) != FuriStatusOk) furi_delay_tick(1);

    cpu_sync_ref_timestamp();
    LL_TIM_EnableCounter(TIM2);
    state_t* s = tamalib_get_state();
    uint32_t last_tc = *s->tick_counter;
    uint32_t yield_counter = 0;
    while(running) {
        if(furi_thread_flags_get()) {
            running = false;
        } else {
            tamalib_step();
            uint32_t tc = *s->tick_counter;
            if(g_ctx->ff_ticks) {
                uint32_t delta = tc - last_tc;
                if(delta >= g_ctx->ff_ticks) {
                    g_ctx->ff_ticks = 0;
                    tamalib_set_speed(g_ctx->turbo ? 0 : 1);
                    cpu_sync_ref_timestamp();
                } else {
                    g_ctx->ff_ticks -= delta;
                }
            }
            last_tc = tc;

            // At max speed (turbo/catch-up) wait_for_cycles never sleeps, so the
            // mutex is never released and the GUI/input starve. Yield regularly.
            if(++yield_counter >= 512) {
                yield_counter = 0;
                furi_mutex_release(mutex);
                furi_delay_tick(1);
                while(furi_mutex_acquire(mutex, FuriWaitForever) != FuriStatusOk)
                    furi_delay_tick(1);
            }
        }
    }
    LL_TIM_DisableCounter(TIM2);
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
    furi_hal_vibro_on(false);
    furi_mutex_release(mutex);
    return 0;
}

static void tamagotchi_p1_init(TamaApp* const ctx) {
    g_ctx = ctx;
    memset(ctx, 0, sizeof(TamaApp));
    ctx->volume = 2;
    tamagotchi_p1_hal_init(&ctx->hal);

    // Load ROM
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo fi;
    if(storage_common_stat(storage, TAMA_ROM_PATH, &fi) == FSE_OK) {
        File* rom_file = storage_file_alloc(storage);
        if(storage_file_open(rom_file, TAMA_ROM_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            ctx->rom = malloc((size_t)fi.size);
            uint8_t* buf_ptr = ctx->rom;
            size_t read = 0;
            while(read < fi.size) {
                size_t to_read = fi.size - read;
                if(to_read > UINT16_MAX) to_read = UINT16_MAX;
                uint16_t now_read = storage_file_read(rom_file, buf_ptr, (uint16_t)to_read);
                read += now_read;
                buf_ptr += now_read;
            }

            // Reorder endianess of ROM
            for(size_t i = 0; i < fi.size; i += 2) {
                uint8_t b = ctx->rom[i];
                ctx->rom[i] = ctx->rom[i + 1];
                ctx->rom[i + 1] = b & 0xF;
            }
        }

        storage_file_close(rom_file);
        storage_file_free(rom_file);
    }
    furi_record_close(RECORD_STORAGE);

    if(ctx->rom != NULL) {
        // Init TIM2
        // 64KHz
        if(!furi_hal_bus_is_enabled(FuriHalBusTIM2)) {
            furi_hal_bus_enable(FuriHalBusTIM2);
        }
        LL_TIM_InitTypeDef tim_init = {
            .Prescaler = 999,
            .CounterMode = LL_TIM_COUNTERMODE_UP,
            .Autoreload = 0xFFFFFFFF,
        };
        LL_TIM_Init(TIM2, &tim_init);
        LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL);
        LL_TIM_DisableCounter(TIM2);
        LL_TIM_SetCounter(TIM2, 0);

        // Init TamaLIB
        tamalib_register_hal(&ctx->hal);
        tamalib_init((u12_t*)ctx->rom, NULL, 64000);
        tamalib_set_speed(1);

        // Restore saved state if present
        tamagotchi_p1_load_state();

        // TODO: implement fast forwarding
        ctx->fast_forward_done = true;

        // Start stepping thread
        ctx->thread = furi_thread_alloc();
        furi_thread_set_name(ctx->thread, "TamaLIB");
        furi_thread_set_stack_size(ctx->thread, 4096);
        furi_thread_set_callback(ctx->thread, tamagotchi_p1_worker);
        furi_thread_set_context(ctx->thread, g_state_mutex);
        furi_thread_start(ctx->thread);
    }
}

static void tamagotchi_p1_deinit(TamaApp* const ctx) {
    if(ctx->rom != NULL) {
        tamalib_release();
        if(furi_hal_bus_is_enabled(FuriHalBusTIM2)) {
            furi_hal_bus_disable(FuriHalBusTIM2);
        }
        furi_thread_free(ctx->thread);
        free(ctx->rom);
    }
}

int32_t tamagotchi_p1_app(void* p) {
    UNUSED(p);

    TamaApp* ctx = malloc(sizeof(TamaApp));
    g_state_mutex = furi_mutex_alloc(FuriMutexTypeRecursive);
    tamagotchi_p1_init(ctx);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(TamaEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, tamagotchi_p1_draw_callback, g_state_mutex);
    view_port_input_callback_set(view_port, tamagotchi_p1_input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    FuriTimer* timer =
        furi_timer_alloc(tamagotchi_p1_update_timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 30);

    uint32_t autosave_counter = 0;
    for(bool running = true; running;) {
        TamaEvent event;
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, FuriWaitForever);
        if(event_status == FuriStatusOk) {
            // Local override with acquired context
            if(furi_mutex_acquire(g_state_mutex, FuriWaitForever) != FuriStatusOk) continue;

            if(event.type == EventTypeTick) {
                view_port_update(view_port);
                // Periodic autosave every ~2 min (3600 ticks at 30/s)
                if(ctx->rom != NULL && ctx->ff_ticks == 0 && ++autosave_counter >= 3600) {
                    autosave_counter = 0;
                    tamagotchi_p1_save_state();
                }
            } else if(event.type == EventTypeInput) {
                FURI_LOG_D(
                    TAG,
                    "EventTypeInput: %lu %d %d",
                    event.input.sequence,
                    event.input.key,
                    event.input.type);
                InputType input_type = event.input.type;
                if(input_type == InputTypePress || input_type == InputTypeRelease) {
                    btn_state_t tama_btn_state = 0;
                    if(input_type == InputTypePress)
                        tama_btn_state = BTN_STATE_PRESSED;
                    else if(input_type == InputTypeRelease)
                        tama_btn_state = BTN_STATE_RELEASED;

                    if(event.input.key == InputKeyLeft) {
                        tamalib_set_button(BTN_LEFT, tama_btn_state);
                    } else if(event.input.key == InputKeyOk) {
                        tamalib_set_button(BTN_MIDDLE, tama_btn_state);
                    } else if(event.input.key == InputKeyRight) {
                        tamalib_set_button(BTN_RIGHT, tama_btn_state);
                    }
                }

                if(event.input.key == InputKeyUp && event.input.type == InputTypeShort) {
                    // Toggle turbo
                    ctx->turbo = !ctx->turbo;
                    if(ctx->ff_ticks == 0) {
                        tamalib_set_speed(ctx->turbo ? 0 : 1);
                        cpu_sync_ref_timestamp();
                    }
                } else if(event.input.key == InputKeyUp && event.input.type == InputTypeLong) {
                    // Toggle vibration
                    ctx->vibrate = !ctx->vibrate;
                    if(!ctx->vibrate) furi_hal_vibro_on(false);
                } else if(event.input.key == InputKeyDown && event.input.type == InputTypeShort) {
                    // Cycle volume: high -> low -> mute
                    ctx->volume = (ctx->volume + 2) % 3;
                } else if(event.input.key == InputKeyDown && event.input.type == InputTypeLong) {
                    // Reset pet: fresh egg, delete save
                    ctx->ff_ticks = 0;
                    ctx->turbo = false;
                    tamalib_set_speed(1);
                    cpu_reset();
                    Storage* storage = furi_record_open(RECORD_STORAGE);
                    storage_common_remove(storage, TAMA_SAVE_PATH);
                    furi_record_close(RECORD_STORAGE);
                    FURI_LOG_I(TAG, "Pet reset");
                }

                if(event.input.key == InputKeyBack && event.input.type == InputTypeLong) {
                    furi_timer_stop(timer);
                    running = false;
                }
            }

            furi_mutex_release(g_state_mutex);
        } else {
            // Timeout
            // FURI_LOG_D(TAG, "Timed out");
        }
    }

    if(ctx->rom != NULL) {
        furi_thread_flags_set(furi_thread_get_id(ctx->thread), 1);
        furi_thread_join(ctx->thread);
        tamagotchi_p1_save_state();
    }

    furi_timer_free(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(g_state_mutex);
    tamagotchi_p1_deinit(ctx);
    free(ctx);

    return 0;
}
