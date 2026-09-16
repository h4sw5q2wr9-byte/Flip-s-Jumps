# Flip's Jumps

A Doodle Jump style endless jumper for the [Flipper Zero](https://flipperzero.one/).
Flip bounces automatically — you only steer. Miss the platforms and it is a long
way down.

The game runs **rotated**: hold the Flipper turned a quarter turn, D-pad below the
screen, for a tall 64x128 playfield. If it comes out upside down for the way you
hold it, change `FLIPS_JUMPS_ORIENTATION` in `flips_jumps.h` to
`ViewPortOrientationVerticalFlip`.

```
   FLIP'S            +--------------+
   JUMPS             |           12 |
  BEST 1284          |  ######      |
                     |         ____ |   <- spring: plate on a coil
    ,--.             |        |####||
   ( oo )            |   ,--. #####  |
    `--'             |  ( oo )      |
 ##########          |   `--'       |
                     | ----------   |   <- crumbling: dotted, drops you
  PRESS OK           |    ######    |
  < > MOVE           +--------------+
  UP:SND ON
```

## Controls

| Button | Action |
| --- | --- |
| ← / → | Steer left and right (hold to keep moving). The firmware remaps the D-pad for the rotated screen, so these are whichever keys point left and right as you hold it. |
| OK | Start, pause, resume, retry |
| Back | Pause, then back to the menu |
| Back (hold) | Quit to the app list |
| Up (in menu) | Toggle sound |

The screen wraps: run off the right edge and you come back on the left.

## Platforms

| | Platform | Behaviour |
| --- | --- | --- |
| `##########` | Normal | Solid. Bounces you straight back up. |
| `#### #####` | Moving | Slides side to side. Bounces normally — if you can land on it. |
| `----------` | Crumbling | Falls apart the moment you touch it. No bounce, so you drop through. |
| plate on a coil | Spring | Launches you about twice as high as a normal hop, and squashes flat as it fires. |

Above 150 points, bugs start drifting down the screen. Land on one from above to
squash it for 50 points; touch it any other way and the run is over.

Your best score is saved to `/ext/apps_data/flips_jumps/` and survives a reboot.

## Building

The app builds with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```sh
pip install ufbt
ufbt            # builds dist/flips_jumps.fap
ufbt launch     # builds, uploads and runs it on a connected Flipper
```

Copy `dist/flips_jumps.fap` to `apps/Games/` on the SD card to install it by hand.

## Layout

| File | Contents |
| --- | --- |
| `flips_jumps.h` | Shared types and all the tuning constants |
| `flips_jumps.c` | App lifecycle, input, sound, saved high score |
| `game.c` | Physics, platform generation, collisions |
| `draw.c` | Everything drawn on the 128x64 screen |
| `test/` | Host-side test suite (see below) |

## Tests

The simulation is plain C with no firmware dependencies, so it can be compiled
and played on a normal machine — no Flipper and no SDK needed:

```sh
./test/run_tests.sh             # playability, generation and renderer checks
./test/run_tests.sh --screens   # also dump every screen as ASCII art
```

`game.c` is pure simulation with no firmware calls at all, which is what makes
this possible. The tick records sound, vibration and saving as `Effects` for the
app layer to perform; it never calls them itself.

`test/bot.h` is a model of a competent player that reads the world the way a
person reads the screen and steers with the same left/right input the game
accepts. The harness plays hundreds of full games with it and checks that:

- a normal hop clears the largest gap the generator can produce, with margin;
- a player who never touches a button is not killed by the opening screen;
- every rung of the ladder in play stays within one hop of the rung below it,
  **including** after a crumbling platform drops out of the chain — the check
  that stops the game from generating a wall nobody can climb;
- all four platform kinds actually spawn, and crumbling ones never spawn back
  to back;
- scores rise, the camera never scrolls backwards, the player never leaves the
  screen, and no value ever goes NaN;
- long runs keep climbing rather than stalling out on one platform.

`test/canvas_sim.c` reimplements the Flipper's drawing calls against an
in-memory bitmap, so `--screens` renders the real `draw.c` output as ASCII art.
That is how the menu, both dialogs and every sprite were checked for overflow
and clipping without hardware.

`test/render_check.c` renders every frame of 40 full games and asserts that no
draw call ever uses a negative coordinate. The Flipper passes coordinates to
u8g2, whose coordinate type is unsigned, so a negative value is not clipped - it
becomes ~65535 and paints somewhere it should not. That was drawing stripes
across the display, worst when an enemy was on screen, since enemies enter from
above and so are drawn at negative y on the way in. Everything in `draw.c` now
goes through the clipping helpers at the top of the file.
