/*
 * Flip's Jumps - simulation: physics, platform generation, collisions.
 */
#include "flips_jumps.h"

static uint32_t rnd_range(uint32_t min, uint32_t max) {
    if(max <= min) return min;
    return min + (furi_hal_random_get() % (max - min + 1));
}

static float rnd_float(float min, float max) {
    return min + (float)(furi_hal_random_get() % 1000UL) * 0.001f * (max - min);
}

static bool rnd_chance(uint32_t percent) {
    return rnd_range(0, 99) < percent;
}

/*
 * The playfield wraps horizontally, so a box near an edge also overlaps
 * anything on the opposite edge. Test the three candidate positions.
 */
static bool boxes_overlap_x(float ax, float aw, float bx, float bw) {
    for(int8_t i = -1; i <= 1; i++) {
        float x = ax + (float)i * (float)SCREEN_WIDTH;
        if((x + aw > bx) && (x < bx + bw)) return true;
    }
    return false;
}

/* 0 at the start, 100 once the run is going well. Drives platform mix and gaps. */
static uint32_t difficulty(const GameWorld* world) {
    uint32_t d = world->score / 12;
    return d > 100 ? 100 : d;
}

/*
 * A crumbler is placed at most PLATFORM_REACH - MIN_GAP above the rung below
 * it, and the rung above it gets whatever is left of the budget, so the hole
 * it leaves behind is always jumpable.
 */
_Static_assert(2 * MIN_GAP <= PLATFORM_REACH, "a crumbler's hole must stay within one hop");

static PlatformType platform_choose_type(GameWorld* world, uint32_t d) {
    uint32_t spring_chance = 7;
    uint32_t break_chance = d / 4; /* up to 25% */
    uint32_t moving_chance = 8 + d / 3; /* up to 41% */
    uint32_t roll = rnd_range(0, 99);

    /* Two crumblers in a row would leave a hole nobody can jump. */
    bool allow_fragile = !world->last_was_fragile;

    if(roll < spring_chance) return PlatformTypeSpring;
    if(roll < spring_chance + break_chance && allow_fragile) return PlatformTypeBreakable;
    if(roll < spring_chance + break_chance + moving_chance) return PlatformTypeMoving;
    return PlatformTypeNormal;
}

static void platform_place(GameWorld* world, Platform* platform, float y, PlatformType type) {
    /* Step sideways from the last platform rather than teleporting anywhere,
       reflecting off the screen edges so the spread stays even. */
    int32_t limit = SCREEN_WIDTH - PLATFORM_WIDTH;
    int32_t x = (int32_t)world->last_platform_x +
                (int32_t)rnd_range(0, 2 * PLATFORM_MAX_X_STEP) - PLATFORM_MAX_X_STEP;
    if(x < 0) x = -x;
    if(x > limit) x = 2 * limit - x;
    if(x < 0) x = 0;

    platform->y = y;
    platform->x = (float)x;
    platform->vx = 0.0f;
    platform->break_vy = 0.0f;
    platform->broken = false;
    platform->spring_frame = 0;
    platform->type = type;

    if(type == PlatformTypeMoving) {
        uint32_t d = difficulty(world);
        platform->vx = rnd_float(0.4f, 0.9f + (float)d * 0.006f);
        if(rnd_chance(50)) platform->vx = -platform->vx;
    }

    world->last_was_fragile = (type == PlatformTypeBreakable);
    world->last_platform_x = platform->x;
    world->top_platform_y = y;
}

static void platform_spawn_above(GameWorld* world, Platform* platform) {
    uint32_t d = difficulty(world);
    uint32_t max_gap = MAX_GAP + d / 20; /* 16..21, comfortably inside one hop */

    /*
     * The rung above a crumbler has to be reachable from the rung below it,
     * because once the crumbler is gone the two are all that is left.
     */
    if(world->last_was_fragile) {
        uint32_t budget =
            (PLATFORM_REACH > world->last_gap) ? (PLATFORM_REACH - world->last_gap) : MIN_GAP;
        if(max_gap > budget) max_gap = budget;
    }
    if(max_gap < MIN_GAP) max_gap = MIN_GAP;

    PlatformType type = platform_choose_type(world, d);
    if(type == PlatformTypeBreakable) {
        /* Keep crumblers close to the rung below so their hole stays jumpable. */
        uint32_t fragile_max = PLATFORM_REACH - MIN_GAP;
        if(max_gap > fragile_max) max_gap = fragile_max;
    }

    uint32_t gap = rnd_range(MIN_GAP, max_gap);
    world->last_gap = gap;
    platform_place(world, platform, world->top_platform_y - (float)gap, type);
}

