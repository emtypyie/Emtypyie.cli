#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdbool.h>
#include "theme.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Effects / parallel animation engine ───
 * Renders rich terminal "rice" art (box-drawing, block chars, gradients)
 * using the active Theme palette. Frame computation runs on a small thread
 * pool so the main draw thread never blocks — this is the "parallel
 * computing" layer that keeps the UI responsive while heavy frames bake.
 *
 * A frame is a malloc'd array of strings (rows), each already wrapped in
 * ANSI theme colors. The producer threads fill a ring of frame slots;
 * the player blits the latest ready frame on a timer.
 */

typedef struct {
    char **rows;     /* NULL-terminated array of malloc'd row strings */
    int    nrows;
    int    width;    /* max display width in cells */
} EffectFrame;

/* Callback that produces frame `index` (0-based, monotonic). The callback
 * must return a heap-allocated EffectFrame (use effects_frame_free). When
 * the animation is exhausted, return NULL. */
typedef EffectFrame* (*EffectProducer)(int index, void *user);

/* Play an animation.
 *  - producer: generates successive frames
 *  - total_frames: number of frames, or -1 for infinite (plays until stop)
 *  - fps: frames per second (e.g. 20)
 *  - workers: thread count for parallel frame baking (1..8; clamps)
 *  - user: opaque data passed to producer
 * Returns when the animation completes or is interrupted (Ctrl-C / key).
 * Honors EMTYPYIE_NO_ANIM / --no-animation via effects_skip(). */
void effects_play(EffectProducer producer, int total_frames, int fps,
                  int workers, void *user);

/* True if animations should be skipped (env / flag). */
bool effects_skip(void);

/* Allocate a frame with `nrows` empty rows (caller fills rows). */
EffectFrame* effects_frame_new(int nrows, int width);

/* Free a frame and its rows. */
void effects_frame_free(EffectFrame *f);

/* Clear current line region of `nrows` (ANSI cursor up + erase). */
void effects_erase(int nrows);

/* ─── Baking Bread art ───
 * The signature v3.0.2 about-art. Returns a static (not owned) frame the
 * caller may render directly, or pass to the player for a shimmer loop. */
EffectFrame* effects_baking_bread_frame(int index, void *user);

/* Number of shimmer frames for the baking-bread loop (for infinite play). */
int effects_baking_bread_frames(void);

#ifdef __cplusplus
}
#endif

#endif
