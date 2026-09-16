/* Native harness: runs the Flipper game simulation headless and checks it plays. */
#include "flips_jumps.h"
#include <assert.h>



#define FAIL(...) do { printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } while(0)
static int failures = 0;

/* Theoretical apex of a normal hop, in pixels. */
static float jump_height(float v0) { return (v0 * v0) / (2.0f * GRAVITY); }

#include "bot.h"

typedef struct {
    uint32_t score;
    uint32_t frames;
    int enemies_seen;
    int max_visible_gap;
    int min_visible_platforms;
} RunResult;

static RunResult play(bool use_bot, uint32_t max_frames) {
    GameWorld world;
    memset(&world, 0, sizeof(world));
    game_reset(&world);
    GameWorld* w = &world;

    Bot bot = {0};
    RunResult r = {0};
    r.min_visible_platforms = 99;
    float last_camera = w->camera_y;
    uint32_t last_score = 0;

    while(w->state == GameStatePlaying && r.frames < max_frames) {
        if(use_bot) bot_input(&bot, w);
        game_tick(w);
        r.frames++;

        const Player* p = &w->player;
        if(isnan(p->x) || isnan(p->y) || isnan(p->vy) || isnan(p->vx)) FAIL("NaN in player state");
        if(p->x < -(float)PLAYER_WIDTH - 1.0f || p->x > SCREEN_WIDTH + 1.0f)
            FAIL("player escaped horizontally: x=%f", (double)p->x);
        if(w->camera_y > last_camera + 0.001f) FAIL("camera scrolled downwards");
        last_camera = w->camera_y;
        if(w->score < last_score) FAIL("score decreased %u -> %u", last_score, w->score);
        last_score = w->score;
        if(w->enemy.active) r.enemies_seen = 1;

        /* Platform coverage: sorted gaps and how many are on screen. */
        float ys[PLATFORM_COUNT];
        int n = 0, visible = 0;
        for(uint8_t i = 0; i < PLATFORM_COUNT; i++) {
            const Platform* pf = &w->platforms[i];
            if(!isfinite(pf->y)) FAIL("platform y not finite");
            if(pf->broken) continue;
            float sy = pf->y - w->camera_y;
            if(sy >= -4.0f && sy < SCREEN_HEIGHT) visible++;
            ys[n++] = pf->y;
        }
        if(visible < r.min_visible_platforms) r.min_visible_platforms = visible;

        for(int i = 0; i < n; i++)
            for(int j = i + 1; j < n; j++)
                if(ys[j] < ys[i]) { float t = ys[i]; ys[i] = ys[j]; ys[j] = t; }
        for(int i = 1; i < n; i++) {
            float sy = ys[i] - w->camera_y;
            if(sy < -10.0f || sy > SCREEN_HEIGHT) continue; /* only judge the ladder in play */
            int gap = (int)(ys[i] - ys[i - 1]);
            if(gap > r.max_visible_gap) r.max_visible_gap = gap;
            /*
             * The invariant that makes the game winnable: every rung of the
             * ladder in play is within one hop of the rung below it, even
             * after a crumbling platform has dropped out of the chain.
             */
            if(gap > PLATFORM_REACH)
                FAIL("unclimbable ladder: %dpx between rungs, a hop clears %d", gap, PLATFORM_REACH);
        }
    }
    r.score = w->score;
    return r;
}

static void trace_one(void) {
    GameWorld world;
    memset(&world, 0, sizeof(world));
    game_reset(&world);
    GameWorld* w = &world;
    Bot bot = {0};
    int f = 0;
    while(w->state == GameStatePlaying && f < 300) {
        bot_input(&bot, w);
        game_tick(w);
        f++;
        printf("f%-3d feetscr=%5.1f vy=%5.2f px=%5.1f dir=%2d |",
               f, (double)(w->player.y + PLAYER_HEIGHT - w->camera_y),
               (double)w->player.vy, (double)w->player.x, w->input_dir);
        for(int i = 0; i < PLATFORM_COUNT; i++) {
            Platform* p = &w->platforms[i];
            float sy = p->y - w->camera_y;
            if(sy < -15 || sy > 75) continue;
            printf(" (y%.0f x%.0f-%.0f t%d%s)", (double)sy, (double)p->x,
                   (double)(p->x + PLATFORM_WIDTH), p->type, p->broken ? "B" : "");
        }
        printf("\n");
    }
    printf("DIED frame %d score %u\n", f, w->score);
}

