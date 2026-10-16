/* SPDX-License-Identifier: GPL-3.0-only */
/* LOF-1 app: the project (everything that is saved) and the app's entry points. Included by the unity
 * build after gfx.c (drawing helpers in scope) and by the host simulator. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/amb.h"

#ifndef EN_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION "DEV"
#endif
#define PROJ_MAGIC 0x314F464Cu           /* "LOF1" */
#define PROJ_FORMAT 4u                   /* 2: Daydream 2; 3: ten presets; 4: Groove 2 */
#define NPIECES EN_NPIECES

typedef struct {
    int16_t par[P_NPARAMS];
} slot_t;

typedef struct {
    uint32_t magic, format;
    int16_t par[P_NPARAMS];              /* the tape playing, as it is now (amb.h P_*) */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t piece;                       /* which preset, 0-9 (01-10) */
    uint8_t rsv0[2];
    uint16_t pats[EN_NPAT][EN_NLANE];    /* the drum patterns (shared by the tapes) */
    slot_t slot[NPIECES];                /* every tape as it was left (Tune and MIDI are global) */
    uint8_t rsv[16];                     /* room to grow */
} project_t;

extern project_t proj;

void project_defaults(void);
int project_load(void);                  /* 0 = loaded; else defaults are in place */
int project_save(void);                  /* 0 = saved and verified */
void project_apply(void);                /* every value to the engine (boot, load) */
const char *piece_name(int i);
void project_piece(int i);               /* the playing piece kept, piece i taken up as it was left */
void project_piece_reset(void);          /* the playing piece back to its recipe */

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);
