#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The site's "not found" page (GitHub Pages serves 404.html for any missing path), and a download link per
firmware that never changes. The workflow runs this last, over build/pages.

Every build names its download after its version (hyp1-1.5.35.fwsc), so a link copied from an earlier build
stops working at the next push. With this:
  - each firmware's file is also published as <name>-latest.fwsc (firmware/hyp1-latest.fwsc, lof/firmware/
    lof1-latest.fwsc, ...): a link to share that always gets the current build;
  - an old or mistyped download link lands on 404.html, which sends it on to that firmware's current file;
  - any other missing page gets a short "not found" with the way to every firmware and OPEN-1.

  python3 tools/site/notfound.py build/pages"""
import json
import os
import shutil
import sys
from pathlib import Path

PRODUCTS = [("", "hyp1", "HYP-1", "hyp/"), ("lof/", "lof1", "LOF-1", "lof/"), ("eco/", "eco1", "ECO-1", "eco/"),
            ("grav/", "grav1", "GRAV-1", "grav/")]

PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Not found · FM-1 firmware by RedtailWorks</title>
<meta name="robots" content="noindex">
<style>
:root { --ground: #141318; --card: #1f1c27; --ink: #f1ecf4; --muted: #aaa1b8; --rule: #3e3a4c; --orange: #ff8020; color-scheme: dark; }
body { background: var(--ground); color: var(--ink); margin: 0; padding-block: 48px; padding-inline: 16px;
       font: 400 18px/1.6 "Barlow Semi Condensed", "Helvetica Neue", Arial, sans-serif; }
main { max-width: 640px; margin: 0 auto; }
h1 { font: 700 2rem/1.2 "Chakra Petch", "Helvetica Neue", Arial, sans-serif; margin: 0 0 .6rem; }
p { color: var(--muted); }
a { color: var(--ink); text-decoration-color: var(--orange); text-decoration-thickness: 2px; text-underline-offset: 3px; }
ul { list-style: none; padding: 0; display: grid; gap: 8px; }
li a { display: block; background: var(--card); border: 1px solid var(--rule); border-radius: 10px; padding: 10px 14px; text-decoration: none; }
li a:hover { border-color: var(--orange); }
.dl { border: 1px solid var(--orange); border-radius: 10px; padding: 12px 16px; background: #2a1d10; color: #f3dcc4; }
</style>
</head>
<body>
<main>
<div id="dl" class="dl" hidden></div>
<h1 id="h">Page not found</h1>
<p id="why">That page isn't here, or it has moved. These are the ways in:</p>
<ul id="ways"></ul>
</main>
<script>
(function () {
  var ROOT = __ROOT__;                       /* the site's path, e.g. /fm1-HYP1/ */
  var LATEST = __LATEST__;                   /* folder -> [the current download, the firmware's name, its page] */
  var path = location.pathname;
  var rel = path.indexOf(ROOT) === 0 ? path.slice(ROOT.length) : path.replace(/^\\//, "");
  var ways = document.getElementById("ways");
  function add(href, text) {
    var li = document.createElement("li"), a = document.createElement("a");
    a.href = href; a.textContent = text; li.appendChild(a); ways.appendChild(li);
  }
  add(ROOT, "All four firmwares (the front page)");
  Object.keys(LATEST).forEach(function (k) { add(ROOT + LATEST[k][2], LATEST[k][1]); });
  add(ROOT + "open1/", "OPEN-1, the next idea");
  /* an old download: <folder>firmware/<anything>.fwsc -> that firmware's current file */
  var m = rel.match(/^((?:lof|eco|grav)\\/)?firmware\\/[^\\/]+\\.fwsc$/i);
  if (m) {
    var f = LATEST[m[1] || ""];
    if (f) {
      var to = ROOT + f[0], dl = document.getElementById("dl");
      document.getElementById("h").textContent = "Downloading the latest " + f[1];
      document.getElementById("why").textContent = "The link you followed was for an earlier build. Your download of the current one should start now.";
      dl.hidden = false;
      dl.innerHTML = 'If it doesn\\u2019t start, <a href="' + to + '">download ' + f[0].split("/").pop() + "</a>.";
      location.replace(to);
    }
  }
})();
</script>
</body>
</html>
"""


def main(root):
    root = Path(root)
    latest = {}
    for folder, stem, name, page in PRODUCTS:
        fw = root / folder / "firmware"
        files = sorted(p for p in fw.glob(f"{stem}-*.fwsc") if not p.name.endswith("-latest.fwsc")) if fw.exists() else []
        if not files:
            continue                                   # that firmware didn't build this time
        cur = files[-1]
        shutil.copy(cur, fw / f"{stem}-latest.fwsc")   # the link that never changes
        latest[folder] = [f"{folder}firmware/{cur.name}", name, page]
    site_root = os.environ.get("SITE_ROOT", "/fm1-HYP1/")
    page = PAGE.replace("__ROOT__", json.dumps(site_root)).replace("__LATEST__", json.dumps(latest))
    (root / "404.html").write_text(page, encoding="utf-8")
    print(f"notfound: 404.html; latest links for {', '.join(v[1] for v in latest.values())}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
