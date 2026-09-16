#include "flips_jumps.h"
void screen_dump(const char* title);
#include "bot.h"

int main(void) {
    game_seed(9);
    FlipsJumpsApp app;
    memset(&app, 0, sizeof(app));
    app.sound_on = true;

    /* Menu, mid-animation. */
    app.world.state = GameStateMenu;
    app.world.high_score = 1284;
    app.world.tick = 8;
    game_draw(NULL, &app.world, app.sound_on);
    screen_dump("MENU");

    /* Play until a spring, a crumbler and an enemy are all on screen. */
    game_reset(&app.world);
    Bot bot = {0};
    int best_frame = -1, best_types = -1;
    for(int f = 0; f < 4000 && app.world.state == GameStatePlaying; f++) {
        bot_input(&bot, &app.world);
        game_tick(&app.world);
        int types = 0;
        for(int i = 0; i < PLATFORM_COUNT; i++) {
            Platform* p = &app.world.platforms[i];
            float sy = p->y - app.world.camera_y;
            if(sy < 0 || sy > SCREEN_HEIGHT) continue;
            if(p->type == PlatformTypeSpring) types |= 1;
            if(p->type == PlatformTypeBreakable) types |= 2;
            if(p->type == PlatformTypeMoving) types |= 4;
        }
        if(app.world.enemy.active) types |= 8;
        int score = __builtin_popcount(types);
        if(score > best_types) { best_types = score; best_frame = f; }
    }

    game_seed(9);
    game_reset(&app.world);
    memset(&bot, 0, sizeof(bot));
    for(int f = 0; f <= best_frame && app.world.state == GameStatePlaying; f++) {
        bot_input(&bot, &app.world);
        game_tick(&app.world);
    }
    game_draw(NULL, &app.world, app.sound_on);
    screen_dump("IN GAME (score, platforms, player)");

    app.world.state = GameStatePaused;
    game_draw(NULL, &app.world, app.sound_on);
    screen_dump("PAUSED");

    app.world.state = GameStateOver;
    app.world.score = 1337;
    app.world.high_score = 4021;
    app.world.new_record = false;
    app.world.tick = 5;
    game_draw(NULL, &app.world, app.sound_on);
    screen_dump("GAME OVER");

    app.world.new_record = true;
    app.world.high_score = 1337;
    app.world.tick = 0;
    game_draw(NULL, &app.world, app.sound_on);
    screen_dump("GAME OVER (new record)");

    /* A reference sheet of every sprite, side by side. */
    memset(&app, 0, sizeof(app));
    GameWorld* w = &app.world;
    w->state = GameStatePlaying;
    w->score = 4242;
    w->tick = 3;
    for(int i = 0; i < PLATFORM_COUNT; i++) w->platforms[i].y = 999;
    /* One of each kind, stacked down the tall screen. */
    w->platforms[0] = (Platform){.x = 4,  .y = 20, .type = PlatformTypeNormal};
    w->platforms[1] = (Platform){.x = 30, .y = 20, .type = PlatformTypeMoving, .vx = 1.0f};
    w->platforms[2] = (Platform){.x = 4,  .y = 44, .type = PlatformTypeBreakable};
    w->platforms[3] = (Platform){.x = 30, .y = 44, .type = PlatformTypeSpring};
    w->platforms[4] = (Platform){.x = 30, .y = 68, .type = PlatformTypeSpring, .spring_frame = 5};
    w->platforms[5] = (Platform){.x = 4,  .y = 68, .type = PlatformTypeBreakable, .broken = true};
    /* Player mid-fall, facing right; enemy alongside. */
    w->player.x = 6; w->player.y = 90; w->player.vy = 2.0f; w->player.facing_left = false;
    w->enemy.active = true; w->enemy.x = 34; w->enemy.y = 90;

    game_draw(NULL, &app.world, app.sound_on);
    screen_dump("SPRITES: normal / moving / breakable / spring   +broken, compressed spring, player, enemy");

    /* Player wrapping across the screen edge, and the knocked-out face. */
    memset(w->platforms, 0, sizeof(w->platforms));
    for(int i = 0; i < PLATFORM_COUNT; i++) w->platforms[i].y = 999;
    w->platforms[0] = (Platform){.x = 0, .y = 60, .type = PlatformTypeNormal};
    w->player.x = 60; w->player.y = 18; w->player.dying = false;
    w->enemy.active = false;
    game_draw(NULL, &app.world, app.sound_on);
    screen_dump("PLAYER WRAPPING THE RIGHT EDGE");

    w->player.x = 28; w->player.dying = true;
    game_draw(NULL, &app.world, app.sound_on);
    screen_dump("KNOCKED OUT (X eyes)");

    return 0;
}
