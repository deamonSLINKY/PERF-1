#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The dev-build page: one private claude.ai artifact, republished after every dev build, so the build can be
looked at (and its docs read) from any machine. Standard library only.

    python3 tools/make_dev_artifact.py [--version 0.16.5] [--out build/dev_artifact]

Writes OUT/index.html (the page), OUT/docs/*.md (the changelog, manual and quick guide as they are in this tree),
OUT/img/*.png (the manual's screenshots, when tools/make_screenshots.py has made them), OUT/emu/* (the browser
emulator, when web/emu/build.sh has built it) and OUT/files.json: the Artifact tool's `files` map for all of it.
Publish with the Artifact tool: file_path OUT/index.html, files from OUT/files.json, url the dev page's URL.
"""
import argparse, html, json, re, shutil, subprocess, sys
from datetime import datetime, timezone
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent


def git(*a):
    try:
        return subprocess.run(["git", *a], cwd=SRC, capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def top_section(md):
    """The changelog's newest '## ' section (the one after the note on version numbers)."""
    parts = re.split(r"(?m)^## ", md)
    for p in parts[1:]:
        if not p.lower().startswith("a note"):
            return "## " + p.rstrip()
    return ""


def newest_build(section):
    """The highest development number named in a changelog section: '(0.16.4)' -> '0.16.4'."""
    nums = re.findall(r"\((\d+\.\d+\.\d+)\)", section) or re.findall(r"\b(\d+\.\d+\.\d+)\b", section)
    return max(nums, key=lambda v: tuple(int(x) for x in v.split(".")), default="DEV")


PAGE = r"""<title>PERF-1 Dev Build</title>
<meta name="claude:viewport-fit-sides" content="cover">
<style>
/* Layout: a build sheet. The build's identity first, then tabs for what changed, the commits, the screens and the
   docs; one column, read top to bottom. */
:root { --bg:#f6f1ea; --panel:#fffaf3; --line:#e4d9ca; --fg:#231a14; --muted:#7a6a5c; --accent:#b2471f; --ok:#2f7d4f; --warn:#a86a00;
  --body: "IBM Plex Sans", system-ui, sans-serif; --mono: "IBM Plex Mono", ui-monospace, Consolas, monospace; }
