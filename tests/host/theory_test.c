/* SPDX-License-Identifier: GPL-3.0-only */
/* THEORY's spelling (firmware/src/app/theory.h) against the keys a player reads off sheet music: every mode's notes with the key's
 * letters, the key signature, borrowed notes (minor's raised seventh), the circle's labels. The mockup's vectors and more. */
#include <stdio.h>
#include <string.h>
#include "../../firmware/src/app/theory.h"

static int bad, n;
static void check(const char *what, const char *got, const char *want)
{
    n++;
    if (strcmp(got, want)) {
        bad++;
        printf("FAIL %s: got \"%s\", want \"%s\"\n", what, got, want);
    }
}
static uint32_t scale(int root, const int *iv, int m)
{
    uint32_t p = 0;
    int i;
    for (i = 0; i < m; i++)
        p |= 1u << ((root + iv[i]) % 12);
    return p;
}
static const int ION[7] = {0, 2, 4, 5, 7, 9, 11}, DOR[7] = {0, 2, 3, 5, 7, 9, 10}, PHR[7] = {0, 1, 3, 5, 7, 8, 10},
                 LYD[7] = {0, 2, 4, 6, 7, 9, 11}, MIX[7] = {0, 2, 4, 5, 7, 9, 10}, AEO[7] = {0, 2, 3, 5, 7, 8, 10},
                 PMAJ[5] = {0, 2, 4, 7, 9}, PMIN[5] = {0, 3, 5, 7, 10};

/* the scale spelled from its root, and the signature */
static void key(const char *what, int root, int flat, const int *iv, int m, const char *want_notes, int want_sig)
{
    th_key_t k;
    char b[64], *o = b, s[8];
    int i;
    th_key(scale(root, iv, m), root, flat, &k);
    for (i = 0; i < m; i++) {
        if (i)
            *o++ = ' ';
        th_spell(&k, (root + iv[i]) % 12, o);
        o += strlen(o);
    }
    check(what, b, want_notes);
    th_sig_text(k.sig, s);
    {
        char w[8];
        th_sig_text(want_sig, w);
        char t[80];
        snprintf(t, sizeof t, "%s: signature", what);
        check(t, s, w);
    }
}

int main(void)
{
    th_key_t k;
    char b[16];
    key("C Ionian", 0, 0, ION, 7, "C D E F G A B", 0);
    key("E Ionian (the old table said Ab)", 4, 0, ION, 7, "E F# G# A B C# D#", 4);
    key("F# Ionian", 6, 0, ION, 7, "F# G# A# B C# D# E#", 6);
    key("Db Ionian", 1, 0, ION, 7, "Db Eb F Gb Ab Bb C", -5);
    key("B Ionian", 11, 0, ION, 7, "B C# D# E F# G# A#", 5);
    key("A Aeolian", 9, 0, AEO, 7, "A B C D E F G", 0);
    key("C Aeolian", 0, 0, AEO, 7, "C D Eb F G Ab Bb", -3);
    key("Eb Aeolian (lofi 07)", 3, 1, AEO, 7, "Eb F Gb Ab Bb Cb Db", -6);
    key("G# Aeolian, not Ab (7 flats)", 8, 1, AEO, 7, "G# A# B C# D# E F#", 5);
    key("F# Aeolian", 6, 0, AEO, 7, "F# G# A B C# D E", 3);
    key("D Dorian", 2, 0, DOR, 7, "D E F G A B C", 0);
    key("E Phrygian", 4, 0, PHR, 7, "E F G A B C D", 0);
    key("C# Phrygian (in A major)", 1, 0, PHR, 7, "C# D E F# G# A B", 3);
    key("F Lydian", 5, 0, LYD, 7, "F G A B C D E", 0);
    key("Bb Lydian (in F major)", 10, 1, LYD, 7, "Bb C D E F G A", -1);
    key("G Mixolydian", 7, 0, MIX, 7, "G A B C D E F", 0);
    key("A major pentatonic (in A major)", 9, 0, PMAJ, 5, "A B C# E F#", 3);
    key("E minor pentatonic (in G major)", 4, 0, PMIN, 5, "E G A B D", 1);

    /* borrowed: minor's raised seventh on V */
    th_key(scale(9, AEO, 7), 9, 0, &k);
    check("A minor: G# borrowed", (th_spell(&k, 8, b), b), "G#");
    n++, bad += th_spell(&k, 8, b) != 1;
    {
        char e7[32] = "", t[8];
        const int pcs[4] = {4, 8, 11, 2};
        int i;
        for (i = 0; i < 4; i++) {
            th_spell(&k, pcs[i], t);
            if (i)
                strcat(e7, " ");
            strcat(e7, t);
        }
        check("A minor, V7: E G# B D", e7, "E G# B D");
    }
    th_key(scale(0, AEO, 7), 0, 0, &k);
    check("C minor's raised seventh: B natural", (th_spell(&k, 11, b), b), "B");
    th_key(scale(3, AEO, 7), 3, 1, &k);
    check("Eb minor's raised seventh: D natural", (th_spell(&k, 2, b), b), "D");
    th_key(scale(8, AEO, 7), 8, 1, &k);
    check("G# minor's raised seventh: F##", (th_spell(&k, 7, b), b), "F##");

    /* the circle's labels */
    th_key(scale(4, ION, 7), 4, 0, &k);
    check("E major's circle: Bb outside", (th_label(&k, 10, b), b), "Bb");
    check("E major's circle: G# inside", (th_label(&k, 8, b), b), "G#");
    check("E major's circle: F# (sharp side)", (th_label(&k, 6, b), b), "F#");
    th_key(scale(5, ION, 7), 5, 0, &k);
    check("F major's circle: Gb (flat side)", (th_label(&k, 6, b), b), "Gb");
    check("F major's circle: Bb inside", (th_label(&k, 10, b), b), "Bb");
    th_key(scale(3, AEO, 7), 3, 1, &k);
    check("Eb minor's circle: Cb inside", (th_label(&k, 11, b), b), "Cb");

    /* letters, for the numerals */
    th_key(scale(9, AEO, 7), 9, 0, &k);
    n++, bad += th_letter(&k, 4) - th_letter(&k, 9) != -1 && (th_letter(&k, 4) - th_letter(&k, 9) + 7) % 7 != 4;
    if (bad) {
        printf("theory_test: %d of %d failed\n", bad, n);
        return 1;
    }
    printf("theory_test: %d checks passed\n", n);
    return 0;
}
