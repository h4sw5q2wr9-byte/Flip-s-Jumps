/*
 * Flip's Jumps - application entry point, input handling and persistence.
 */
#include "flips_jumps.h"

#include <dolphin/dolphin.h>
#include <storage/storage.h>

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t high_score;
    uint8_t sound_on;
} FlipsJumpsSave;

/* ------------------------------------------------------------------ sound */

void sound_stop(FlipsJumpsApp* app) {
    app->sound_ticks = 0;
    if(app->speaker_acquired) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        app->speaker_acquired = false;
    }
}

void sound_play(FlipsJumpsApp* app, float frequency, uint8_t ticks) {
    if(!app->sound_on) return;

    if(!app->speaker_acquired) {
        if(!furi_hal_speaker_acquire(10)) return;
        app->speaker_acquired = true;
    }
    furi_hal_speaker_start(frequency, 0.4f);
    app->sound_ticks = ticks;
}

static void sound_update(FlipsJumpsApp* app) {
    if(app->sound_ticks > 0 && --app->sound_ticks == 0) {
        sound_stop(app);
    }
}

/* ------------------------------------------------------------------- save */

void flips_jumps_save(FlipsJumpsApp* app) {
    FlipsJumpsSave save = {
        .magic = FLIPS_JUMPS_SAVE_MAGIC,
        .version = 1,
        .high_score = app->world.high_score,
        .sound_on = app->sound_on ? 1 : 0,
    };

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, FLIPS_JUMPS_SAVE_FOLDER);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FLIPS_JUMPS_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &save, sizeof(save));
    }
    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}

static void flips_jumps_load(FlipsJumpsApp* app) {
    FlipsJumpsSave save = {0};

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, FLIPS_JUMPS_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, &save, sizeof(save)) == sizeof(save) &&
           save.magic == FLIPS_JUMPS_SAVE_MAGIC) {
            app->world.high_score = save.high_score;
            app->sound_on = save.sound_on != 0;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* --------------------------------------------------------------- callbacks */

static void flips_jumps_draw_callback(Canvas* canvas, void* context) {
    FlipsJumpsApp* app = context;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    game_draw(canvas, app);
    furi_mutex_release(app->mutex);
}

static void flips_jumps_input_callback(InputEvent* input_event, void* context) {
    FlipsJumpsApp* app = context;
    GameEvent event = {.type = GameEventTypeInput, .input = *input_event};

    furi_message_queue_put(app->queue, &event, FuriWaitForever);
}

static void flips_jumps_timer_callback(void* context) {
    FlipsJumpsApp* app = context;
    GameEvent event = {.type = GameEventTypeTick};

    /* Never block the timer thread: a dropped frame beats a stalled kernel. */
    furi_message_queue_put(app->queue, &event, 0);
}

/* ------------------------------------------------------------------ input */

static void game_start(FlipsJumpsApp* app) {
    dolphin_deed(DolphinDeedPluginGameStart);
    game_reset(&app->world);
    app->world.input_dir = 0;
    app->world.left_held = false;
    app->world.right_held = false;
}

static void movement_update(GameWorld* world) {
    if(world->left_held && !world->right_held) {
        world->input_dir = -1;
    } else if(world->right_held && !world->left_held) {
        world->input_dir = 1;
    } else {
        world->input_dir = 0;
    }
}

static void flips_jumps_handle_input(FlipsJumpsApp* app, InputEvent* input) {
    GameWorld* world = &app->world;

    /* Holding Back always leaves, whatever we are doing. */
    if(input->key == InputKeyBack && input->type == InputTypeLong) {
        app->running = false;
        return;
    }

    if(input->key == InputKeyLeft || input->key == InputKeyRight) {
        bool held = (input->type == InputTypePress) || (input->type == InputTypeRepeat);
        if(input->type == InputTypeRelease) held = false;

        if(input->type == InputTypePress || input->type == InputTypeRepeat ||
           input->type == InputTypeRelease) {
            if(input->key == InputKeyLeft) {
                world->left_held = held;
            } else {
                world->right_held = held;
            }
            movement_update(world);
        }
        return;
    }

    if(input->type != InputTypeShort) return;

    switch(world->state) {
    case GameStateMenu:
        if(input->key == InputKeyOk) {
            game_start(app);
        } else if(input->key == InputKeyUp) {
            app->sound_on = !app->sound_on;
            if(!app->sound_on) sound_stop(app);
            flips_jumps_save(app);
        } else if(input->key == InputKeyBack) {
            app->running = false;
        }
        break;

    case GameStatePlaying:
        if(input->key == InputKeyBack || input->key == InputKeyOk) {
            world->state = GameStatePaused;
            /* Drop any held direction so resuming does not drift. */
            world->left_held = false;
            world->right_held = false;
            world->input_dir = 0;
            sound_stop(app);
        }
        break;

    case GameStatePaused:
        if(input->key == InputKeyOk) {
            world->state = GameStatePlaying;
        } else if(input->key == InputKeyBack) {
            world->state = GameStateMenu;
        }
        break;

    case GameStateOver:
        if(input->key == InputKeyOk) {
            game_start(app);
        } else if(input->key == InputKeyBack) {
            world->state = GameStateMenu;
        }
        break;
    }
}

/* ------------------------------------------------------------- lifecycle */

static FlipsJumpsApp* flips_jumps_app_alloc(void) {
    FlipsJumpsApp* app = malloc(sizeof(FlipsJumpsApp));
    memset(app, 0, sizeof(FlipsJumpsApp));

    app->running = true;
    app->sound_on = true;
    app->world.state = GameStateMenu;

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue = furi_message_queue_alloc(16, sizeof(GameEvent));

    flips_jumps_load(app);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, flips_jumps_draw_callback, app);
    view_port_input_callback_set(app->view_port, flips_jumps_input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(app->notifications, &sequence_display_backlight_enforce_on);

    app->timer = furi_timer_alloc(flips_jumps_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->timer, furi_kernel_get_tick_frequency() / GAME_FPS);

    return app;
}

static void flips_jumps_app_free(FlipsJumpsApp* app) {
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);

    sound_stop(app);

    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    furi_record_close(RECORD_NOTIFICATION);

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);

    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);

    free(app);
}

int32_t flips_jumps_app(void* p) {
    UNUSED(p);

    FlipsJumpsApp* app = flips_jumps_app_alloc();
    GameEvent event;

    while(app->running) {
        if(furi_message_queue_get(app->queue, &event, FuriWaitForever) != FuriStatusOk) continue;

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(event.type == GameEventTypeInput) {
            flips_jumps_handle_input(app, &event.input);
        } else {
            game_tick(app);
            sound_update(app);
        }
        furi_mutex_release(app->mutex);

        view_port_update(app->view_port);
    }

    flips_jumps_app_free(app);
    return 0;
}