@media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --bg:#120e0c; --panel:#1b1512; --line:#2f2622; --fg:#efe4d2; --muted:#a08f7c; --accent:#ffb347; --ok:#6fd39a; --warn:#ffd447; color-scheme: dark } }
:root[data-theme="dark"] { --bg:#120e0c; --panel:#1b1512; --line:#2f2622; --fg:#efe4d2; --muted:#a08f7c; --accent:#ffb347; --ok:#6fd39a; --warn:#ffd447; color-scheme: dark }
* { box-sizing: border-box }
body { background: var(--bg); color: var(--fg); font: 15px/1.6 var(--body); padding-block: 24px 48px;
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }
.wrap { max-width: 980px; margin: 0 auto; display: flex; flex-direction: column; gap: 20px }
.id { display: flex; flex-wrap: wrap; gap: 10px 28px; align-items: baseline; border-bottom: 2px solid var(--accent); padding-bottom: 12px }
.id h1 { margin: 0; font: 600 28px/1.1 var(--mono); letter-spacing: -.01em }
.id span { font: 13px var(--mono); color: var(--muted); font-variant-numeric: tabular-nums }
.pill { font: 12px var(--mono); padding: 2px 8px; border-radius: 99px; border: 1px solid currentColor }
.pill.ok { color: var(--ok) } .pill.warn { color: var(--warn) }
nav { display: flex; flex-wrap: wrap; gap: 6px }
nav button { font: 13px var(--mono); background: var(--panel); color: var(--fg); border: 1px solid var(--line); padding: 6px 12px; border-radius: 4px; cursor: pointer }
nav button[aria-selected="true"] { background: var(--accent); color: var(--bg); border-color: var(--accent) }
nav button:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px }
section { min-width: 0 }
.md { max-width: 75ch } .md h2 { font: 600 20px/1.3 var(--mono); margin: 18px 0 8px } .md h3 { font: 600 16px var(--mono); margin: 14px 0 6px }
.md p, .md li { margin: 0 0 8px } .md code { font: 13px var(--mono); background: var(--panel); border: 1px solid var(--line); padding: 0 4px; border-radius: 3px }
.md pre { background: var(--panel); border: 1px solid var(--line); padding: 10px; overflow-x: auto } .md pre code { border: 0; padding: 0 }
.md .tbl { overflow-x: auto } .md table { border-collapse: collapse; font-size: 14px } .md th, .md td { border-bottom: 1px solid var(--line); padding: 4px 8px; text-align: left; vertical-align: top }
.md a { color: var(--accent) }
ol.commits { font: 13px/1.5 var(--mono); padding-left: 0; list-style: none; margin: 0 } ol.commits li { padding: 4px 0; border-bottom: 1px solid var(--line) } ol.commits b { color: var(--accent); font-weight: 400 }
.shots { display: grid; grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); gap: 14px }
.shots figure { margin: 0 } .shots img { width: 100%; max-width: 100%; aspect-ratio: 1; image-rendering: pixelated; border-radius: 4px; background: #000 }
.shots figcaption { font: 12px var(--mono); color: var(--muted) }
.note { color: var(--muted); font-size: 14px }
</style>
<div class="wrap">
  <header class="id">
    <h1>PERF-1 __VERSION__</h1>
    <span>commit __COMMIT__ · __BRANCH__ · __DATE__</span>
    <span class="pill __DIRTYCLS__">__DIRTY__</span>
    __RAM__
  </header>
  <nav role="tablist" id="tabs"></nav>
  <section id="v-changes" class="md" data-tab="Changes"></section>
  <section id="v-commits" data-tab="Commits" hidden><ol class="commits">__COMMITS__</ol></section>
  <section id="v-screens" data-tab="Screens" hidden>__SHOTS__</section>
  <section id="v-emu" data-tab="Emulator" hidden>__EMU__</section>
  <section id="v-docs" data-tab="Docs" hidden><p class="note">The docs as they are in this build's tree (published with the page; a Claude session can read each with the Artifact tool's read and its path).</p><ul>__DOCS__</ul><div class="md" id="docview"></div></section>
