/*
 * Flip's Jumps - rendering onto the 128x64 monochrome canvas.
 */
#include "flips_jumps.h"

static void draw_player_sprite(Canvas* canvas, int32_t x, int32_t y, bool facing_left, bool dying) {
    /* Body. */
    canvas_draw_rbox(canvas, x, y + 1, PLAYER_WIDTH, 7, 2);

    /* Feet. */
    canvas_draw_box(canvas, x + 1, y + 8, 2, 1);
    canvas_draw_box(canvas, x + 6, y + 8, 2, 1);

    /* Snout, pointing where we are heading. */
    canvas_draw_dot(canvas, facing_left ? x - 1 : x + PLAYER_WIDTH, y + 4);

    /* Eyes punched out of the body. */
    canvas_set_color(canvas, ColorWhite);
    if(dying) {
        canvas_draw_dot(canvas, x + 2, y + 3);
        canvas_draw_dot(canvas, x + 4, y + 3);
        canvas_draw_dot(canvas, x + 3, y + 4);
        canvas_draw_dot(canvas, x + 2, y + 5);
        canvas_draw_dot(canvas, x + 4, y + 5);
        canvas_draw_dot(canvas, x + 6, y + 4);
    } else {
        canvas_draw_box(canvas, facing_left ? x + 1 : x + 4, y + 3, 2, 2);
        canvas_draw_box(canvas, facing_left ? x + 4 : x + 6, y + 3, 1, 2);
    }
    canvas_set_color(canvas, ColorBlack);
}

/* The playfield wraps, so a sprite near an edge is drawn on both sides. */
static void draw_player(Canvas* canvas, const Player* player, float camera_y) {
    int32_t y = (int32_t)(player->y - camera_y);
    for(int8_t i = -1; i <= 1; i++) {
        int32_t x = (int32_t)player->x + i * SCREEN_WIDTH;
        if(x > SCREEN_WIDTH || x < -PLAYER_WIDTH) continue;
        draw_player_sprite(canvas, x, y, player->facing_left, player->dying);
    }
}

static void draw_platform(Canvas* canvas, const Platform* platform, float camera_y) {
    int32_t x = (int32_t)platform->x;
    int32_t y = (int32_t)(platform->y - camera_y);

    if(y < -PLATFORM_HEIGHT - 6 || y > SCREEN_HEIGHT) return;

    if(platform->broken) {
        /* Crumbled: two halves tumbling away. */
        canvas_draw_line(canvas, x, y, x + 6, y);
        canvas_draw_line(canvas, x + 11, y + 1, x + PLATFORM_WIDTH - 1, y + 1);
        return;
    }

    switch(platform->type) {
    case PlatformTypeBreakable:
        /* Hollow and dotted: it will not hold your weight. */
        canvas_draw_line(canvas, x, y, x + PLATFORM_WIDTH - 1, y);
        for(int32_t i = 0; i < PLATFORM_WIDTH; i += 3) {
            canvas_draw_dot(canvas, x + i, y + 2);
        }
        break;

    case PlatformTypeMoving:
        canvas_draw_box(canvas, x, y, PLATFORM_WIDTH, PLATFORM_HEIGHT);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_line(canvas, x + 5, y, x + 5, y + PLATFORM_HEIGHT - 1);
        canvas_draw_line(canvas, x + 12, y, x + 12, y + PLATFORM_HEIGHT - 1);
        canvas_set_color(canvas, ColorBlack);
        break;

    case PlatformTypeSpring: {
        canvas_draw_box(canvas, x, y, PLATFORM_WIDTH, PLATFORM_HEIGHT);
        /* Coil compresses for a few frames after it launches you. */
        int32_t height = platform->spring_frame ? 1 : 3;
        int32_t sx = x + 7;
        canvas_draw_line(canvas, sx + 1, y - height, sx + 1, y - 1);
        canvas_draw_line(canvas, sx, y - height, sx + 2, y - height);
        break;
    }

    default:
        canvas_draw_box(canvas, x, y, PLATFORM_WIDTH, PLATFORM_HEIGHT);
        break;
    }
}

