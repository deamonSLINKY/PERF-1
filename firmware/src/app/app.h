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
    uint8_t mod[EN_NLANE][16];           /* its step tricks (en_mod): a lane's ratchet, push and level on each of its steps */
    uint8_t acl[18];                     /* the acid line as it was when the piece was left (used when the piece's acid is frozen) */
} slot_t;

typedef struct {
    int16_t par[P_NPARAMS];              /* the piece playing, as it is now (amb.h P_*); its patterns are en_pats */
    uint16_t pool;                       /* its scale, relative to the root (bit 0 = the root) */
    uint8_t acl[18];                     /* the piece's frozen acid line (a slot's, while it plays) */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t piece;                       /* which piece */
    uint8_t genre;                       /* which bank of ten pieces (EN_NGENRE): the original, lofi; the others are factory sets */
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
void project_set_genre(int g);           /* another bank: the pieces as they were left in it (or their recipes), the same number playing */
void project_piece_save(void);           /* the playing piece, as it is, into the bank */
void project_piece_factory(void);        /* the playing piece back to its recipe, and out of the bank */
void project_globals(void);              /* the theme, Tune or MIDI out changed: into the bank */
int project_saved(int i);                /* piece i is in the bank */
int project_pending(void);               /* the bank has changes not yet in flash */
int project_flush(void);                 /* and into flash (call while stopped): 0 = written */
int project_pending_globals(void);       /* the theme, Tune or MIDI out changed, not yet in flash */
int project_flush_globals(void);         /* those alone into flash (while playing too): 0 = written */

void ui_init(void);
int ui_restore_valid(uint32_t obj, const uint8_t *b, uint32_t len);   /* a backup's object, as its loader would take it (1) */
void ui_restore_start(uint32_t obj, const uint8_t *b, uint32_t len);    /* write it (the music fades out for it) */
int ui_restore_state(void);              /* 0 none, 1 waiting, 2 written, 3 not written */
void ui_flush(void);                    /* before an update session: a store or autosave in flight written now, the fade undone */
void ui_frame(void);
void ui_input_only(void);
int ui_remote(void);                   /* remote (SEL + GLO): the panel plays the browser over USB */
extern uint32_t ui_splash_ms;            /* the RedtailWorks card's time at power-on (0: none) */
int ui_dirty(void);
void ui_say(const char *a, const char *b);
void ui_param_set(int p, int v);                 /* the editor: a parameter, as its knob sets it */
int ui_param_get(int p);
void ui_voices_changed(int n, int kind);   /* the player's DX7 voices changed (main_fm1.c dx_service): say so, and keep them (kind 1 bulk, 2 single, 3 one slot, 4 cleared; n: the voices taken, -1 a bad checksum, -2 not taken) */
