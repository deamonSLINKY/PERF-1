#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The OPEN-1 page: RedtailWorks' idea for an open-firmware synth, with its goals, the pitch video, the target
specs and prices, a mailing list and a timeline of updates. Built from open1/ in the repository:

  open1/site.json          the settings: the current phase, the YouTube id, the Buttondown name, the links
  open1/updates/*.md       one file per update (front matter: title, date, tag; then Markdown); newest first
  open1/media/             the video, its poster and any picture an update uses (as media/name.png)

  python3 tools/site/open1.py build/pages      ->  build/pages/open1/index.html, feed.xml, media/

Needs the Python Markdown package (pip install markdown)."""
import html
import json
import re
import shutil
import sys
from datetime import date, datetime, timezone
from email.utils import format_datetime
from pathlib import Path

import markdown

SRC = Path(__file__).resolve().parents[2]
CONTENT = SRC / "open1"

PHASES = [  # name, what happens
    ("Idea and pitch", "The spec, the parts list, drawings of the board, a pitch deck and the concept video."),
    ("Breadboard prototype", "A Daisy Seed (the same chip as OPEN-1) on the bench, and the beta board design in KiCad, to work out the product and test it."),
    ("First boards", "The first custom circuit boards and a hand-built case, to test the real keys, pads, screen and audio."),
    ("Emulator, SDK and firmware jam", "Developers build firmware in the browser before the hardware ships; the community votes on the pack that comes with it."),
    ("Crowdfunding", "Early-bird and backer pricing for the first production run."),
    ("Manufacturing and shipping", "The first 1,000 units, with the community firmware pack preloaded."),
]

ENGINE = [  # what, OPEN-1 target, the FM-1 today
    ("Processor", "480 MHz Arm Cortex-M7 (STM32H750)", "240 MHz"),
    ("Working memory", "32 MB + 1 MB on the chip", "About 432 KB"),
    ("Firmware storage", "16 MB of firmware slots, plus one microSD card for firmware, samples and projects", "1 MB"),
    ("Firmwares installed", "Several, swapped from the menu", "One at a time"),
    ("Audio", "4 in and 4 out, 24-bit; also a 4 × 4 USB audio interface", "Stereo out"),
    ("MIDI", "TRS in and out with an A/B switch, plus USB", "TRS in, USB, Bluetooth"),
    ("USB-C", "Device (audio, MIDI, charging) and host (keyboards, drives)", "Device"),
    ("Sync and CV", "Sync in and out, 2 CV outs (0–5 V); rates and roles set by the firmware", "—"),
]

INSTRUMENT = [
    ("Keys", "27 silicone keys like the FM-1's, each lit by its own RGB LED (no velocity)"),
    ("Pads", "16 velocity-sensitive RGB pads, 4 × 4"),
    ("Buttons", "16 lit buttons in two rows of 8; each firmware names them on screen"),
    ("Controls", "8 endless push encoders, a thumbstick, 2 spring levers, a volume knob"),
    ("Screen", "4.0″ IPS, 480 × 320"),
    ("Sound out", "Built-in speaker, headphones, two stereo line outs"),
    ("Pedal", "6.35 mm jack for a sustain or expression pedal"),
    ("Power", "3000 mAh battery, charges over USB-C"),
    ("Connections", "Wired only: no Bluetooth, for low latency"),
    ("Size", "About 320 × 230 mm, a third smaller than the first design; a wedge 34 mm at the back, 18 mm at the front"),
    ("For tinkerers", "A 2 × 5 expansion header"),
]

GOALS = [
    ("Under $200, easy to carry", "An instrument a beginner can buy as their first synth and take anywhere: keys, pads, sounds and effects in one compact box."),
    ("Open firmware", "A documented chip, an open SDK and a free toolchain. Anyone can write firmware for it, and anyone can install it."),
    ("Many machines in one", "Keep several firmwares on the device and a library on the SD card. Techno box tonight, plant simulator tomorrow."),
    ("Room to grow", "Twice the clock speed and about 75 times the working memory of the FM-1, for longer delays, samples and bigger engines."),
    ("More ways in and out", "Stereo audio in turns it into an effects box. MIDI out and USB host let it drive and be driven by other gear."),
    ("Firmware before hardware", "A browser emulator and a firmware jam before launch, so it ships with a library already full."),
]

PRICES = [  # label, price, note, featured
    ("Early bird", "$159", "Crowdfunding, a limited first batch", False),
    ("Crowdfunding", "$179", "Backer price for the first run", False),
    ("Retail", "$199", "The target price after launch", True),
]

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OPEN-1 by RedtailWorks</title>
<meta name="description" content="OPEN-1 is RedtailWorks' idea for an open-firmware synth: bring your own keys, 16 pads, a microSD firmware library, all the compute on board. Breadboard prototype phase: follow along.">
<meta name="theme-color" content="#121116">
<link rel="icon" type="image/png" href="../img/favicon.png">
<link rel="alternate" type="application/rss+xml" title="OPEN-1 updates" href="feed.xml">
<meta property="og:title" content="OPEN-1: one synth, every firmware">
<meta property="og:description" content="An open synth you rewrite from an SD card. Bring your own keys. Early prototype, targets not promises.">
<meta property="og:image" content="__SITE__media/pitch-poster.jpg">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@500;600;700&family=Barlow+Semi+Condensed:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500&display=swap">
<style>
/* One dark ground like the rest of the site. Orange leads here (the firmware pages lead with purple);
   a monospace face marks every target figure, like readings off a spec sheet. */
