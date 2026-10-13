/* SPDX-License-Identifier: GPL-3.0-only */
/* HYP-1 app: the project (the pieces as they are while the power is on; the bank, saved) and the app's entry points. Included by the unity
 * build after gfx.c (drawing helpers in scope) and by the host simulator. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/amb.h"

#ifndef EN_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION "DEV"
#endif
#define NPIECES EN_NPIECES

typedef struct {
    int16_t par[P_NPARAMS];
    uint16_t pool;
    uint16_t pats[EN_NPAT][EN_NLANE];    /* its six drum patterns */
} slot_t;

typedef struct {
    int16_t par[P_NPARAMS];              /* the piece playing, as it is now (amb.h P_*); its patterns are en_pats */
    uint16_t pool;                       /* its scale, relative to the root (bit 0 = the root) */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t piece;                       /* which piece */
    uint16_t edited;                     /* bit i: piece i changed since it was taken up (saved or not) */
    slot_t slot[NPIECES];                /* every piece as it was left while the power is on */
} project_t;

extern project_t proj;

/* Power-on: each piece as it was saved (Global, SAVE), or its recipe if never saved; a piece at
 * random, nudged a little if it was never saved. Nothing changes the flash but SAVE (and the theme,
 * Tune, MIDI out, which save themselves): the bank is written while stopped. */
void project_start(uint32_t seed);       /* power-on (seed 0: the first piece, exact) */
void project_apply(void);                /* every value to the engine (boot, a piece) */
const char *piece_name(int i);           /* "01" .. "10" */
void project_piece(int i);               /* the playing piece kept as it is, piece i taken up as it was left */
void project_piece_save(void);           /* the playing piece, as it is, into the bank */
void project_piece_factory(void);        /* the playing piece back to its recipe, and out of the bank */
void project_globals(void);              /* the theme, Tune or MIDI out changed: into the bank */
int project_saved(int i);                /* piece i is in the bank */
int project_pending(void);               /* the bank has changes not yet in flash */
int project_flush(void);                 /* and into flash (call while stopped): 0 = written */
int project_pending_globals(void);       /* the theme, Tune or MIDI out changed, not yet in flash */
int project_flush_globals(void);         /* those alone into flash (while playing too): 0 = written */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_remote(void);                   /* remote (SEL + GLO): the panel plays the browser over USB */
extern uint32_t ui_splash_ms;            /* the RedtailWorks card's time at power-on (0: none) */
int ui_dirty(void);
void ui_say(const char *a, const char *b);