void game_reset(GameWorld* world) {
    uint32_t high_score = world->high_score;

    memset(world, 0, sizeof(GameWorld));
    world->high_score = high_score;
    world->state = GameStatePlaying;

    world->player.x = (float)((SCREEN_WIDTH - PLAYER_WIDTH) / 2);
    world->player.y = PLAYER_START_Y;
    world->player.vy = JUMP_VELOCITY;
    world->highest_y = world->player.y;
    world->camera_y = 0.0f;

    /* A guaranteed landing pad right below the player, then a ladder upwards. */
    Platform* first = &world->platforms[0];
    first->type = PlatformTypeNormal;
    first->x = (float)((SCREEN_WIDTH - PLATFORM_WIDTH) / 2);
    first->y = PLAYER_START_Y + PLAYER_HEIGHT + 15.0f;
    world->top_platform_y = first->y;
    world->last_platform_x = first->x;

    for(uint8_t i = 1; i < PLATFORM_COUNT; i++) {
        /* The opening rungs are a gentle, solid warm-up. */
        uint32_t gap = rnd_range(MIN_GAP, MAX_GAP);
        world->last_gap = gap;
        platform_place(
            world, &world->platforms[i], world->top_platform_y - (float)gap, PlatformTypeNormal);
    }
}

static void platforms_update(GameWorld* world) {
    for(uint8_t i = 0; i < PLATFORM_COUNT; i++) {
        Platform* platform = &world->platforms[i];

        if(platform->broken) {
            platform->break_vy += GRAVITY;
            platform->y += platform->break_vy;
        } else if(platform->type == PlatformTypeMoving) {
            platform->x += platform->vx;
            if(platform->x < 0.0f) {
                platform->x = 0.0f;
                platform->vx = -platform->vx;
            } else if(platform->x > (float)(SCREEN_WIDTH - PLATFORM_WIDTH)) {
                platform->x = (float)(SCREEN_WIDTH - PLATFORM_WIDTH);
                platform->vx = -platform->vx;
            }
        }

        if(platform->spring_frame) platform->spring_frame--;

        /* Recycle anything that has scrolled off the bottom. */
        if(platform->y - world->camera_y > (float)SCREEN_HEIGHT + 10.0f) {
            platform_spawn_above(world, platform);
        }
    }
}

static void enemy_update(GameWorld* world) {
    Enemy* enemy = &world->enemy;

    if(!enemy->active) {
        if(world->enemy_cooldown) {
            world->enemy_cooldown--;
        } else if(world->score >= ENEMY_MIN_SCORE && rnd_range(0, 179) == 0) {
            enemy->active = true;
            enemy->x = (float)rnd_range(0, SCREEN_WIDTH - ENEMY_WIDTH);
            enemy->y = world->camera_y - 16.0f;
            enemy->vx = rnd_float(0.3f, 0.9f);
            if(rnd_chance(50)) enemy->vx = -enemy->vx;
        }
        return;
    }

    enemy->x += enemy->vx;
    if(enemy->x < 0.0f) {
        enemy->x = 0.0f;
        enemy->vx = -enemy->vx;
    } else if(enemy->x > (float)(SCREEN_WIDTH - ENEMY_WIDTH)) {
        enemy->x = (float)(SCREEN_WIDTH - ENEMY_WIDTH);
        enemy->vx = -enemy->vx;
    }

    if(enemy->y - world->camera_y > (float)SCREEN_HEIGHT + 20.0f) {
        enemy->active = false;
        world->enemy_cooldown = 90;
    }
}

static void game_over(FlipsJumpsApp* app) {
    GameWorld* world = &app->world;

    world->state = GameStateOver;
    if(world->score > world->high_score) {
        world->high_score = world->score;
        world->new_record = true;
        flips_jumps_save(app);
    }

    sound_stop(app);
    notification_message(app->notifications, &sequence_single_vibro);
}

static void player_knocked_out(FlipsJumpsApp* app) {
    Player* player = &app->world.player;

    player->dying = true;
    player->vy = -1.2f;
    player->vx = 0.0f;
    sound_play(app, 110.0f, 8);
    notification_message(app->notifications, &sequence_blink_red_100);
}

