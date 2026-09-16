/*
 * Plays full games and renders every frame, asserting that no draw call is
 * ever issued with a negative coordinate.
 *
 * This matters because the Flipper passes coordinates straight to u8g2, whose
 * coordinate type is unsigned: a negative value is not clipped away, it becomes
 * ~65535 and the shape is painted somewhere it should not be. On hardware that
 * showed up as stripes across the display, worst when an enemy was on screen -
 * enemies enter from above, so they are drawn at negative y on the way in.
 */
#include "flips_jumps.h"
#include "bot.h"

extern int draw_violations;
extern char first_violation[256];

int main(void) {
    int frames = 0, enemy_frames = 0;

    for(int seed = 1; seed <= 40; seed++) {
        game_seed((uint32_t)seed);
        GameWorld world;
        memset(&world, 0, sizeof(world));
        game_reset(&world);
        Bot bot = {0};

        for(int f = 0; f < 6000 && world.state == GameStatePlaying; f++) {
            bot_input(&bot, &world);
            game_tick(&world);
            game_draw(NULL, &world, true);
            frames++;
            if(world.enemy.active) enemy_frames++;
        }

        /* The end-of-run screens draw over the world, so cover them too. */
        world.state = GameStatePaused;
        game_draw(NULL, &world, true);
        world.state = GameStateOver;
        game_draw(NULL, &world, true);
        world.state = GameStateMenu;
        game_draw(NULL, &world, true);
    }

    printf("rendered %d frames (%d with an enemy on screen)\n", frames, enemy_frames);
    if(draw_violations) {
        printf("FAIL: %d draw calls with a negative coordinate, first: %s\n",
               draw_violations, first_violation);
        return 1;
    }
    printf("no draw call used a negative coordinate\n");
    return 0;
}