static void draw_enemy(Canvas* canvas, const Enemy* enemy, float camera_y, uint32_t tick) {
    if(!enemy->active) return;

    int32_t x = (int32_t)enemy->x;
    int32_t y = (int32_t)(enemy->y - camera_y);
    if(y < -ENEMY_HEIGHT || y > SCREEN_HEIGHT) return;

    /* Antennae. */
    canvas_draw_line(canvas, x + 2, y + 2, x + 1, y);
    canvas_draw_line(canvas, x + 8, y + 2, x + 9, y);

    canvas_draw_rbox(canvas, x, y + 2, ENEMY_WIDTH, 6, 2);

    /* Flapping wings. */
    int32_t flap = (tick / 4) % 2;
    canvas_draw_line(canvas, x - 1, y + 3 + flap, x - 1, y + 5);
    canvas_draw_line(canvas, x + ENEMY_WIDTH, y + 3 + flap, x + ENEMY_WIDTH, y + 5);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x + 2, y + 4, 2, 2);
    canvas_draw_box(canvas, x + 7, y + 4, 2, 2);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_world(Canvas* canvas, const GameWorld* world) {
    for(uint8_t i = 0; i < PLATFORM_COUNT; i++) {
        draw_platform(canvas, &world->platforms[i], world->camera_y);
    }
    draw_enemy(canvas, &world->enemy, world->camera_y, world->tick);
    draw_player(canvas, &world->player, world->camera_y);
}

/* Score sits on a white pad so it stays readable over platforms. */
static void draw_hud(Canvas* canvas, const GameWorld* world) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)world->score);

    canvas_set_font(canvas, FontSecondary);
    uint16_t width = canvas_string_width(canvas, buffer);

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, width + 5, 11);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 2, 9, buffer);
}

static void draw_dialog(Canvas* canvas, int32_t x, int32_t y, int32_t width, int32_t height) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, x, y, width, height, 3);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, x, y, width, height, 3);
}

static void draw_menu(Canvas* canvas, FlipsJumpsApp* app) {
    const GameWorld* world = &app->world;
    char buffer[24];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "FLIP'S JUMPS");

    canvas_set_font(canvas, FontSecondary);
    snprintf(buffer, sizeof(buffer), "BEST %lu", (unsigned long)world->high_score);
    canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignCenter, buffer);

    /* A demo hop over a platform, ping-ponging through a 40 frame cycle. */
    int32_t phase = (int32_t)(world->tick % 40);
    int32_t height = phase < 20 ? phase : 40 - phase;
    canvas_draw_box(canvas, 55, 47, PLATFORM_WIDTH, PLATFORM_HEIGHT);
    draw_player_sprite(canvas, 59, 38 - height / 2, false, false);

    if((world->tick / 12) % 2 == 0) {
        canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignCenter, "PRESS OK TO JUMP");
    }
    canvas_draw_str(canvas, 0, 63, app->sound_on ? "UP:SND ON" : "UP:SND OFF");
    canvas_draw_str_aligned(canvas, 128, 63, AlignRight, AlignBottom, "< > MOVE");
}

static void draw_paused(Canvas* canvas) {
    /* Wide enough that the hint line stays clear of the frame. */
    draw_dialog(canvas, 6, 16, 116, 32);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 27, AlignCenter, AlignCenter, "PAUSED");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "OK:RESUME BACK:MENU");
}

static void draw_game_over(Canvas* canvas, const GameWorld* world) {
    char buffer[48];

    draw_dialog(canvas, 6, 8, 116, 48);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "GAME OVER");

    /* One stat per line: both on one line will not fit at any sane score. */
    canvas_set_font(canvas, FontSecondary);
    snprintf(buffer, sizeof(buffer), "SCORE %lu", (unsigned long)world->score);
    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buffer);

    if(world->new_record) {
        if((world->tick / 10) % 2 == 0) {
            canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignCenter, "NEW BEST!");
        }
    } else {
        snprintf(buffer, sizeof(buffer), "BEST %lu", (unsigned long)world->high_score);
        canvas_draw_str_aligned(canvas, 64, 41, AlignCenter, AlignCenter, buffer);
    }

    canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignCenter, "OK:RETRY BACK:MENU");
}

void game_draw(Canvas* canvas, FlipsJumpsApp* app) {
    GameWorld* world = &app->world;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(world->state == GameStateMenu) {
        draw_menu(canvas, app);
        return;
    }

    draw_world(canvas, world);
    draw_hud(canvas, world);

    if(world->state == GameStatePaused) {
        draw_paused(canvas);
    } else if(world->state == GameStateOver) {
        draw_game_over(canvas, world);
    }
}
