#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""PERF-1's web site (for GitHub Pages or any static host) from a release build:

  python3 tools/make_pages_site.py VERSION OUTDIR      (after tools/release.sh VERSION)

OUTDIR gets index.html (the landing page), install/ (the web installer with the package in it),
emu/ (the browser emulator), editor/ (the parameter and DX7 voice editor), manual/ and firmware/perf1-VERSION.fwsc.
All links are relative, so it works at any path (https://USER.github.io/PERF-1/ and a local server alike)."""
import json
import re
import shutil
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402

ver = sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__)
OUT = Path(sys.argv[2] if len(sys.argv) > 2 else SRC / "build" / "site")
REPO = "https://github.com/deamonSLINKY/PERF-1"
pkg = SRC / "build" / f"amb-{ver}.fwsc"
raw = pkg.read_bytes()
product = product_of(raw)
if not re.fullmatch(r"FM-1_7\d{6}", product):
    sys.exit(f"{pkg}: identity {product!r}: make a release build (tools/release.sh {ver})")
if b"FELUCCA-LOADER-1" not in raw:
    sys.exit(f"{pkg}: no update loader in it")
if OUT.exists():
    for old in OUT.iterdir():
        shutil.rmtree(old) if old.is_dir() else old.unlink()
for d in ("install", "firmware", "img", "manual/img"):
    (OUT / d).mkdir(parents=True, exist_ok=True)
(OUT / ".nojekyll").write_text("", encoding="utf-8")
name = f"perf1-{ver}.fwsc"
shutil.copy(pkg, OUT / "firmware" / name)
shutil.copytree(SRC / "build" / "emu", OUT / "emu")
shutil.copy(SRC / "web" / "img" / "slinky.png", OUT / "img" / "slinky.png")
shutil.copy(SRC / "web" / "img" / "favicon.png", OUT / "img" / "favicon.png")

page = (SRC / "web" / "amb_installer.html").read_text(encoding="utf-8")
lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8"))
meta = json.dumps({"version": ver, "product": product, "pkg": "../firmware/" + name})
page = page.replace("/*LIB*/", lib).replace("/*META*/", meta).replace("<h1>Install PERF-1</h1>", f"<h1>Install PERF-1 {ver}</h1>")
(OUT / "install" / "index.html").write_text(page, encoding="utf-8", newline="\n")

params = SRC / "build" / "params.json"
if params.exists():
    (OUT / "editor").mkdir(exist_ok=True)
    ed = (SRC / "web" / "editor" / "index.html").read_text(encoding="utf-8").replace("/*PARAMS*/", params.read_text(encoding="utf-8"))
    (OUT / "editor" / "index.html").write_text(ed, encoding="utf-8", newline="\n")
shutil.copy(SRC / "docs" / "manual.html", OUT / "manual" / "index.html")
for img in (SRC / "docs" / "img").glob("perf-screen-*.png"):
    shutil.copy(img, OUT / "manual" / "img" / img.name)

(OUT / "index.html").write_text(f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PERF-1</title><link rel="icon" href="img/favicon.png">
<meta name="description" content="PERF-1: live-performance firmware for the M-VAVE FM-1: ten genres of generative pieces, an auto-DJ, effects, a web editor and DX7 voices.">
<style>
:root{{--bg:#140e0c;--panel:#2a1e1b;--line:#4e3831;--text:#f4e8c8;--dim:#a68c7e;--acc:#ffd447;--red:#e0344f}}
@media (prefers-color-scheme:light){{:root{{--bg:#f6efe2;--panel:#ebe0cc;--line:#cdbb9f;--text:#2a1c16;--dim:#7a6657;--acc:#a86b00;--red:#b3203a}}}}
*{{box-sizing:border-box}}body{{margin:0;background:var(--bg);color:var(--text);font:16px/1.55 system-ui,"Segoe UI",sans-serif}}
main{{max-width:760px;margin:0 auto;padding:40px 16px 56px}}
img.logo{{width:96px;height:auto;display:block;margin:0 0 12px}}h1{{margin:0;font-size:2.4rem;letter-spacing:.02em}}h1 span{{color:var(--red)}}
.tag{{color:var(--dim);margin:.2rem 0 1.6rem}}.v{{color:var(--acc);font-weight:600}}
.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px;margin:0 0 28px}}
.card{{display:block;background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:14px 16px;color:inherit;text-decoration:none}}
.card:hover{{border-color:var(--acc)}}.card b{{display:block;font-size:1.05rem;color:var(--acc)}}.card span{{color:var(--dim);font-size:.92rem}}
p.small{{color:var(--dim);font-size:.88rem}}a{{color:var(--acc)}}
</style></head><body><main>
<img class="logo" src="img/slinky.png" alt="the PERF-1 slinky">
<h1>PERF<span>-1</span></h1>
<p class="tag">Custom live-performance firmware for the M-VAVE FM-1, by deamonSLINKY. Version <span class="v">{ver}</span>.</p>
<p>Ten genres of ten generative pieces (pad, drone, acid, a pluck or DX7 voice, drums you can sequence), an auto-DJ with breakdowns, builds and drops, 27 effects, 24 beat FX, 15 flex beats and 27 DJ snips, scenes, a web editor and your own DX7 voices over USB.</p>
<div class="grid">
<a class="card" href="install/"><b>Install</b><span>The web installer (Chrome or Edge, FM-1 on USB). It can put M-VAVE's own firmware back.</span></a>
<a class="card" href="emu/"><b>Try it in your browser</b><span>The same code the FM-1 runs, with sound. No FM-1 needed.</span></a>
<a class="card" href="editor/"><b>Editor</b><span>Every parameter, and a DX7 voice editor with live preview on the unit.</span></a>
<a class="card" href="manual/"><b>Manual</b><span>Every control, page and setting.</span></a>
<a class="card" href="firmware/{name}"><b>Download the package</b><span>{name} ({len(raw) // 1024} KB), for the installer or tools/fm1_install.py.</span></a>
<a class="card" href="{REPO}/issues/new/choose"><b>Feedback and ideas</b><span>Comments, suggestions, ideas and bug reports: open an issue. Everyone is welcome to help.</span></a>
<a class="card" href="{REPO}"><b>Source on GitHub</b><span>GPL-3.0. Read it, build it, send changes.</span></a>
</div>
<p class="small">Installing firmware is at your own risk, though the installer can restore the stock firmware and an FM-1 that fails to start twice comes up in a safe mode the installer can reach. Read the manual's section 1 first.</p>
<p class="small">PERF-1 is a fork of HYP-1 by <a href="https://www.redtail.works">RedtailWorks</a>, itself a fork of AMB-1 by Charles Vestal, built on Felucca by Leo Kuroshita (Hügelton Instruments); the six-operator FM engine is Melodee's by Kerem Kilic (Ellic Studio), after Dexed. GPL-3.0-only; see LICENSING.md in the repository. "M-VAVE" and "FM-1" are trademarks of their owners: PERF-1 is independent firmware, not affiliated with or endorsed by them.</p>
</main></body></html>
''', encoding="utf-8", newline="\n")
print(f"site: {OUT}: index, install/, emu/, editor/, manual/, firmware/{name} ({len(raw)} B, {product})")
