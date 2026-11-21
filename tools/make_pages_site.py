#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""PERF-1's web site (for GitHub Pages or any static host) from a release build:

  python3 tools/make_pages_site.py VERSION OUTDIR      (after tools/release.sh VERSION)

OUTDIR gets index.html (the landing page), install/ (the web installer with the package in it),
emu/ (the browser emulator), studio/ (a piece over its bars, played in the emulator), editor/ (the parameter and DX7 voice editor), pieces/ and library/ (sharing), manual/ and firmware/perf1-VERSION.fwsc.
  --no-sharing   leaves out pieces/ and library/ and their links (a release whose sharing pages are not ready: 0.16.1)
All links are relative, so it works at any path (https://USER.github.io/PERF-1/ and a local server alike)."""
import json
import re
import shutil
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SRC / "web"))
from make_site import product_of, strip_module  # noqa: E402

SHARING = "--no-sharing" not in sys.argv
args = [a for a in sys.argv[1:] if a != "--no-sharing"]
ver = args[0] if args else sys.exit(__doc__)
OUT = Path(args[1] if len(args) > 1 else SRC / "build" / "site")
REPO = "https://github.com/deamonSLINKY/PERF-1"
pkg = SRC / "build" / f"perf1-{ver}.fwsc"
if not pkg.exists():
    pkg = SRC / "build" / f"amb-{ver}.fwsc"   # (before 0.17.55)
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
shutil.copy(SRC / "web" / "img" / "banner.png", OUT / "img" / "banner.png")   # (1.0) the banner (the maker's festival picture)