</div>
<script id="changes" type="text/markdown">__CHANGES__</script>
<script>
__MDJS__
const views = [...document.querySelectorAll("section[data-tab]")], nav = document.getElementById("tabs");
function show(id) { views.forEach(v => v.hidden = v.id !== id); [...nav.children].forEach(b => b.setAttribute("aria-selected", String(b.dataset.v === id))); }
views.forEach(v => { const b = document.createElement("button"); b.id = "tab-" + v.id; b.dataset.v = v.id; b.role = "tab"; b.textContent = v.dataset.tab; b.onclick = () => show(v.id); nav.appendChild(b); });
const h = location.hash.slice(1); show(views.some(v => v.id === "v-" + h) ? "v-" + h : "v-changes");
document.getElementById("v-changes").innerHTML = md(document.getElementById("changes").textContent.split("<\\/script").join("</script"));
document.querySelectorAll("a[data-doc]").forEach(a => a.onclick = async e => {
  e.preventDefault();
  const el = document.getElementById("docview");
  try { const r = await fetch(a.dataset.doc); el.innerHTML = r.ok ? md(await r.text()) : "<p>Could not load " + a.dataset.doc + " (" + r.status + ").</p>"; }
  catch (err) { el.innerHTML = "<p>Could not load " + a.dataset.doc + ".</p>"; }
});
</script>
"""

# A small Markdown renderer: headings, paragraphs, lists, tables, fenced code, bold, italics, inline code, links.
MDJS = r"""
function esc(s) { return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;"); }
function inl(s) {
  return esc(s).replace(/`([^`]+)`/g, "<code>$1</code>").replace(/\*\*([^*]+)\*\*/g, "<b>$1</b>")
    .replace(/(^|[^*])\*([^*\s][^*]*)\*/g, "$1<i>$2</i>")
    .replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, (m, t, u) => /^(https?:|#|[\w./-]+$)/.test(u) ? `<a href="${u}" target="_blank" rel="noopener">${t}</a>` : t);
}
function md(src) {
  const L = src.replace(/\r/g, "").split("\n"), out = []; let i = 0;
  while (i < L.length) {
    let l = L[i];
    if (/^```/.test(l)) { const b = []; i++; while (i < L.length && !/^```/.test(L[i])) b.push(esc(L[i++])); i++; out.push("<pre><code>" + b.join("\n") + "</code></pre>"); continue; }
    let m = l.match(/^(#{1,4})\s+(.*)/); if (m) { const n = Math.min(m[1].length + 1, 4); out.push(`<h${n}>${inl(m[2])}</h${n}>`); i++; continue; }
    if (/^---+\s*$/.test(l)) { out.push("<hr>"); i++; continue; }
    if (/^\|/.test(l) && i + 1 < L.length && /^\|[\s:|-]+\|?\s*$/.test(L[i + 1])) {
      const row = r => r.trim().replace(/^\||\|$/g, "").split(/(?<!\\)\|/).map(c => inl(c.trim().replace(/\\\|/g, "|")));
      const hd = row(l); i += 2; const rows = [];
      while (i < L.length && /^\|/.test(L[i])) rows.push(row(L[i++]));
      out.push('<div class="tbl"><table><thead><tr>' + hd.map(c => "<th>" + c + "</th>").join("") + "</tr></thead><tbody>" +
        rows.map(r => "<tr>" + r.map(c => "<td>" + c + "</td>").join("") + "</tr>").join("") + "</tbody></table></div>"); continue;
    }
    if (/^\s*([-*]|\d+\.)\s+/.test(l)) {
      const ord = /^\s*\d+\./.test(l), items = [];
      while (i < L.length && (/^\s*([-*]|\d+\.)\s+/.test(L[i]) || (/^\s{2,}\S/.test(L[i]) && items.length))) {
        if (/^\s*([-*]|\d+\.)\s+/.test(L[i]) && !/^\s{4,}/.test(L[i])) items.push(L[i].replace(/^\s*([-*]|\d+\.)\s+/, ""));
        else items[items.length - 1] += " " + L[i].trim().replace(/^([-*]|\d+\.)\s+/, "· ");
        i++;
      }
      out.push((ord ? "<ol>" : "<ul>") + items.map(t => "<li>" + inl(t) + "</li>").join("") + (ord ? "</ol>" : "</ul>")); continue;
    }
    if (/^>\s?/.test(l)) { const b = []; while (i < L.length && /^>\s?/.test(L[i])) b.push(L[i++].replace(/^>\s?/, "")); out.push("<blockquote>" + md(b.join("\n")) + "</blockquote>"); continue; }
    if (!l.trim()) { i++; continue; }
    const p = []; while (i < L.length && L[i].trim() && !/^(#{1,4}\s|```|\||\s*([-*]|\d+\.)\s|>)/.test(L[i])) p.push(L[i++]);
    if (!p.length) { out.push("<p>" + inl(L[i++]) + "</p>"); continue; }
    out.push("<p>" + inl(p.join(" ")) + "</p>");
  }
  return out.join("\n");
}
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--version", help="the dev build's number (default: the newest named in docs/CHANGELOG.md)")
    ap.add_argument("--out", default="build/dev_artifact")
    ap.add_argument("--commits", type=int, default=40)
    ap.add_argument("--page-only", action="store_true", help="the page alone: no docs, screenshots or emulator")
    a = ap.parse_args()
    out = (SRC / a.out).resolve()
    if out.exists():
        shutil.rmtree(out)
    (out / "docs").mkdir(parents=True)
    files = {}

    log = (SRC / "docs" / "CHANGELOG.md").read_text(encoding="utf-8") if (SRC / "docs" / "CHANGELOG.md").exists() else ""
    sec = top_section(log)
    version = a.version or newest_build(sec)
    ram = re.findall(r"RAM \.data\+\.bss (\d+)", sec)
    commit = git("rev-parse", "--short", "HEAD") or "?"
    dirty = len([x for x in git("status", "--porcelain").splitlines() if x.strip()])

    docs = []
    for name in (() if a.page_only else ("CHANGELOG.md", "MANUAL.md", "QUICK.md", "GUIDE.md", "SMK37.md")):
        for base in (SRC / "docs", SRC):
            p = base / name
            if p.exists():
                shutil.copy2(p, out / "docs" / name)
                files[f"docs/{name}"] = str(out / "docs" / name)
                docs.append(f'<li><a href="docs/{name}" data-doc="docs/{name}">{name}</a></li>')
                break

    shots = [] if a.page_only else sorted((SRC / "docs" / "img").glob("perf-screen-*.png"))
    if shots:
        (out / "img").mkdir()
        figs = []
        for s in shots:
            shutil.copy2(s, out / "img" / s.name)
            files[f"img/{s.name}"] = str(out / "img" / s.name)
            label = s.stem.replace("perf-screen-", "")
            figs.append(f'<figure><img src="img/{s.name}" alt="{label} screen" loading="lazy"><figcaption>{label}</figcaption></figure>')
        shots_html = '<div class="shots">' + "".join(figs) + "</div>"
    else:
        shots_html = '<p class="note">No screenshots in this build: run tools/make_screenshots.py before this script.</p>'

    emu = SRC / "build" / "emu"
    if not a.page_only and (emu / "index.html").exists() and (emu / "amb.wasm").exists():
        (out / "emu").mkdir()
        for n in ("index.html", "worklet.js", "amb.wasm"):
            if (emu / n).exists():
                shutil.copy2(emu / n, out / "emu" / n)
                files[f"emu/{n}"] = {"from": str(out / "emu" / n), "contentType": "application/wasm"} if n.endswith(".wasm") else str(out / "emu" / n)
        emu_html = ('<p><a class="pill ok" href="emu/index.html">Open the emulator for this build</a></p>'
                    '<p class="note">The browser emulator built from this tree (web/emu/build.sh). If it does not start here, the viewer may block '
                    'WebAssembly: use the GitHub Pages emulator for the released version instead.</p>')
    else:
        emu_html = '<p class="note">No emulator in this build: run web/emu/build.sh before this script.</p>'

    commits = git("log", f"-{a.commits}", "--format=%h%x09%ad%x09%s", "--date=short")
    commits_html = "".join(f"<li><b>{html.escape(h)}</b> {html.escape(d)} · {html.escape(s)}</li>"
                           for h, d, s in (ln.split("\t", 2) for ln in commits.splitlines() if ln.count("\t") >= 2)) or "<li>no git history</li>"

    page = (PAGE.replace("__VERSION__", html.escape(version))
            .replace("__COMMIT__", html.escape(commit))
            .replace("__BRANCH__", html.escape(git("rev-parse", "--abbrev-ref", "HEAD") or "?"))
            .replace("__DATE__", datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC"))
            .replace("__DIRTYCLS__", "warn" if dirty else "ok")
            .replace("__DIRTY__", f"{dirty} uncommitted file(s)" if dirty else "clean tree")
            .replace("__RAM__", f'<span>RAM .data+.bss {ram[0]} B</span>' if ram else "")
            .replace("__COMMITS__", commits_html)
            .replace("__SHOTS__", shots_html)
            .replace("__EMU__", emu_html)
            .replace("__DOCS__", "".join(docs) or "<li>none</li>")
            .replace("__MDJS__", MDJS)
            .replace("__CHANGES__", (sec or "No changelog section found.").replace("</script", "<\\/script")))
    (out / "index.html").write_text(page, encoding="utf-8")
    (out / "files.json").write_text(json.dumps(files, indent=1), encoding="utf-8")
    print(f"dev page: {out / 'index.html'} ({version}, {commit}, {len(files)} files)")


if __name__ == "__main__":
    sys.exit(main())
