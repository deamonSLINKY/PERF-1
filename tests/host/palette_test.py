# SPDX-License-Identifier: GPL-3.0-only
# (0.17.40) The screen's themes, as the panel shows them (RGB565): the ground, the panel, the lines and the dim
# text must stay apart (one step in two channels, or two in one; green counted in red/blue-sized steps), and each
# layer's dark tint must sit below its colour and lean no further than 25 degrees from it.
#   python3 tests/host/palette_test.py
import colorsys, os, re, sys

SRC = os.path.join(os.path.dirname(__file__), '..', '..', 'firmware', 'src', 'app', 'ui.c')
NAMES = ['bg', 'panel', 'line', 'dim', 'text', 'acc', 'pad', 'pad_t', 'acid', 'acid_t', 'bass', 'bass_t',
         'pulse', 'pulse_t', 'frost', 'pluck']
src = open(SRC, encoding='utf-8').read()
i = src.index('static const theme_t THEMES[EN_NTHEME] = {')
body = src[i:src.index('};', i)]
themes = [[tuple(int(v) for v in m) for m in re.findall(r'RGB\((\d+),\s*(\d+),\s*(\d+)\)', b)]
          for b in re.findall(r'\{(RGB\([^}]*)\}', body)]
bad = 0


def q(c):
    return (c[0] >> 3, (c[1] >> 2) // 2, c[2] >> 3)


def apart(a, b):
    d = [abs(x - y) for x, y in zip(q(a), q(b))]
    return sum(1 for v in d if v >= 1) >= 2 or max(d) >= 2


def hls(c):
    return colorsys.rgb_to_hls(*(v / 255 for v in c))


for n, cs in enumerate(themes):
    if len(cs) != len(NAMES):
        print('theme', n, 'has', len(cs), 'colours'); bad = 1; continue
    t = dict(zip(NAMES, cs))
    for a, b in (('bg', 'panel'), ('panel', 'line'), ('line', 'dim'), ('dim', 'text')):
        if not apart(t[a], t[b]):
            print(f'theme {n}: {a} {t[a]} and {b} {t[b]} are one colour on the panel'); bad = 1
    for v in ('pad', 'acid', 'bass', 'pulse'):
        hb, lb, _ = hls(t[v]); ht, lt, _ = hls(t[v + '_t'])
        dh = abs((hb - ht) * 360 + 540) % 360 - 180
        if lt >= lb or abs(dh) > 25:
            print(f'theme {n}: {v}_t {t[v + "_t"]} against {v} {t[v]}: lightness {lt:.2f}/{lb:.2f}, hue {dh:+.0f}'); bad = 1
print(f'{len(themes)} themes', 'ok' if not bad else 'FAILED')
sys.exit(bad)