page = (SRC / "web" / "amb_installer.html").read_text(encoding="utf-8")
lib = strip_module((SRC / "web" / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + strip_module((SRC / "web" / "fm1ota.js").read_text(encoding="utf-8")) + "\n" + strip_module((SRC / "web" / "fm1backup.js").read_text(encoding="utf-8")) + "\n" + strip_module((SRC / "web" / "fm1idle.js").read_text(encoding="utf-8")) + "\n" + strip_module((SRC / "web" / "fm1kit.js").read_text(encoding="utf-8"))
meta = json.dumps({"version": ver, "product": product, "pkg": "../firmware/" + name})
page = page.replace("/*LIB*/", lib).replace("/*META*/", meta).replace("<h1>Install PERF-1</h1>", f"<h1>Install PERF-1 {ver}</h1>")
(OUT / "install" / "index.html").write_text(page, encoding="utf-8", newline="\n")

params = SRC / "build" / "params.json"
if params.exists():
    (OUT / "editor").mkdir(exist_ok=True)
    ed = (SRC / "web" / "editor" / "index.html").read_text(encoding="utf-8").replace("/*PARAMS*/", params.read_text(encoding="utf-8"))
    (OUT / "editor" / "index.html").write_text(ed, encoding="utf-8", newline="\n")
    plib = "\n".join(strip_module((SRC / "web" / f).read_text(encoding="utf-8")) for f in ("fm1backup.js", "fm1piece.js", "fm1card.js", "fm1share.js", "fm1idle.js"))
    lib_json = (SRC / "web" / "library" / "library.json").read_text(encoding="utf-8")
    relay = SRC / "web" / "relay" / "relay.json"           # the library relay's address (web/relay/README.md), or none
    relay_json = relay.read_text(encoding="utf-8").strip() if relay.exists() else "{}"
    for d in ("pieces", "library") if SHARING else ():   # sharing pieces and sets, and the library of them: the modules, the parameters' names, the library's codes
        (OUT / d).mkdir(exist_ok=True)
        pg = (SRC / "web" / d / "index.html").read_text(encoding="utf-8").replace("/*PARAMS*/", params.read_text(encoding="utf-8")).replace("/*LIB*/", plib)
        (OUT / d / "index.html").write_text(pg.replace("/*LIBRARY*/", lib_json).replace("/*RELAY*/", relay_json), encoding="utf-8", newline="\n")
    studio = SRC / "web" / "studio" / "index.html"
    if studio.exists():                           # (1.0) the studio: a piece over its bars, edited, played in the emulator (../emu/); a static page, no special headers
        sys.path.insert(0, str(SRC / "tools"))
        from make_params import param_enum       # noqa: E402  (every P_ name: the studio reads a piece's settings by name)
        (OUT / "studio").mkdir(exist_ok=True)
        slib = plib + "\n" + "\n".join(strip_module((SRC / "web" / f).read_text(encoding="utf-8")) for f in ("fm1midi.js", "fm1link.js"))
        pg = studio.read_text(encoding="utf-8").replace("/*PARAMS*/", params.read_text(encoding="utf-8")).replace("/*LIB*/", slib)
        (OUT / "studio" / "index.html").write_text(pg.replace("/*PENUM*/", json.dumps(param_enum())), encoding="utf-8", newline="\n")
shutil.copy(SRC / "docs" / "manual.html", OUT / "manual" / "index.html")
(OUT / "manual" / "maps").mkdir(exist_ok=True)         # ready MIDI maps the manual links (0.16.18: the Launch Control XL 3's)
for f in (SRC / "docs" / "maps").glob("*.perfmap"):
    shutil.copy(f, OUT / "manual" / "maps" / f.name)
quick_card = ""
if (SRC / "build" / "quick-manual.pdf").exists():   # the printable quick manual (tools/make_quick_manual.py, made on Windows with Edge)
    shutil.copy(SRC / "build" / "quick-manual.pdf", OUT / "manual" / "PERF-1-quick-manual.pdf")
    quick_card = '<a class="card" href="manual/PERF-1-quick-manual.pdf" download><b>Quick manual (PDF)</b><span>Eight A4 pages to print: every control, page and key map on paper.</span></a>'
if (SRC / "build" / "beginners-guide.pdf").exists():  # the beginner's guide (docs/GUIDE.md, tools/make_quick_manual.py docs/GUIDE.md --out beginners-guide)
    shutil.copy(SRC / "build" / "beginners-guide.pdf", OUT / "manual" / "PERF-1-beginners-guide.pdf")
    quick_card = '<a class="card" href="manual/PERF-1-beginners-guide.pdf" download><b>Beginner\'s guide (PDF)</b><span>Your first hour, step by step, in plain words.</span></a>' + quick_card
if (SRC / "build" / "manual.pdf").exists():         # the whole manual as a PDF (Edge headless printing docs/manual.html: tools/make_manual_pdf.sh)
    shutil.copy(SRC / "build" / "manual.pdf", OUT / "manual" / "PERF-1-manual.pdf")
    quick_card += '<a class="card" href="manual/PERF-1-manual.pdf" download><b>Full manual (PDF)</b><span>The whole manual to download or print.</span></a>'
for img in [*(SRC / "docs" / "img").glob("perf-screen-*.png"), *(SRC / "docs" / "img").glob("guide-*.png")]:
    shutil.copy(img, OUT / "manual" / "img" / img.name)

(OUT / "index.html").write_text(f'''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PERF-1</title><link rel="icon" href="img/favicon.png">
<meta name="description" content="PERF-1: live-performance firmware for the M-VAVE FM-1: twenty-two genres of generative pieces, an auto-DJ, effects, a web editor and DX7 voices.">
<style>
:root{{--bg:#0b1022;--panel:#121a33;--line:#2c3766;--text:#f2efe6;--dim:#9aa3c7;--acc:#ffb000;--red:#a58bff}}
@media (prefers-color-scheme:light){{:root{{--bg:#f3f5fa;--panel:#ffffff;--line:#d9deec;--text:#121833;--dim:#5a6388;--acc:#b76a00;--red:#5b3fd0}}}}
*{{box-sizing:border-box}}body{{margin:0;background:var(--bg);}}@media (prefers-color-scheme:dark){{body{{background:radial-gradient(1600px 720px at 50% -280px, rgba(114,83,230,.26), transparent 70%) fixed, url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='180' height='180'><filter id='n'><feTurbulence type='fractalNoise' baseFrequency='.85' numOctaves='3' stitchTiles='stitch'/><feColorMatrix values='0 0 0 0 .62  0 0 0 0 .58  0 0 0 0 1  0 0 0 .08 0'/></filter><rect width='100%25' height='100%25' filter='url(%23n)'/></svg>"), repeating-linear-gradient(0deg, rgba(255,255,255,.014) 0 1px, transparent 1px 3px), var(--bg)}}}}body{{color:var(--text);font:16px/1.55 system-ui,"Segoe UI",sans-serif}}
main{{max-width:1100px;margin:0 auto;padding:40px 16px 56px}}
img.logo{{width:96px;height:auto;display:block;margin:0 0 12px}}h1{{margin:0;font-size:2.4rem;letter-spacing:.02em}}h1 span{{color:var(--red)}}
.tag{{color:var(--dim);margin:.2rem 0 1.6rem}}.v{{color:var(--acc);font-weight:600}}
.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:12px;margin:0 0 28px}}
.card{{display:block;background:var(--panel);border:1px solid var(--line);border-radius:2px;padding:14px 16px;color:inherit;text-decoration:none}}
.card:hover{{border-color:var(--acc)}}.card b{{display:block;font-size:1.05rem;color:var(--acc)}}.card span{{color:var(--dim);font-size:.92rem}}
p.small{{color:var(--dim);font-size:.88rem}}a{{color:var(--acc)}}
  /* (1.0) the banner: the maker's festival picture, PERF-1 in the boot card's letters, the lights pulsing at 128 BPM */  .hero {{ position: relative; height: clamp(260px, 46vw, 560px); margin: 0 0 1.6rem; overflow: hidden; border-radius: 2px;          background: #05070f 50% 62% / cover no-repeat; image-rendering: pixelated; border: 1px solid #2c3766; border-top-color: #5a47c8;          box-shadow: 0 0 0 1px #05070f, 0 0 0 7px #161d3a, 0 0 0 8px #3d4a8a, 0 0 0 9px #05070f, 0 16px 48px rgba(114,83,230,.32), inset 0 1px 0 rgba(165,139,255,.3);          display: flex; flex-direction: column; align-items: center; justify-content: flex-start; padding-top: clamp(14px, 3vw, 34px); gap: .5rem; }}  .hero.slim {{ height: clamp(130px, 13vw, 190px); padding-top: clamp(10px, 1.6vw, 20px); background-position: 50% 42%; }}  .hero::before {{ content: ""; position: absolute; inset: 0; pointer-events: none; mix-blend-mode: screen;                  background: radial-gradient(55% 60% at 50% 30%, rgba(255,110,220,.42), transparent 70%),                              radial-gradient(30% 50% at 12% 20%, rgba(80,200,255,.30), transparent 70%),                              radial-gradient(30% 50% at 88% 20%, rgba(80,200,255,.30), transparent 70%);                  animation: hero-pulse .46875s ease-out infinite; }}  .hero::after {{ content: ""; position: absolute; inset: 0; pointer-events: none;                 background: linear-gradient(rgba(5,7,15,.55), transparent 38%, transparent 70%, rgba(11,16,34,.75)); }}  @keyframes hero-pulse {{ 0% {{ opacity: 1; }} 55%, 100% {{ opacity: .2; }} }}  .hero .mark {{ position: relative; z-index: 1; height: clamp(46px, 7.5vw, 104px); width: auto; filter: drop-shadow(0 0 14px rgba(165,139,255,.55)); }}  .hero.slim .mark {{ height: clamp(34px, 4.2vw, 58px); }}  .hero .tagline {{ position: relative; z-index: 1; font: 600 clamp(11px, 1.1vw, 15px) "Chakra Petch", sans-serif; letter-spacing: .32em;                   color: #f2efe6; text-shadow: 0 0 10px rgba(114,83,230,.9), 0 1px 0 #05070f; }}  @media (prefers-reduced-motion: reduce) {{ .hero::before {{ animation: none; opacity: .5; }} }}  @media print {{ .hero {{ display: none; }} }}  /* (1.0) hard metal: sharp corners, a lit top edge, a dark outer line */  .card {{ border-radius: 2px; background: radial-gradient(circle at 13px 13px, #d4d8f6 0 .9px, #8a90c4 1.6px, #565c94 2.8px, #2a3162 3.8px, #05070f 4.6px, rgba(5,7,15,.6) 5.2px, transparent 6px), radial-gradient(circle at calc(100% - 13px) 13px, #d4d8f6 0 .9px, #8a90c4 1.6px, #565c94 2.8px, #2a3162 3.8px, #05070f 4.6px, rgba(5,7,15,.6) 5.2px, transparent 6px), radial-gradient(circle at 13px calc(100% - 13px), #d4d8f6 0 .9px, #8a90c4 1.6px, #565c94 2.8px, #2a3162 3.8px, #05070f 4.6px, rgba(5,7,15,.6) 5.2px, transparent 6px), radial-gradient(circle at calc(100% - 13px) calc(100% - 13px), #d4d8f6 0 .9px, #8a90c4 1.6px, #565c94 2.8px, #2a3162 3.8px, #05070f 4.6px, rgba(5,7,15,.6) 5.2px, transparent 6px), url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='180' height='180'><filter id='n'><feTurbulence type='fractalNoise' baseFrequency='.85' numOctaves='3' stitchTiles='stitch'/><feColorMatrix values='0 0 0 0 .62  0 0 0 0 .58  0 0 0 0 1  0 0 0 .08 0'/></filter><rect width='100%25' height='100%25' filter='url(%23n)'/></svg>"), repeating-linear-gradient(90deg, rgba(255,255,255,.02) 0 1px, rgba(0,0,0,.05) 1px 2px, transparent 2px 4px), linear-gradient(180deg, #17213f, #0e152e); border-color: #2c3766; border-top-color: #3d4a8a;          box-shadow: inset 0 1px 0 rgba(165,139,255,.10), 0 0 0 1px #05070f, 0 6px 18px rgba(0,0,0,.35); }}  button {{ border-radius: 2px; }}</style></head><body><main>
<div class="hero" style="background-image:url(img/banner.png)"><svg class="mark" viewBox="0 0 36 8" role="img" aria-label="PERF-1" shape-rendering="crispEdges"><g fill="#ffb000"><rect x="0.5" y="0.5" width="1" height="1"/><rect x="1.5" y="0.5" width="1" height="1"/><rect x="2.5" y="0.5" width="1" height="1"/><rect x="3.5" y="0.5" width="1" height="1"/><rect x="0.5" y="1.5" width="1" height="1"/><rect x="4.5" y="1.5" width="1" height="1"/><rect x="0.5" y="2.5" width="1" height="1"/><rect x="4.5" y="2.5" width="1" height="1"/><rect x="0.5" y="3.5" width="1" height="1"/><rect x="1.5" y="3.5" width="1" height="1"/><rect x="2.5" y="3.5" width="1" height="1"/><rect x="3.5" y="3.5" width="1" height="1"/><rect x="0.5" y="4.5" width="1" height="1"/><rect x="0.5" y="5.5" width="1" height="1"/><rect x="0.5" y="6.5" width="1" height="1"/><rect x="6.5" y="0.5" width="1" height="1"/><rect x="7.5" y="0.5" width="1" height="1"/><rect x="8.5" y="0.5" width="1" height="1"/><rect x="9.5" y="0.5" width="1" height="1"/><rect x="10.5" y="0.5" width="1" height="1"/><rect x="6.5" y="1.5" width="1" height="1"/><rect x="6.5" y="2.5" width="1" height="1"/><rect x="6.5" y="3.5" width="1" height="1"/><rect x="7.5" y="3.5" width="1" height="1"/><rect x="8.5" y="3.5" width="1" height="1"/><rect x="9.5" y="3.5" width="1" height="1"/><rect x="6.5" y="4.5" width="1" height="1"/><rect x="6.5" y="5.5" width="1" height="1"/><rect x="6.5" y="6.5" width="1" height="1"/><rect x="7.5" y="6.5" width="1" height="1"/><rect x="8.5" y="6.5" width="1" height="1"/><rect x="9.5" y="6.5" width="1" height="1"/><rect x="10.5" y="6.5" width="1" height="1"/><rect x="12.5" y="0.5" width="1" height="1"/><rect x="13.5" y="0.5" width="1" height="1"/><rect x="14.5" y="0.5" width="1" height="1"/><rect x="15.5" y="0.5" width="1" height="1"/><rect x="12.5" y="1.5" width="1" height="1"/><rect x="16.5" y="1.5" width="1" height="1"/><rect x="12.5" y="2.5" width="1" height="1"/><rect x="16.5" y="2.5" width="1" height="1"/><rect x="12.5" y="3.5" width="1" height="1"/><rect x="13.5" y="3.5" width="1" height="1"/><rect x="14.5" y="3.5" width="1" height="1"/><rect x="15.5" y="3.5" width="1" height="1"/><rect x="12.5" y="4.5" width="1" height="1"/><rect x="14.5" y="4.5" width="1" height="1"/><rect x="12.5" y="5.5" width="1" height="1"/><rect x="15.5" y="5.5" width="1" height="1"/><rect x="12.5" y="6.5" width="1" height="1"/><rect x="16.5" y="6.5" width="1" height="1"/><rect x="18.5" y="0.5" width="1" height="1"/><rect x="19.5" y="0.5" width="1" height="1"/><rect x="20.5" y="0.5" width="1" height="1"/><rect x="21.5" y="0.5" width="1" height="1"/><rect x="22.5" y="0.5" width="1" height="1"/><rect x="18.5" y="1.5" width="1" height="1"/><rect x="18.5" y="2.5" width="1" height="1"/><rect x="18.5" y="3.5" width="1" height="1"/><rect x="19.5" y="3.5" width="1" height="1"/><rect x="20.5" y="3.5" width="1" height="1"/><rect x="21.5" y="3.5" width="1" height="1"/><rect x="18.5" y="4.5" width="1" height="1"/><rect x="18.5" y="5.5" width="1" height="1"/><rect x="18.5" y="6.5" width="1" height="1"/><rect x="25.5" y="3.5" width="1" height="1"/><rect x="26.5" y="3.5" width="1" height="1"/><rect x="27.5" y="3.5" width="1" height="1"/><rect x="32.5" y="0.5" width="1" height="1"/><rect x="31.5" y="1.5" width="1" height="1"/><rect x="32.5" y="1.5" width="1" height="1"/><rect x="32.5" y="2.5" width="1" height="1"/><rect x="32.5" y="3.5" width="1" height="1"/><rect x="32.5" y="4.5" width="1" height="1"/><rect x="32.5" y="5.5" width="1" height="1"/><rect x="31.5" y="6.5" width="1" height="1"/><rect x="32.5" y="6.5" width="1" height="1"/><rect x="33.5" y="6.5" width="1" height="1"/></g><g fill="#a58bff"><rect x="0" y="0" width="1" height="1"/><rect x="1" y="0" width="1" height="1"/><rect x="2" y="0" width="1" height="1"/><rect x="3" y="0" width="1" height="1"/><rect x="0" y="1" width="1" height="1"/><rect x="4" y="1" width="1" height="1"/><rect x="0" y="2" width="1" height="1"/><rect x="4" y="2" width="1" height="1"/><rect x="0" y="3" width="1" height="1"/><rect x="1" y="3" width="1" height="1"/><rect x="2" y="3" width="1" height="1"/><rect x="3" y="3" width="1" height="1"/><rect x="0" y="4" width="1" height="1"/><rect x="0" y="5" width="1" height="1"/><rect x="0" y="6" width="1" height="1"/><rect x="6" y="0" width="1" height="1"/><rect x="7" y="0" width="1" height="1"/><rect x="8" y="0" width="1" height="1"/><rect x="9" y="0" width="1" height="1"/><rect x="10" y="0" width="1" height="1"/><rect x="6" y="1" width="1" height="1"/><rect x="6" y="2" width="1" height="1"/><rect x="6" y="3" width="1" height="1"/><rect x="7" y="3" width="1" height="1"/><rect x="8" y="3" width="1" height="1"/><rect x="9" y="3" width="1" height="1"/><rect x="6" y="4" width="1" height="1"/><rect x="6" y="5" width="1" height="1"/><rect x="6" y="6" width="1" height="1"/><rect x="7" y="6" width="1" height="1"/><rect x="8" y="6" width="1" height="1"/><rect x="9" y="6" width="1" height="1"/><rect x="10" y="6" width="1" height="1"/><rect x="12" y="0" width="1" height="1"/><rect x="13" y="0" width="1" height="1"/><rect x="14" y="0" width="1" height="1"/><rect x="15" y="0" width="1" height="1"/><rect x="12" y="1" width="1" height="1"/><rect x="16" y="1" width="1" height="1"/><rect x="12" y="2" width="1" height="1"/><rect x="16" y="2" width="1" height="1"/><rect x="12" y="3" width="1" height="1"/><rect x="13" y="3" width="1" height="1"/><rect x="14" y="3" width="1" height="1"/><rect x="15" y="3" width="1" height="1"/><rect x="12" y="4" width="1" height="1"/><rect x="14" y="4" width="1" height="1"/><rect x="12" y="5" width="1" height="1"/><rect x="15" y="5" width="1" height="1"/><rect x="12" y="6" width="1" height="1"/><rect x="16" y="6" width="1" height="1"/><rect x="18" y="0" width="1" height="1"/><rect x="19" y="0" width="1" height="1"/><rect x="20" y="0" width="1" height="1"/><rect x="21" y="0" width="1" height="1"/><rect x="22" y="0" width="1" height="1"/><rect x="18" y="1" width="1" height="1"/><rect x="18" y="2" width="1" height="1"/><rect x="18" y="3" width="1" height="1"/><rect x="19" y="3" width="1" height="1"/><rect x="20" y="3" width="1" height="1"/><rect x="21" y="3" width="1" height="1"/><rect x="18" y="4" width="1" height="1"/><rect x="18" y="5" width="1" height="1"/><rect x="18" y="6" width="1" height="1"/><rect x="25" y="3" width="1" height="1"/><rect x="26" y="3" width="1" height="1"/><rect x="27" y="3" width="1" height="1"/><rect x="32" y="0" width="1" height="1"/><rect x="31" y="1" width="1" height="1"/><rect x="32" y="1" width="1" height="1"/><rect x="32" y="2" width="1" height="1"/><rect x="32" y="3" width="1" height="1"/><rect x="32" y="4" width="1" height="1"/><rect x="32" y="5" width="1" height="1"/><rect x="31" y="6" width="1" height="1"/><rect x="32" y="6" width="1" height="1"/><rect x="33" y="6" width="1" height="1"/></g></svg><span class="tagline">LIVE &nbsp;//&nbsp; GENERATIVE &nbsp;//&nbsp; FREE</span></div>
<img class="logo" src="img/slinky.png" alt="the PERF-1 slinky">
<h1>PERF<span>-1</span></h1>
<p class="tag">Free firmware that turns the M-VAVE FM-1 into a groovebox that plays by itself. By deamonSLINKY. Version <span class="v">{ver}</span>.</p>
<p>Switch it on and music starts: drums, bass, a pad, an acid line and a plucked melody, in twenty-two styles, from techno to ambient. You steer it with the buttons and keys: change the chords, break it down and drop it back in, mute parts, throw on effects, or let the auto-DJ run the show. When you want more, nearly everything can be edited and saved. New here? Start with the beginner's guide.</p>
<div class="grid">
<a class="card" href="install/"><b>Install</b><span>Install it from this website in Chrome or Edge, with the FM-1 plugged in. You can go back to the original firmware the same way.</span></a>
<a class="card" href="emu/"><b>Try it in your browser</b><span>Play with PERF-1 on screen, with sound, before you install anything.</span></a>
<a class="card" href="studio/"><b>Studio</b><span>See a piece laid out over its bars, change it, and hear it play in your browser or on your FM-1.</span></a>
<a class="card" href="editor/"><b>Editor</b><span>Change any setting from your computer, and make your own DX7 sounds and hear them on the FM-1.</span></a>
<a class="card" href="pieces/"><b>Library &amp; pieces</b><span>Pieces and sets made by players: hear them in your browser and add them to your FM-1. Submit your own, one or many at once.</span></a>
<a class="card" href="manual/"><b>Manual</b><span>Starts with a beginner's guide, then explains every button and page.</span></a>
{quick_card}
<a class="card" href="firmware/{name}"><b>Download the firmware file</b><span>{name} ({len(raw) // 1024} KB). You only need this if you install another way than the page above.</span></a>
<a class="card" href="{REPO}/issues/new/choose"><b>Ideas and problems</b><span>Tell me what you'd like next, or what went wrong. Most new features started as someone's idea.</span></a>
<a class="card" href="https://ko-fi.com/deamonslinky" target="_blank" rel="noopener"><b>Buy me a coffee</b><span>PERF-1 is free. If it makes your music, a coffee on Ko-fi keeps new features coming.</span></a>
<a class="card" href="{REPO}"><b>Source code</b><span>Free and open source (GPL-3.0-only), on GitHub.</span></a>
</div>
<p class="small">Installing firmware is at your own risk. If something goes wrong, the installer can put the original firmware back, and an FM-1 that fails to start twice in a row starts in a safe mode the installer can reach. Section 1 of the manual explains it all.</p>
<p class="small">PERF-1 by deamonSLINKY builds on the work of others, named in the manual's <a href="manual/#s19">credits</a>. GPL-3.0-only; see LICENSING.md in the repository. "M-VAVE" and "FM-1" are trademarks of their owners: PERF-1 is independent firmware, not affiliated with or endorsed by them.</p>
</main></body></html>
''', encoding="utf-8", newline="\n")
if not SHARING:                                 # the menus and the landing page without them
    for f in OUT.rglob("*.html"):
        t = f.read_text(encoding="utf-8")
        u = re.sub(r'<a (?:class="card" )?href="(?:\.\./)?(?:pieces|library)/"[^>]*>.*?</a>\n?', "", t)
        if u != t:
            f.write_text(u, encoding="utf-8", newline="\n")
print(f"site: {OUT}: index, install/, emu/, editor/, manual/, firmware/{name} ({len(raw)} B, {product})")
