/*
 * A model of a competent human player, used to drive the game in tests.
 * It only ever reads the world the way a player reads the screen, and steers
 * with the same left/right input the real game accepts.
 */
#pragma once

#include "flips_jumps.h"

/* Wrap-aware horizontal distance from a to b, in [-64, 64]. */
static float wrap_delta(float a, float b) {
    float d = b - a;
    while(d > SCREEN_WIDTH / 2) d -= SCREEN_WIDTH;
    while(d < -SCREEN_WIDTH / 2) d += SCREEN_WIDTH;
    return d;
}

/*
 * Models a competent player. Re-aims every frame from the live platform
 * positions (they move) and the velocity it actually has right now, picking
 * the highest pad it can still reach both vertically and sideways.
 */
typedef struct {
    float prev_vy;
} Bot;

/* Frames until the feet rise/fall to `gap` px above them. Negative: unreachable. */
static float time_to_rise(float vy_up, float gap) {
    if(vy_up <= 0.0f) return -1.0f;
    /* Land clear of the apex: grazing it leaves no time to steer sideways. */
    float apex = (vy_up * vy_up) / (2.0f * GRAVITY);
    if(gap > apex - 2.5f) return -1.0f;
    float disc = vy_up * vy_up - 2.0f * GRAVITY * gap;
    if(disc < 0.0f) return -1.0f;
    return (vy_up + sqrtf(disc)) / GRAVITY; /* the descending crossing */
}

static float time_to_fall(float vy_down, float drop) {
    float disc = vy_down * vy_down + 2.0f * GRAVITY * drop;
    return (-vy_down + sqrtf(disc)) / GRAVITY;
}

static bool reachable_sideways(float centre, float target_centre, float frames) {
    float dx = fabsf(wrap_delta(centre, target_centre));
    return dx <= MOVE_SPEED_MAX * frames - 4.0f; /* margin for accelerating */
}

static void bot_input(Bot* bot, GameWorld* w) {
    bot->prev_vy = w->player.vy;

    float feet = w->player.y + PLAYER_HEIGHT;
    float centre = w->player.x + PLAYER_WIDTH / 2.0f;
    const Platform* target = NULL;
    float best = -1.0f;

    /*
     * Still climbing: take the highest pad we can reach comfortably. Chasing a
     * rung right at the limit of the jump fails more often than it gains, and
     * a player who keeps trying it never climbs at all, so anything marginal
     * loses to simply taking the next rung up.
     */
    if(w->player.vy < 0.0f) {
        const Platform* safest = NULL;
        float safest_gap = 1e9f;
        float apex = (w->player.vy * w->player.vy) / (2.0f * GRAVITY);

        for(uint8_t i = 0; i < PLATFORM_COUNT; i++) {
            const Platform* p = &w->platforms[i];
            if(p->broken || p->type == PlatformTypeBreakable) continue;
            float gap = feet - p->y;
            if(gap <= 0.5f) continue;
            float t = time_to_rise(-w->player.vy, gap);
            if(t < 0.0f) continue;

            float centre_p = p->x + PLATFORM_WIDTH / 2.0f;
            if(!reachable_sideways(centre, centre_p, t)) continue;

            if(gap < safest_gap) { safest_gap = gap; safest = p; }

            /* Comfortable: well clear of the apex, with time to spare sideways. */
            bool roomy = gap <= apex - 5.0f &&
                         fabsf(wrap_delta(centre, centre_p)) <=
                             0.6f * (MOVE_SPEED_MAX * t - 4.0f);
            if(roomy && gap > best) { best = gap; target = p; }
        }
        if(!target) target = safest;
    }

    /* Falling (or nothing above): take the highest pad below that we can make. */
    if(!target) {
        float closest = 1e9f;
        for(uint8_t i = 0; i < PLATFORM_COUNT; i++) {
            const Platform* p = &w->platforms[i];
            if(p->broken || p->type == PlatformTypeBreakable) continue;
            float drop = p->y - feet;
            if(drop <= 0.0f) continue;
            float t = time_to_fall(w->player.vy > 0.0f ? w->player.vy : 0.0f, drop);
            if(!reachable_sideways(centre, p->x + PLATFORM_WIDTH / 2.0f, t)) continue;
            if(drop < closest) { closest = drop; target = p; }
        }
    }

    /* Nothing makeable: chase the nearest pad below anyway. */
    if(!target) {
        float closest = 1e9f;
        for(uint8_t i = 0; i < PLATFORM_COUNT; i++) {
            const Platform* p = &w->platforms[i];
            if(p->broken) continue;
            float drop = p->y - feet;
            if(drop > 0.0f && drop < closest) { closest = drop; target = p; }
        }
    }

    if(!target) { w->input_dir = 0; return; }

    float dx = wrap_delta(centre, target->x + PLATFORM_WIDTH / 2.0f);
    w->input_dir = (dx > 1.0f) ? 1 : (dx < -1.0f ? -1 : 0);
}

