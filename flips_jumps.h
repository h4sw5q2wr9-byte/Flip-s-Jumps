/*
 * Flip's Jumps - a Doodle Jump style endless jumper for the Flipper Zero.
 */
#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

#define PLAYER_WIDTH  9
#define PLAYER_HEIGHT 9

#define PLATFORM_WIDTH  18
#define PLATFORM_HEIGHT 3
#define PLATFORM_COUNT  10

#define ENEMY_WIDTH  11
#define ENEMY_HEIGHT 8

/* Physics. Tuned so a normal hop clears ~25px, i.e. the largest gap we spawn. */
#define GRAVITY         0.18f
#define JUMP_VELOCITY   -3.1f
#define SPRING_VELOCITY -4.4f
#define MAX_FALL_SPEED  6.0f
#define MOVE_ACCEL      0.70f
#define MOVE_SPEED_MAX  3.4f
#define MOVE_FRICTION   0.82f

/* The player never rises above this screen line; the camera scrolls instead. */
#define CAMERA_LINE    26.0f
#define PLAYER_START_Y 28.0f

#define MIN_GAP 11
#define MAX_GAP 16 /* grows with difficulty, never past the jump height */

/*
 * How far sideways the next platform may sit from the previous one. The whole
 * screen is only 128px wide, so fully random placement can demand a dash the
 * player has no time to make; this keeps every ladder climbable.
 */
#define PLATFORM_MAX_X_STEP 44

/*
 * The tallest hole we ever allow between two rungs, kept under the 26.7px a
 * hop actually clears so there is room to steer sideways on the way up. A
 * crumbling platform leaves its hole behind permanently, so the rungs around
 * one are placed to stay inside this budget.
 */
#define PLATFORM_REACH 23

#define ENEMY_MIN_SCORE 150

#define GAME_FPS 30

#define FLIPS_JUMPS_SAVE_FOLDER "/ext/apps_data/flips_jumps"
#define FLIPS_JUMPS_SAVE_PATH   FLIPS_JUMPS_SAVE_FOLDER "/flips_jumps.save"
#define FLIPS_JUMPS_SAVE_MAGIC  0x464A5031UL /* "FJP1" */

typedef enum {
    PlatformTypeNormal,
    PlatformTypeMoving,
    PlatformTypeBreakable,
    PlatformTypeSpring,
} PlatformType;

typedef struct {
    float x;
    float y; /* world space, y grows downwards */
    float vx; /* moving platforms only */
    float break_vy; /* breakable platforms falling away */
    PlatformType type;
    bool broken;
    uint8_t spring_frame; /* spring compression animation */
} Platform;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    bool facing_left;
    bool dying; /* knocked out, falling off screen */
} Player;

typedef struct {
    float x;
    float y;
    float vx;
    bool active;
} Enemy;

typedef enum {
    GameStateMenu,
    GameStatePlaying,
    GameStatePaused,
    GameStateOver,
} GameState;

typedef struct {
    GameState state;
    Player player;
    Platform platforms[PLATFORM_COUNT];
    Enemy enemy;

    float camera_y; /* world y drawn at the top of the screen */
    float highest_y; /* smallest y ever reached, i.e. the high water mark */
    float top_platform_y; /* y of the most recently spawned platform */

    uint32_t score;
    uint32_t bonus; /* points that do not come from climbing */
    uint32_t high_score;
    uint32_t tick;
    uint16_t enemy_cooldown;

    int8_t input_dir; /* -1 left, 0 idle, 1 right */
    bool left_held;
    bool right_held;
    bool last_was_fragile; /* never spawn two breakable platforms in a row */
    float last_platform_x; /* keeps consecutive platforms within reach */
    uint32_t last_gap; /* gap used by the last spawn, to budget the next one */
    bool new_record;
} GameWorld;

typedef struct {
    GameWorld world;
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    FuriTimer* timer;
    NotificationApp* notifications;

    bool running;
    bool sound_on;
    bool speaker_acquired;
    uint8_t sound_ticks;
} FlipsJumpsApp;

typedef enum {
    GameEventTypeInput,
    GameEventTypeTick,
} GameEventType;

typedef struct {
    GameEventType type;
    InputEvent input;
} GameEvent;

/* flips_jumps.c */
void sound_play(FlipsJumpsApp* app, float frequency, uint8_t ticks);
void sound_stop(FlipsJumpsApp* app);
void flips_jumps_save(FlipsJumpsApp* app);

/* game.c */
void game_reset(GameWorld* world);
void game_tick(FlipsJumpsApp* app);

/* draw.c */
void game_draw(Canvas* canvas, FlipsJumpsApp* app);
