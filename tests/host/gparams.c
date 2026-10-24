/* SPDX-License-Identifier: GPL-3.0-only */
/* Tool (not in the suite): every parameter of the engine as JSON for the web editor (tools/make_params.py): its name, range, default, and, where
 * the values have names (a sound, a mode, a rate), the name of each. gparams > params_raw.json */
#include <stdio.h>
#include <string.h>
#include "amb.h"

void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }

/* the whole name of a sound (the screen's text is cut to ten characters) */
static const char *full_name(int p, int v)
{
    switch (p) {
    case P_PADTONE: return v >= 0 && v < EN_NSOUND ? EN_PAD_SOUND[v] : 0;
    case P_DR_INST: return v >= 0 && v < EN_NSOUND ? EN_DRONE_SOUND[v] : 0;
    case P_AC_WAVE: return v >= 0 && v < EN_NACID ? EN_ACID_SOUND[v] : 0;
    case P_CM_SND: return v >= 0 && v < EN_NCMSND ? EN_CM_SOUND[v] : 0;
    case P_KIT_KICK: case P_KIT_SNARE: case P_KIT_HAT: case P_KIT_PERC: return v >= 0 && v < EN_NKIT ? EN_KIT_NAME[v] : 0;
    default: return 0;
    }
}

static void put_str(const char *s)
{
    putchar('"');
    for (; *s; s++)
        putchar(*s == '"' || *s == '\\' ? '\'' : *s);
    putchar('"');
}

int main(void)
{
    int p, v;
    en_init();
    printf("[\n");
    for (p = 0; p < P_NPARAMS; p++) {
        const en_param_t *q = en_param_info(p);
        int named = 0;
        char t[16], num[16];
        printf("%s{\"i\":%d,\"n\":", p ? ",\n" : "", p);
        put_str(q->name);
        printf(",\"min\":%d,\"max\":%d,\"def\":%d", q->lo, q->hi, q->def);
        if (q->hi - q->lo <= 70) {
            for (v = q->lo; v <= q->hi && !named; v++) {
                en_param_text(p, v, t);
                snprintf(num, sizeof num, "%d", v);
                if (full_name(p, v) || (strcmp(t, num) && t[0]))
                    named = 1;
            }
        }
        if (named) {
            printf(",\"t\":[");
            for (v = q->lo; v <= q->hi; v++) {
                const char *full = full_name(p, v);
                en_param_text(p, v, t);
                if (v > q->lo)
                    putchar(',');
                put_str(full ? full : t);
            }
            printf("]");
        }
        printf("}");
    }
    printf("\n]\n");
    return 0;
}
