#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The firmware dropdown on every page of the site (the workflow runs this last, over build/pages):
HYP-1's pages at the root, LOF-1's under lof/, ECO-1's under eco/, GRAV-1's under grav/. Choosing another firmware goes to its
matching page (landing, emulator, installer, manual; HYP-1's board has no twin, so the landing), in its own
colours, text and images. A firmware that didn't build this time isn't offered. The bar is the same neutral strip on both, so it reads as the site's own.
The front page (tools/site/hub.py, at the root, HYP-1's landing under hyp/) has its own picker, so it gets no bar;
every other page's bar starts with "All firmwares" to go back to it.

  python3 tools/site/switcher.py build/pages"""
import sys
from pathlib import Path

PRODUCTS = [("hyp", "HYP-1", "hypnotic techno and acid", ""), ("lof", "LOF-1", "a lofi groovebox", "lof/"),
            ("eco", "ECO-1", "a plant plays it", "eco/"), ("grav", "GRAV-1", "a star and its planets", "grav/")]
EXPERIMENTAL = {"lof", "eco", "grav"}
SECTIONS = ("emu", "install", "manual", "board")

BAR = """<style>
.rw-switch { display: flex; flex-wrap: wrap; align-items: center; gap: .3rem .7rem; box-sizing: border-box;
             max-width: 1040px; margin: 0 auto 18px; padding: 7px 12px; border-radius: 10px;
             background: #0b0b0d; border: 1px solid #2c2c33; color: #b9b9c3;
             font: 500 15px/1.3 "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif; letter-spacing: .03em; }
.rw-switch label { display: inline-flex; align-items: center; gap: .5rem; }
.rw-switch select { font: 600 15px "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif; color: #f2f2f4;
                    background: #1c1c22; border: 1px solid #44444e; border-radius: 6px; padding: .2em .5em; max-width: 100%; }
.rw-switch select:focus-visible { outline: 2px solid #e8873a; outline-offset: 2px; }
.rw-switch a.rw-all { display: inline-flex; align-items: center; gap: .35em; min-height: 32px; padding: .15em .8em; border-radius: 6px;
                     background: #e8873a; color: #1a0f05; font-weight: 700; text-decoration: none; }
.rw-switch a.rw-all:hover, .rw-switch a.rw-all:focus-visible { background: #ffa25a; outline: 2px solid #ffa25a; outline-offset: 2px; }
.rw-switch a.rw-open1 { margin-left: auto; display: inline-flex; align-items: center; gap: .4em; min-height: 32px; padding: .15em .8em;
                       border-radius: 6px; border: 1px solid #e8873a; color: #ffb27a; font-weight: 700; text-decoration: none; }
.rw-switch a.rw-open1 small { font: 600 11px "Barlow Semi Condensed", sans-serif; letter-spacing: .1em; text-transform: uppercase; color: #b9b9c3; }
.rw-switch a.rw-open1:hover, .rw-switch a.rw-open1:focus-visible { background: #2a1b10; outline: 2px solid #e8873a; outline-offset: 2px; }
.rw-switch .tag { font-weight: 700; font-size: 12px; letter-spacing: .1em; text-transform: uppercase; color: #1a0f05;
                  background: #e8873a; border-radius: 999px; padding: .1em .6em; }
</style>
<nav class="rw-switch" aria-label="Firmware">__ALL__<label for="rw-fw">FM-1 firmware by RedtailWorks</label>
<select id="rw-fw" onchange="location.href=this.value">__OPTS__</select>__TAG____OPEN1__</nav>
"""


def page_parts(rel):
    """(product, section, depth to the site root) of a page's path under the site"""
    parts = rel.parts
    prod = parts[0] if parts[0] in EXPERIMENTAL and len(parts) > 1 else "hyp"
    rest = parts[1:] if prod != "hyp" else parts
    section = rest[0] if len(rest) > 1 and rest[0] in SECTIONS else ""
    return prod, section, len(parts) - 1


def main(root):
    root = Path(root)
    n = 0
    built = {key for key, *_ in PRODUCTS if key == "hyp" or (root / key / "index.html").exists()}
    for idx in (root / "index.html", root / "hyp" / "index.html"):
        if idx.exists():                                   # a firmware that didn't build: no word of it on the landings
            t = idx.read_text(encoding="utf-8")
            for key in EXPERIMENTAL - built:
                while (a := t.find(f"<!--{key}-->")) >= 0 and (b := t.find(f"<!--/{key}-->", a)) > a:
                    t = t[:a] + t[b + len(f"<!--/{key}-->"):]
            idx.write_text(t, encoding="utf-8")
    open1 = (root / "open1" / "index.html").exists()       # OPEN-1's page (tools/site/open1.py): a link to it on every bar
    hub = (root / "index.html").exists() and "<!--rw-hub-->" in (root / "index.html").read_text(encoding="utf-8")
    for f in sorted(root.rglob("*.html")):
        rel = f.relative_to(root)
        if hub and rel == Path("index.html"):
            continue                                       # the front page has its own picker
        if rel.parts[0] == "open1":
            continue                                       # OPEN-1's page isn't a firmware (tools/site/open1.py)
        prod, section, depth = page_parts(rel)
        if prod not in built:
            continue
        page = f.read_text(encoding="utf-8")
        up = "" if '<base href="../">' in page else "../" * depth   # HYP-1's landing in hyp/ resolves from the root
        opts = [f'<option value="{up or "./"}">All firmwares · the front page</option>'] if hub else []
        for key, name, what, base in PRODUCTS:
            if key not in built:
                continue
            sec = section if section != "board" or key == "hyp" else ""
            if key != "hyp" and sec and not (root / key / sec).exists():
                sec = ""
            if hub and key == "hyp" and not sec:
                href = up + "hyp/"                         # HYP-1's landing, now that the root is the front page
            else:
                href = (up + base + (sec + "/" if sec else "")) or "./"
            label = f"{name} · {what}" + (" (experimental)" if key in EXPERIMENTAL else "")
            opts.append(f'<option value="{href}"{" selected" if key == prod else ""}>{label}</option>')
        if len(opts) < 2 + hub:
            continue
        home = f'<a class="rw-all" href="{up or "./"}"><span aria-hidden="true">&larr;</span> All firmwares</a>' if hub else ""
        bar = BAR.replace("__ALL__", home).replace("__OPTS__", "".join(opts)).replace(
            "__TAG__", '<span class="tag">Experimental</span>' if prod in EXPERIMENTAL else "").replace(
            "__OPEN1__", f'<a class="rw-open1" href="{up}open1/">OPEN-1 <small>what\'s next</small></a>' if open1 else "")
        s = page
        if "rw-switch" in s:
            continue
        i = s.find("<body")
        if i < 0:
            continue
        j = s.index(">", i) + 1
        f.write_text(s[:j] + "\n" + bar + s[j:], encoding="utf-8")
        n += 1
    print(f"switcher: {n} pages")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