static void player_collide_platforms(FlipsJumpsApp* app, float prev_bottom) {
    GameWorld* world = &app->world;
    Player* player = &world->player;

    if(player->vy <= 0.0f) return;

    float bottom = player->y + PLAYER_HEIGHT;

    for(uint8_t i = 0; i < PLATFORM_COUNT; i++) {
        Platform* platform = &world->platforms[i];
        if(platform->broken) continue;

        float top = platform->y;
        /* Swept test: the feet crossed the platform top during this frame. */
        if(prev_bottom > top || bottom < top) continue;
        if(!boxes_overlap_x(player->x, PLAYER_WIDTH, platform->x, PLATFORM_WIDTH)) continue;

        switch(platform->type) {
        case PlatformTypeBreakable:
            /* Crumbles away, the player drops straight through it. */
            platform->broken = true;
            platform->break_vy = 0.6f;
            sound_play(app, 140.0f, 3);
            break;
        case PlatformTypeSpring:
            player->y = top - PLAYER_HEIGHT;
            player->vy = SPRING_VELOCITY;
            platform->spring_frame = 10;
            sound_play(app, 1320.0f, 4);
            break;
        default:
            player->y = top - PLAYER_HEIGHT;
            player->vy = JUMP_VELOCITY;
            sound_play(app, 660.0f, 2);
            break;
        }
        return;
    }
}

static void player_collide_enemy(FlipsJumpsApp* app, float prev_bottom) {
    GameWorld* world = &app->world;
    Player* player = &world->player;
    Enemy* enemy = &world->enemy;

    if(!enemy->active) return;
    if(!boxes_overlap_x(player->x, PLAYER_WIDTH, enemy->x, ENEMY_WIDTH)) return;
    if(player->y + PLAYER_HEIGHT <= enemy->y || player->y >= enemy->y + ENEMY_HEIGHT) return;

    if(player->vy > 0.0f && prev_bottom <= enemy->y + 4.0f) {
        /* Stomped from above. */
        enemy->active = false;
        world->enemy_cooldown = 120;
        world->bonus += 50;
        player->vy = JUMP_VELOCITY * 1.15f;
        sound_play(app, 880.0f, 4);
    } else {
        player_knocked_out(app);
    }
}

void game_tick(FlipsJumpsApp* app) {
    GameWorld* world = &app->world;
    Player* player = &world->player;

    world->tick++;
    if(world->state != GameStatePlaying) return;

    float prev_bottom = player->y + PLAYER_HEIGHT;

    /* Horizontal: accelerate while held, coast to a stop when released. */
    if(world->input_dir != 0 && !player->dying) {
        player->vx += (float)world->input_dir * MOVE_ACCEL;
        player->facing_left = world->input_dir < 0;
    } else {
        player->vx *= MOVE_FRICTION;
        if(fabsf(player->vx) < 0.05f) player->vx = 0.0f;
    }
    if(player->vx > MOVE_SPEED_MAX) player->vx = MOVE_SPEED_MAX;
    if(player->vx < -MOVE_SPEED_MAX) player->vx = -MOVE_SPEED_MAX;

    player->x += player->vx;
    if(player->x + PLAYER_WIDTH < 0.0f) {
        player->x = (float)SCREEN_WIDTH;
    } else if(player->x > (float)SCREEN_WIDTH) {
        player->x = -(float)PLAYER_WIDTH;
    }

    /* Vertical. */
    player->vy += GRAVITY;
    if(player->vy > MAX_FALL_SPEED) player->vy = MAX_FALL_SPEED;
    player->y += player->vy;

    platforms_update(world);
    enemy_update(world);

    if(!player->dying) {
        player_collide_platforms(app, prev_bottom);
        player_collide_enemy(app, prev_bottom);

        /* The camera only ever follows upwards. */
        if(player->y - world->camera_y < CAMERA_LINE) {
            world->camera_y = player->y - CAMERA_LINE;
        }
        if(player->y < world->highest_y) world->highest_y = player->y;

        float climb = PLAYER_START_Y - world->highest_y;
        if(climb < 0.0f) climb = 0.0f;
        world->score = (uint32_t)(climb / 2.0f) + world->bonus;
    }

    if(player->y - world->camera_y > (float)SCREEN_HEIGHT + 4.0f) {
        game_over(app);
    }
}