:root { --ground: #121116; --panel: #1c1a22; --panel-2: #26232e; --ink: #f3eee8; --muted: #a9a2b3; --rule: #393544;
        --orange: #ff8a2a; --orange-dim: #c4581c; --purple: #b88bff; --green: #7fdc8a;
        --display: "Chakra Petch", "Helvetica Neue", Arial, sans-serif;
        --body: "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif;
        --mono: "IBM Plex Mono", ui-monospace, Menlo, Consolas, monospace; color-scheme: dark; }
* { box-sizing: border-box; }
html { scroll-behavior: smooth; }
@media (prefers-reduced-motion: reduce) { html { scroll-behavior: auto; } }
body { background: var(--ground); color: var(--ink); margin: 0; padding-block: 24px 64px;
       padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px));
       font: 400 18px/1.6 var(--body); }
main { max-width: 1080px; margin: 0 auto; }
h1, h2, h3 { font-family: var(--display); text-wrap: balance; }
a { color: var(--ink); text-decoration-color: var(--orange); text-decoration-thickness: 2px; text-underline-offset: 3px; }
a:hover { color: var(--orange); }
:focus-visible { outline: 2px solid var(--orange); outline-offset: 3px; }
.mono { font-family: var(--mono); }

header.site { display: flex; flex-wrap: wrap; align-items: center; gap: .5rem 1.4rem; margin: 0 0 2rem; }
header.site .home { font: 700 1.2rem var(--display); letter-spacing: .08em; text-decoration: none; display: inline-flex; align-items: center; gap: .6rem; }
header.site .home span:nth-of-type(1) { color: var(--purple); } header.site .home span:nth-of-type(2) { color: var(--orange); }
header.site nav { margin-left: auto; display: flex; flex-wrap: wrap; gap: .3rem 1.1rem; font-size: 1.02rem; }
img.panda { image-rendering: pixelated; display: block; height: auto; }

.badge { display: inline-flex; align-items: center; gap: .5em; font: 600 .82rem var(--display); letter-spacing: .14em; text-transform: uppercase;
         color: var(--orange); border: 1.5px solid var(--orange); border-radius: 999px; padding: .3em .9em; }
