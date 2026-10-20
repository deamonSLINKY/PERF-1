---
title: Bring your own keys: the v0.4 beta design
date: 2026-10-20
tag: Prototype
---
OPEN-1 is out of the ideas phase and onto the bench. Here's the beta prototype design, with a new video that walks through it:

![The OPEN-1 v0.4 beta design: screen and four knobs top left, eight menu keys top right, two touch strips across the middle and sixteen RGB pads below](media/open1-v04.jpg)

**Why the keys are gone.** Every earlier OPEN-1 had a keyboard built in. But almost everyone who wants an OPEN-1 already owns keys: an FM-1, a MIDI keyboard, a controller on the desk. Building another keybed into the box made it bigger and more expensive and didn't add much. So OPEN-1 is now a **bring your own keys** synth. Plug in your FM-1 or any USB or TRS MIDI controller, and all the sound, sequencing and firmware run on OPEN-1.

**What's on the beta board**

- A Daisy Seed brain: 480 MHz Cortex-M7 with 64 MB of RAM
- A 2.0″ colour screen and 4 knobs
- 16 RGB pads in two rows of 8 on Cherry MX switches, which you can also play as keys
- 8 Cherry MX menu keys
- 2 touch strips for pitch and mod, built right into the faceplate
- USB-A host for your controller, USB-C for your computer and charging, and TRS MIDI in and out
- A microSD card for firmware, samples and projects
- A battery and a built-in speaker

The schematics are done and checked, the main board is laid out for autorouting, and the faceplate (which doubles as the switch plate) is drawn in KiCad.

**Very early, on purpose.** This beta exists to work out the product and test it. Layouts, parts, size and price can and will change, and the specs further up this page now describe this beta design. The price targets are still from the keyboard design while I re-cost it.

**I'm looking for help.** If you know your way around PCB layout, embedded audio firmware (C++ on the Daisy), enclosures, or small-batch manufacturing, and you'd like to help: partners, henchmen and femboys all welcome. Come say hi in the [OPEN-1 Discord](https://discord.gg/dpgSDDBVb).