int main(int argc, char** argv) {
    if(argc > 2) { srand(atoi(argv[2])); trace_one(); return 0; }
    game_seed(1234);

    (void)argc; (void)argv;
    printf("=== physics ===\n");
    float hop = jump_height(-JUMP_VELOCITY);
    float spring = jump_height(-SPRING_VELOCITY);
    printf("normal hop apex : %.1f px\n", (double)hop);
    printf("spring apex     : %.1f px\n", (double)spring);
    printf("largest gap spawned: %d px\n", MAX_GAP + 100 / 20);
    if(hop <= (float)(MAX_GAP + 100 / 20) + 4.0f)
        FAIL("a normal hop (%.1f px) cannot clear the largest gap (%d px)",
             (double)hop, MAX_GAP + 100 / 20);

    printf("\n=== idle player (no input) ===\n");
    RunResult idle = play(false, 3000);
    printf("survived %u frames, score %u\n", idle.frames, idle.score);
    if(idle.frames < 3000) FAIL("idle player died after %u frames; start pad is not safe", idle.frames);

    printf("\n=== bot runs ===\n");
    uint32_t scores[40];
    uint32_t total = 0, best = 0, quick_deaths = 0;
    int enemies = 0, max_gap = 0, min_visible = 99;
    for(int i = 0; i < 40; i++) {
        RunResult r = play(true, 20000);
        scores[i] = r.score;
        total += r.score;
        if(r.score > best) best = r.score;
        if(r.frames < 90) quick_deaths++;
        enemies += r.enemies_seen;
        if(r.max_visible_gap > max_gap) max_gap = r.max_visible_gap;
        if(r.min_visible_platforms < min_visible) min_visible = r.min_visible_platforms;
    }
    for(int i = 0; i < 40; i++)
        for(int j = i + 1; j < 40; j++)
            if(scores[j] < scores[i]) { uint32_t t = scores[i]; scores[i] = scores[j]; scores[j] = t; }

    printf("median score %u, mean %u, best %u, worst %u\n", scores[20], total / 40, best, scores[0]);
    printf("runs that died in under 3s: %u/40\n", quick_deaths);
    printf("runs where an enemy appeared: %d/40\n", enemies);
    printf("largest on-screen platform gap: %d px (hop clears %.0f)\n", max_gap, (double)hop);
    printf("fewest platforms on screen at once: %d\n", min_visible);

    if(scores[20] == 0) FAIL("median bot score is zero; the game is unplayable");
    if(quick_deaths > 4) FAIL("%u/40 runs died almost immediately", quick_deaths);
    if(min_visible < 1) FAIL("screen had no platforms at some point");
    if(enemies == 0) FAIL("enemies never spawned in any run");

    /* Long tail: many runs, watching for drift, stuck states and gap blowups. */
    /* Every platform kind must actually reach the player. */
    printf("\n=== platform mix (10k spawns at rising difficulty) ===\n");
    {
        int seen[4] = {0};
        GameWorld mix;
        memset(&mix, 0, sizeof(mix));
        game_reset(&mix);
        GameWorld* gw = &mix;
        int fragile_pairs = 0;
        for(int i = 0; i < 10000; i++) {
            gw->score = (uint32_t)i / 8; /* sweep the whole difficulty curve */
            /* Force a recycle of platform 0 by dropping it below the screen. */
            gw->platforms[0].y = gw->camera_y + SCREEN_HEIGHT + 20.0f;
            PlatformType before_fragile = gw->last_was_fragile ? PlatformTypeBreakable : PlatformTypeNormal;
            game_tick(gw);
            PlatformType t = gw->platforms[0].type;
            seen[t]++;
            if(t == PlatformTypeBreakable && before_fragile == PlatformTypeBreakable) fragile_pairs++;
        }
        const char* names[4] = {"normal", "moving", "breakable", "spring"};
        for(int t = 0; t < 4; t++)
            printf("  %-10s %5d (%.1f%%)\n", names[t], seen[t], seen[t] / 100.0);
        for(int t = 0; t < 4; t++)
            if(seen[t] == 0) FAIL("%s platforms never spawn", names[t]);
        if(fragile_pairs) FAIL("%d back-to-back crumblers spawned", fragile_pairs);
    }

    printf("\n=== endurance (300 runs, up to 200k frames each) ===\n");
    uint32_t endurance_best = 0, endurance_frames = 0, deaths = 0;
    int worst_gap = 0;
    float slowest_rate = 1e9f;
    int stuck_runs = 0;
    for(int i = 0; i < 300; i++) {
        RunResult r = play(true, 200000);
        if(r.score > endurance_best) endurance_best = r.score;
        if(r.frames > endurance_frames) endurance_frames = r.frames;
        if(r.max_visible_gap > worst_gap) worst_gap = r.max_visible_gap;
        if(r.frames < 200000) deaths++;
        /* A run that lasts a long time must be climbing, not stuck bouncing. */
        if(r.frames > 5000) {
            float rate = (float)r.score / (float)r.frames;
            if(rate < slowest_rate) slowest_rate = rate;
            if(rate < 0.02f) stuck_runs++;
        }
    }
    printf("slowest climb rate over a long run: %.3f pts/frame\n", (double)slowest_rate);
    printf("runs where the bot stalled out: %d/300\n", stuck_runs);
    if(stuck_runs > 15) FAIL("%d/300 runs made no progress: likely a soft-lock", stuck_runs);
    printf("best score %u over %u frames (%.1f min of play)\n",
           endurance_best, endurance_frames, endurance_frames / (double)(GAME_FPS * 60));
    printf("runs that ended in death: %u/300 (endless game, so expected)\n", deaths);
    printf("worst on-screen gap seen: %d px\n", worst_gap);
    if(endurance_best < 500) FAIL("skilled play never got past %u points", endurance_best);

    printf("\n%s (%d failures)\n", failures ? "FAILED" : "ALL CHECKS PASSED", failures);
    return failures ? 1 : 0;
}