.badge i { width: .5em; height: .5em; border-radius: 50%; background: var(--orange); }
.hero { display: grid; grid-template-columns: minmax(0, .9fr) minmax(0, 1.1fr); gap: 32px; align-items: center; margin-bottom: 1rem; }
.hero h1 { font-size: clamp(3.2rem, 9vw, 5.6rem); line-height: .95; letter-spacing: .02em; margin: 1rem 0 .4rem; }
.hero h1 span { color: var(--orange); }
.hero .line { font: 600 1.55rem/1.25 var(--display); margin: 0 0 1rem; }
.hero .lede { font-size: 1.15rem; color: var(--muted); margin: 0 0 1.4rem; max-width: 34rem; }
.hero img { width: 100%; height: auto; display: block; border-radius: 16px; border: 1px solid var(--rule); }
.ctas { display: flex; flex-wrap: wrap; gap: 10px; }
.btn { display: inline-flex; align-items: center; justify-content: center; gap: .45em; min-height: 46px; padding: .4em 1.3em; border-radius: 10px;
       font: 600 1.05rem var(--body); text-decoration: none; border: 1px solid var(--rule); background: var(--panel-2); color: var(--ink); cursor: pointer; }
.btn.primary { background: var(--orange); border-color: var(--orange); color: #1d0e02; }
.btn.primary:hover { background: #ffa456; color: #1d0e02; }
.btn:hover { border-color: var(--orange); }
.btn.kofi { background: var(--orange-dim); border-color: var(--orange-dim); color: #fff6ee; }
.btn.kofi:hover { background: var(--orange); color: #1d0e02; }
.disclaimer { margin: 1.4rem 0 0; padding: 12px 16px; border-radius: 10px; background: var(--panel); border: 1px dashed var(--orange-dim); color: var(--muted); font-size: 1rem; }
.disclaimer b { color: var(--ink); }

section { margin-top: 3.6rem; scroll-margin-top: 16px; }
h2 { font-size: 1.5rem; font-weight: 700; letter-spacing: .05em; text-transform: uppercase; margin: 0 0 1.1rem; display: flex; align-items: center; gap: .7rem; }
h2::before { content: ""; width: 30px; height: 5px; background: var(--orange); border-radius: 2px; }
h2 + .sub { color: var(--muted); margin: -.5rem 0 1.3rem; max-width: 46rem; }

figure { margin: 0; }
.video video, .video iframe { display: block; width: 100%; aspect-ratio: 16 / 9; height: auto; border-radius: 14px; border: 1px solid var(--rule); background: #000; }
.video figcaption { display: flex; flex-wrap: wrap; justify-content: space-between; gap: .3rem 1rem; color: var(--muted); font-size: .98rem; margin-top: .5rem; }
.video figcaption a { color: var(--orange); }

/* where the project is: a real sequence, so it's numbered */
.phases { list-style: none; margin: 0; padding: 0; display: grid; grid-template-columns: repeat(6, minmax(0, 1fr)); gap: 10px; counter-reset: ph; }
.phases li { position: relative; background: var(--panel); border: 1px solid var(--rule); border-radius: 12px; padding: 14px 14px 16px; min-width: 0; }
.phases li::before { counter-increment: ph; content: counter(ph, decimal-leading-zero); font: 500 .85rem var(--mono); color: var(--muted); }
.phases h3 { font-size: 1.05rem; line-height: 1.2; margin: .3rem 0 .4rem; }
.phases p { margin: 0; font-size: .95rem; line-height: 1.4; color: var(--muted); }
.phases .state { display: inline-block; margin-top: .6rem; font: 600 .72rem var(--display); letter-spacing: .1em; text-transform: uppercase; }
.phases li.done { border-color: #3c5a40; } .phases li.done .state { color: var(--green); }
.phases li.now { border: 2px solid var(--orange); background: linear-gradient(180deg, #2a1d16, var(--panel) 70%); }
.phases li.now .state { color: var(--orange); }
.phases li.next .state { color: var(--muted); }

.goals { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 0; border-top: 1px solid var(--rule); }
.goal { padding: 18px 20px 20px 0; border-bottom: 1px solid var(--rule); min-width: 0; }
.goal:not(:nth-child(3n+1)) { padding-left: 20px; border-left: 1px solid var(--rule); }
.goal h3 { font-size: 1.25rem; margin: 0 0 .35rem; color: var(--ink); }
.goal p { margin: 0; color: var(--muted); }

.specs { display: grid; grid-template-columns: minmax(0, 1.15fr) minmax(0, .85fr); gap: 24px; align-items: start; }
.tablewrap { overflow-x: auto; border: 1px solid var(--rule); border-radius: 12px; background: var(--panel); }
table { width: 100%; border-collapse: collapse; font-size: 1rem; }
caption, .cap { margin: 0; text-align: left; padding: 14px 16px 6px; font: 700 1rem var(--display); letter-spacing: .06em; text-transform: uppercase; }
th, td { text-align: left; padding: 10px 16px; border-top: 1px solid var(--rule); vertical-align: top; }
thead th { font: 600 .8rem var(--display); letter-spacing: .1em; text-transform: uppercase; color: var(--muted); border-top: 0; }
tbody th { font-weight: 600; color: var(--muted); }
td.open { font-family: var(--mono); font-size: .93rem; color: var(--ink); }
td.fm1 { font-family: var(--mono); font-size: .93rem; color: var(--muted); }
thead th.open { color: var(--orange); }
dl.inst { margin: 0; padding: 4px 16px 10px; }
dl.inst div { display: grid; grid-template-columns: 7.5rem minmax(0, 1fr); gap: 12px; padding: 9px 0; border-top: 1px solid var(--rule); }
dl.inst div:first-child { border-top: 0; }
dl.inst dt { color: var(--muted); font-weight: 600; }
dl.inst dd { margin: 0; }
.ports { margin-top: 18px; }
.ports img { width: 100%; height: auto; display: block; border-radius: 12px; border: 1px solid var(--rule); }
.ports figcaption { color: var(--muted); font-size: .95rem; margin-top: .4rem; }

.prices { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 12px; }
.price { background: var(--panel); border: 1px solid var(--rule); border-radius: 14px; padding: 18px 20px; display: flex; flex-direction: column; gap: .2rem; }
.price.featured { border: 2px solid var(--orange); }
.price .label { font: 600 .85rem var(--display); letter-spacing: .12em; text-transform: uppercase; color: var(--muted); }
.price .amt { font: 500 2.6rem/1.1 var(--mono); color: var(--ink); font-variant-numeric: tabular-nums; }
.price.featured .amt { color: var(--orange); }
.price .note { color: var(--muted); }
.costs { display: flex; flex-wrap: wrap; gap: 10px 28px; margin: 16px 0 0; padding: 14px 18px; border-radius: 12px; background: var(--panel); border: 1px solid var(--rule); }
.costs div { display: flex; flex-direction: column; min-width: 0; }
.costs b { font: 500 1.25rem var(--mono); }
.costs span { color: var(--muted); font-size: .95rem; }
.small { color: var(--muted); font-size: .95rem; }

.join { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); gap: 24px; align-items: center; background: var(--panel);
        border: 1px solid var(--orange-dim); border-radius: 16px; padding: 24px; }
.join h2 { margin-bottom: .6rem; }
.join p { margin: 0; color: var(--muted); }
.join form { display: flex; flex-wrap: wrap; gap: 10px; }
.join label { flex-basis: 100%; font: 600 .85rem var(--display); letter-spacing: .1em; text-transform: uppercase; color: var(--muted); }
.join input[type=email] { flex: 1 1 14rem; min-width: 0; min-height: 48px; padding: .4em .9em; border-radius: 10px; border: 1px solid var(--rule);
                          background: var(--ground); color: var(--ink); font: 500 1.05rem var(--body); }
.join input[type=email]:focus-visible { border-color: var(--orange); }
.join .msg { flex-basis: 100%; min-height: 1.4em; margin: 0; font-size: .98rem; color: var(--green); }
.join .alt { flex-basis: 100%; font-size: .95rem; }

/* the timeline: a line down the left, a dot per update */
.timeline { list-style: none; margin: 0; padding: 0 0 0 26px; border-left: 2px solid var(--rule); max-width: 46rem; }
.post { position: relative; padding: 0 0 2.2rem; scroll-margin-top: 16px; }
.post::before { content: ""; position: absolute; left: -34px; top: .45rem; width: 14px; height: 14px; border-radius: 50%;
                background: var(--ground); border: 3px solid var(--orange); }
.post:last-child { padding-bottom: 0; }
.post .meta { display: flex; flex-wrap: wrap; align-items: center; gap: .3rem .8rem; font-size: .95rem; color: var(--muted); }
.post time { font-family: var(--mono); font-size: .9rem; }
.post .tag { font: 600 .72rem var(--display); letter-spacing: .1em; text-transform: uppercase; color: var(--purple); border: 1px solid var(--purple); border-radius: 999px; padding: .1em .65em; }
.post .meta a { color: var(--muted); text-decoration: none; }
.post .meta a:hover { color: var(--orange); }
.post h3 { font-size: 1.45rem; line-height: 1.2; margin: .35rem 0 .6rem; }
.post .body > :first-child { margin-top: 0; }
.post .body p, .post .body ul, .post .body ol { margin: 0 0 .8rem; }
.post .body li { margin-bottom: .2rem; }
.post .body img { max-width: 100%; height: auto; border-radius: 10px; border: 1px solid var(--rule); }
.post .body code { font-family: var(--mono); font-size: .9em; background: var(--panel-2); padding: .1em .35em; border-radius: 4px; }
.feed { margin-top: 1rem; font-size: .95rem; color: var(--muted); }

.support { display: flex; flex-wrap: wrap; align-items: center; gap: 1rem 1.6rem; }
.support p { margin: 0; flex: 1 1 24rem; color: var(--muted); }
.support p b { color: var(--ink); }
.contact { display: inline-flex; flex-wrap: wrap; align-items: center; gap: .5rem; }
.contact code { font: 500 1rem var(--mono); background: var(--panel-2); border: 1px solid var(--rule); padding: .35em .7em; border-radius: 8px; user-select: all; }
footer { margin-top: 3.4rem; padding-top: 1.2rem; border-top: 1px solid var(--rule); display: flex; flex-wrap: wrap; gap: .6rem 1.6rem; align-items: center; }
footer .rw { font: 700 1rem var(--display); letter-spacing: .08em; text-decoration: none; display: inline-flex; align-items: center; gap: .5rem; }
footer .rw span:nth-of-type(1) { color: var(--purple); } footer .rw span:nth-of-type(2) { color: var(--orange); }

@media (max-width: 960px) { .phases { grid-template-columns: repeat(3, minmax(0, 1fr)); } .specs { grid-template-columns: minmax(0, 1fr); } }
@media (max-width: 760px) {
  .hero, .join { grid-template-columns: minmax(0, 1fr); }
  .hero img { order: -1; }
  .goals { grid-template-columns: minmax(0, 1fr); }
  .goal, .goal:not(:nth-child(3n+1)) { padding: 16px 0; border-left: 0; }
  .prices { grid-template-columns: minmax(0, 1fr); }
  header.site nav { margin-left: 0; }
}
@media (max-width: 520px) { .phases { grid-template-columns: repeat(2, minmax(0, 1fr)); } dl.inst div { grid-template-columns: 1fr; gap: 0; } }
</style>
</head>
<body>
<!--rw-open1-->
<main>
<header class="site"><a class="home" href="../"><img class="panda" src="../img/panda.png" width="64" height="50" alt=""><span>REDTAIL</span><span>WORKS</span></a>
  <nav><a href="../">FM-1 firmware</a><a href="#updates">Updates</a><a href="#join">Mailing list</a><a href="https://www.youtube.com/@RedtailWorks">YouTube</a><a href="__KOFI__">Ko-fi</a></nav></header>

<div class="hero">
  <div>
    <span class="badge"><i></i>Beta design &middot; breadboard phase</span>
    <h1>OPEN<span>-1</span></h1>
    <p class="line">One synth, every firmware.</p>
    <p class="lede">An open synth you rewrite from an SD card. Bring your own keys: plug in an FM-1 or any USB or MIDI controller, and OPEN-1 does the rest, with 16 RGB pads, touch strips, knobs, a screen and a library of firmware you swap from the menu.</p>
    <div class="ctas"><a class="btn primary" href="#join">Get updates</a><a class="btn" href="#video">Watch the video</a></div>
  </div>
  <img src="media/open1-v04.jpg" width="1600" height="1025" alt="Render of the OPEN-1 v0.4 beta design: a screen and four knobs top left, eight Cherry MX menu keys, two touch strips across the middle and sixteen RGB pads in two rows of eight.">
</div>
<p class="disclaimer"><b>Not a product yet.</b> OPEN-1 is a very early design I'm sharing to work out the product and test it. Nothing is for sale; the renders are design previews, and every spec and price on this page is a target, not a promise.</p>

<section id="video">
  <h2>The latest video</h2>
  <figure class="video">
__VIDEO__
  </figure>
</section>

<section id="phase">
  <h2>Where it is now</h2>
  <p class="sub">Six phases from idea to your desk. The orange one is where OPEN-1 is today.</p>
  <ol class="phases">
__PHASES__
  </ol>
</section>

<section id="goals">
  <h2>Goals</h2>
  <div class="goals">
__GOALS__
  </div>
</section>

<section id="specs">
  <h2>Target specs</h2>
  <p class="sub">These targets come from design 2, the keyboard version. The v0.4 beta drops the built-in keys (see the updates below), so expect this table to change as the beta settles.</p>
  <div class="specs">
    <div>
      <div class="tablewrap"><table>
        <caption>The engine, next to the FM-1</caption>
        <thead><tr><th scope="col">&nbsp;</th><th scope="col" class="open">OPEN-1 target</th><th scope="col">FM-1 today</th></tr></thead>
        <tbody>
__ENGINE__
        </tbody>
      </table></div>
      <figure class="ports"><img src="media/open1-rear.jpg" width="1600" height="470" alt="The rear edge, left to right: power switch, USB-C device and host, microSD slot, MIDI in and out with an A/B switch, sync in and out, CV 1 and 2, a 6.35 mm pedal jack, audio in 1-2 and 3-4, audio out 1-2 and 3-4, and phones." loading="lazy">
        <figcaption>The port side. Every connection is on the back edge.</figcaption></figure>
    </div>
    <div class="tablewrap">
      <p class="cap">The instrument</p>
      <dl class="inst">
__INSTRUMENT__
      </dl>
    </div>
  </div>
</section>

<section id="price">
  <h2>Price targets</h2>
  <p class="sub">The goal is an entry instrument, so the price is a design constraint from the first sketch.</p>
  <div class="prices">
__PRICES__
  </div>
  <div class="costs">
    <div><b>~$73</b><span>Target parts cost per unit, at 1,000 units</span></div>
    <div><b>~$80&ndash;250</b><span>Breadboard prototype, the current phase</span></div>
    <div><b>$100&ndash;120k</b><span>Estimated cost of the first 1,000 units</span></div>
  </div>
  <p class="small">Prices in US dollars, before tax and shipping. Estimates from the current parts list; they'll be updated here as quotes come in.</p>
</section>

<section id="join" class="join">
  <div>
    <h2>Mailing list</h2>
    <p>One email when there's real news: a working prototype, the firmware jam opening, crowdfunding dates. No spam, unsubscribe any time.</p>
  </div>
__FORM__
</section>

<section id="updates">
  <h2>Updates</h2>
  <p class="sub">The build log, newest first.</p>
  <ol class="timeline">
__POSTS__
  </ol>
  <p class="feed">Follow in a feed reader: <a href="feed.xml">RSS</a></p>
</section>

<section id="support" class="support">
  <p><b>Want to help it happen?</b> Ko-fi tips fund the breadboard prototype and keep the free FM-1 firmware coming. For partnerships, manufacturing or firmware, email me.</p>
  <div class="ctas"><a class="btn kofi" href="__KOFI__">Support on Ko-fi</a>
    <span class="contact"><code>__EMAIL__</code><a class="btn" href="mailto:__EMAIL__">Email</a></span></div>
</section>

<footer><a class="rw" href="https://www.redtail.works"><img class="panda" src="../img/panda.png" width="48" height="38" alt=""><span>REDTAIL</span><span>WORKS</span></a>
  <span class="small">OPEN-1 is an independent RedtailWorks concept, not affiliated with or endorsed by M-VAVE. &middot; <a href="https://www.redtail.works">www.redtail.works</a> &middot; <a href="../">FM-1 firmware</a></span></footer>
</main>
__SCRIPT__
</body>
</html>
"""

FORM_BUTTONDOWN = """  <form id="join-form" action="https://buttondown.com/api/emails/embed-subscribe/__BD__" method="post" target="bd-popup"
        onsubmit="window.open('https://buttondown.com/__BD__', 'bd-popup'); document.getElementById('join-msg').textContent = 'Check your inbox to confirm.'; return true">
    <label for="join-email">Your email</label>
    <input type="email" name="email" id="join-email" placeholder="you@example.com" autocomplete="email" required>
    <button class="btn primary" type="submit">Join the list</button>
    <p class="msg" id="join-msg" aria-live="polite"></p>
  </form>"""

FORM_EMAIL = """  <form id="join-form">
    <label for="join-email">Your email</label>
    <input type="email" name="email" id="join-email" placeholder="you@example.com" autocomplete="email" required>
    <button class="btn primary" type="submit">Join the list</button>
    <p class="msg" id="join-msg" aria-live="polite"></p>
    <p class="alt small">This opens your email app with a sign-up message to <code>__EMAIL__</code>. Send it and you're on the list.</p>
  </form>"""

SCRIPT_EMAIL = """<script>
document.getElementById('join-form').addEventListener('submit', function (e) {
  e.preventDefault();
  var a = document.getElementById('join-email').value.trim();
  location.href = 'mailto:__EMAIL__?subject=' + encodeURIComponent('Add me to the OPEN-1 list') +
    '&body=' + encodeURIComponent('Please add ' + a + ' to the OPEN-1 mailing list.');
  document.getElementById('join-msg').textContent = 'Your email app should open now. Send the message to join.';
});
</script>"""


def read_posts():
    posts = []
    for f in sorted((CONTENT / "updates").glob("*.md")):
        text = f.read_text(encoding="utf-8")
        meta, body = {}, text
        m = re.match(r"^---\s*\n(.*?)\n---\s*\n(.*)$", text, re.S)
        if m:
            for line in m.group(1).splitlines():
                if ":" in line:
                    k, v = line.split(":", 1)
                    meta[k.strip().lower()] = v.strip().strip('"')
            body = m.group(2)
        d = meta.get("date") or f.name[:10]
        try:
            when = date.fromisoformat(d)
        except ValueError:
            sys.exit(f"open1: {f.name}: the date must look like 2026-10-17, not {d!r}")
        slug = re.sub(r"^\d{4}-\d{2}-\d{2}-", "", f.stem)
        posts.append({"slug": slug, "title": meta.get("title") or slug.replace("-", " ").capitalize(),
                      "date": when, "tag": meta.get("tag", ""), "file": f.name,
                      "html": markdown.markdown(body.strip(), extensions=["extra", "sane_lists"])})
    posts.sort(key=lambda p: (p["date"], p["file"]), reverse=True)
    return posts


def esc(s):
    return html.escape(s, quote=True)


def main(root):
    out = Path(root) / "open1"
    cfg = json.loads((CONTENT / "site.json").read_text(encoding="utf-8"))
    email, kofi = cfg.get("email", "itsme@redtail.works"), cfg.get("kofi", "https://ko-fi.com/redtailworks")
    site = cfg.get("site_url", "https://rustypawz.github.io/fm1-HYP1/open1/").rstrip("/") + "/"
    phase = int(cfg.get("phase", 0))
    yt, bd = cfg.get("youtube", "").strip(), cfg.get("buttondown", "").strip()

    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(CONTENT / "media", out / "media")

    if yt:
        video = (f'    <iframe src="https://www.youtube-nocookie.com/embed/{esc(yt)}?rel=0" title="OPEN-1 beta prototype design" loading="lazy" '
                 'allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>\n'
                 f'    <figcaption><span>The v0.4 beta prototype design: why the built-in keys went, what&rsquo;s on the board, and the KiCad files so far. A very early design, for working out the product.</span>'
                 f'<a href="https://www.youtube.com/watch?v={esc(yt)}">Watch on YouTube</a></figcaption>')
    else:
        video = ('    <video controls preload="metadata" playsinline poster="media/pitch-poster.jpg" width="1920" height="1080">'
                 '<source src="media/open1-pitch.mp4" type="video/mp4"></video>\n'
                 '    <figcaption><span>OPEN-1 running a simulated HYP-1, in time with a real HYP-1 take on the FM-1. 55 seconds, with sound. It shows the first, 37-key design; design 2 is in the updates below.</span>'
                 '<a href="https://www.youtube.com/@RedtailWorks">More on YouTube</a></figcaption>')

    phases = []
    for i, (name, what) in enumerate(PHASES):
        cls, state = ("done", "Done") if i < phase else ("now", "Now") if i == phase else ("next", "Next" if i == phase + 1 else "Later")
        cur = ' aria-current="step"' if i == phase else ""
        phases.append(f'    <li class="{cls}"{cur}><h3>{esc(name)}</h3><p>{esc(what)}</p><span class="state">{state}</span></li>')

    goals = [f'    <div class="goal"><h3>{esc(t)}</h3><p>{esc(p)}</p></div>' for t, p in GOALS]
    engine = [f'          <tr><th scope="row">{esc(a)}</th><td class="open">{esc(b)}</td><td class="fm1">{esc(c)}</td></tr>' for a, b, c in ENGINE]
    inst = [f'        <div><dt>{esc(a)}</dt><dd>{esc(b)}</dd></div>' for a, b in INSTRUMENT]
    prices = [f'    <div class="price{" featured" if feat else ""}"><span class="label">{esc(l)}</span><span class="amt">{esc(p)}</span>'
              f'<span class="note">{esc(n)}</span></div>' for l, p, n, feat in PRICES]

    posts = read_posts()
    items = []
    for p in posts:
        tag = f'<span class="tag">{esc(p["tag"])}</span>' if p["tag"] else ""
        items.append(f'    <li class="post" id="{esc(p["slug"])}"><div class="meta"><time datetime="{p["date"].isoformat()}">'
                     f'{p["date"].strftime("%b")} {p["date"].day}, {p["date"].year}</time>{tag}<a href="#{esc(p["slug"])}">Link</a></div>'
                     f'<h3>{esc(p["title"])}</h3><div class="body">{p["html"]}</div></li>')
    if not items:
        items.append('    <li class="post"><h3>No updates yet</h3><div class="body"><p>The first one is on its way.</p></div></li>')

    if bd:
        form, script = FORM_BUTTONDOWN.replace("__BD__", esc(bd)), ""
    else:
        form, script = FORM_EMAIL, SCRIPT_EMAIL

    page = (PAGE.replace("__VIDEO__", video).replace("__PHASES__", "\n".join(phases)).replace("__GOALS__", "\n".join(goals))
            .replace("__ENGINE__", "\n".join(engine)).replace("__INSTRUMENT__", "\n".join(inst)).replace("__PRICES__", "\n".join(prices))
            .replace("__POSTS__", "\n".join(items)).replace("__FORM__", form).replace("__SCRIPT__", script)
            .replace("__EMAIL__", esc(email)).replace("__KOFI__", esc(kofi)).replace("__SITE__", site))
    (out / "index.html").write_text(page, encoding="utf-8")

    rss = ['<?xml version="1.0" encoding="utf-8"?>', '<rss version="2.0"><channel>', '<title>OPEN-1 updates</title>',
           f'<link>{site}</link>', '<description>The build log for OPEN-1, an open-firmware synth idea by RedtailWorks.</description>',
           '<language>en</language>']
    for p in posts:
        when = datetime(p["date"].year, p["date"].month, p["date"].day, 12, tzinfo=timezone.utc)
        body = p["html"].replace('src="media/', f'src="{site}media/')
        rss.append(f'<item><title>{esc(p["title"])}</title><link>{site}#{p["slug"]}</link><guid isPermaLink="false">open1-{p["slug"]}</guid>'
                   f'<pubDate>{format_datetime(when)}</pubDate><description>{esc(body)}</description></item>')
    rss.append('</channel></rss>')
    (out / "feed.xml").write_text("\n".join(rss) + "\n", encoding="utf-8")
    print(f"open1: index.html, feed.xml, {len(posts)} update(s), phase {phase + 1} of {len(PHASES)}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
