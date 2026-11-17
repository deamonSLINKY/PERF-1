#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/format_fixture.sh, for a release whose host has no dumpobj (0.15.x): the command as later releases have it, put in before "seed"."""
import sys

p = sys.argv[1]
s = open(p, encoding='utf-8', newline='').read()
a = '} else if (!strcmp(cmd, "seed")) {'
assert s.count(a) == 1, 'no "seed" command to put dumpobj before'
s = s.replace(a, '''} else if (!strcmp(cmd, "dumpobj")) {
            static uint8_t tmp[PLAT_STORE_MAX];
            FILE *fo = fopen(b, "wb");
            if (fo) {
                const int n = plat_store_load((uint32_t)atoi(a), tmp, sizeof tmp);
                if (n > 0)
                    fwrite(tmp, 1, (size_t)n, fo);
                fclose(fo);
            }
        ''' + a)
open(p, 'w', encoding='utf-8', newline='').write(s)
