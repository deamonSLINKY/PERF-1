#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""docs/QUICK.md -> build/quick-manual.html (print CSS embedded) -> build/quick-manual.pdf (Edge headless).

  tools/make_quick_manual.py [IN.md] [--html-only] [--measure] [--edge PATH]

--measure also prints each section's height in mm (each on a page of its own), to see what to trim.

The quick manual is an eight-page A4 sheet: QUICK.md is its source, written for print. A `---` rule or a
`<div class="page"></div>` line is a page break; a `<div class="cols">` .. `</div>` pair sets two columns;
a `<!-- footer: ... -->` line is the running footer (version, date, where the full manual is). Standard
library only: the Markdown converter below handles the subset the sheet uses (headings, paragraphs, bold,
italic, code, tables, lists, rules, raw HTML lines). After the PDF is written the page count is printed
(pypdf if it is installed, else counted in the PDF's bytes) with each page's first line, so an overflow
onto a ninth page is seen at once."""
import html
import os
import re
import subprocess
import sys
import time
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
WANT_PAGES = 8
IMG_BASE = None                                     # the Markdown file's folder (images are relative to it)

CSS = """
@page { size: A4; margin: 12mm 12mm 13mm; @bottom-center { content: var(--foot); } }   /* (filled in by convert(): the running footer and the page number) */
html { font-size: 9pt; }
body { margin: 0; color: #000; background: #fff; line-height: 1.22;
       font-family: "Segoe UI", "Helvetica Neue", Helvetica, Arial, sans-serif;
       -webkit-print-color-adjust: exact; print-color-adjust: exact; }
code { font-family: Consolas, "Cascadia Mono", Menlo, "DejaVu Sans Mono", monospace; font-size: .92em; white-space: nowrap; }
h1 { font-size: 18pt; font-weight: 700; margin: 0 0 1.2mm; letter-spacing: .01em; }
h2 { font-size: 12pt; font-weight: 700; margin: 0 0 1.5mm; padding-bottom: .5mm; border-bottom: .5pt solid #000; break-after: avoid; }
h3 { font-size: 9.6pt; font-weight: 700; margin: 1.8mm 0 .7mm; break-after: avoid; }
h3 + p, h3 + table, h3 + ul { break-before: avoid; }
p { margin: 0 0 1.1mm; }
p.lead { font-size: 9.4pt; margin-bottom: 1.6mm; }
ul, ol { margin: 0 0 1.1mm; padding-left: 3.6mm; }
li { margin: 0 0 .3mm; }
strong { font-weight: 700; }
table { border-collapse: collapse; width: 100%; font-size: 8pt; line-height: 1.18; margin: 0 0 1.4mm; }
tr { break-inside: avoid; }
thead { display: table-header-group; }
th, td { border: .3pt solid #777; padding: .25mm .8mm; vertical-align: top; text-align: left; }
th { font-weight: 700; border-bottom: .7pt solid #000; }
td:first-child { white-space: nowrap; }
.cols { columns: 2; column-gap: 4.5mm; }
figure { margin: 1.5mm 0 2.5mm; break-inside: avoid; }
figure img { display: block; max-width: 40%; image-rendering: pixelated; border: .3pt solid #999; }
figure img[src*="panel"] { max-width: 78%; image-rendering: auto; }
a { color: inherit; }
figcaption { font-size: .85em; color: #444; margin-top: .6mm; }
.newcol { break-before: column; }                 /* <h3 class="newcol"> in QUICK.md: start this heading in the next column */
.page + .page { margin-top: 3.5mm; padding-top: 2mm; border-top: .5pt solid #bbb; }   /* sections flow on: a section a little over a page no longer leaves the rest of the next sheet blank */
@media screen {
  body { background: #ccc; padding: 10mm 0; }
  main { background: #fff; width: 210mm; margin: 0 auto; padding: 12mm; box-shadow: 0 1px 6px rgba(0,0,0,.4); }
}
"""

# ---- inline Markdown ------------------------------------------------------------------------------------

def inline(s):
    """**bold**, *italic*, `code`; the rest HTML-escaped. Code spans are set aside first so nothing inside is touched."""
    codes = []
    def keep(m):
        codes.append("<code>" + html.escape(m.group(1)) + "</code>")
        return "\x00%d\x00" % (len(codes) - 1)
    s = re.sub(r"`([^`]+)`", keep, s)
    s = html.escape(s, quote=False)
    s = re.sub(r"\*\*(?=\S)(.+?)(?<=\S)\*\*", r"<strong>\1</strong>", s)
    s = re.sub(r"(?<![\w*])\*(?=\S)([^*]+?)(?<=\S)\*(?![\w*])", r"<em>\1</em>", s)
    s = re.sub(r"\[([^\]]+)\]\(([^)\s]+)\)", lambda m: '<a href="%s">%s</a>' % (m.group(2), m.group(1)), s)   # [text](url): the text, linked
    s = re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], s)
    return s

# ---- block Markdown -------------------------------------------------------------------------------------

def table(lines):
    rows = []
    for ln in lines:
        ln = ln.strip()
        if ln.startswith("|"): ln = ln[1:]
        if ln.endswith("|"): ln = ln[:-1]
        rows.append([c.strip() for c in ln.split("|")])
    if len(rows) >= 2 and all(re.fullmatch(r":?-{2,}:?", c) for c in rows[1] if c):
        head, body = rows[0], rows[2:]
    else:
        head, body = None, rows
    out = ["<table>"]
    if head:
        out.append("<thead><tr>" + "".join("<th>%s</th>" % inline(c) for c in head) + "</tr></thead>")
    out.append("<tbody>")
    for r in body:
        out.append("<tr>" + "".join("<td>%s</td>" % inline(c) for c in r) + "</tr>")
    out.append("</tbody></table>")
    return "\n".join(out)

def blocks(text):
    """Markdown -> HTML for one page's worth of lines."""
    out, lines, i = [], text.split("\n"), 0
    while i < len(lines):
        ln = lines[i]
        s = ln.strip()
        if not s:
            i += 1; continue
        if s.startswith("<!--"):
            while i < len(lines) and "-->" not in lines[i]: i += 1
            i += 1; continue
        if s.startswith("<"):                                  # a raw HTML line (div, p.lead ...)
            out.append(s); i += 1; continue
        m = re.match(r"(#{1,3})\s+(.*)", s)
        if m:
            out.append("<h%d>%s</h%d>" % (len(m.group(1)), inline(m.group(2)), len(m.group(1)))); i += 1; continue
        if s.startswith("|"):
            j = i
            while j < len(lines) and lines[j].strip().startswith("|"): j += 1
            out.append(table(lines[i:j])); i = j; continue
        if re.match(r"[-*]\s+", s) or re.match(r"\d+\.\s+", s):
            ordered = bool(re.match(r"\d+\.", s))
            items, j = [], i
            while j < len(lines):
                t = lines[j].strip()
                if re.match(r"\d+\.\s+", t) if ordered else re.match(r"[-*]\s+", t):
                    items.append(re.sub(r"^(\d+\.|[-*])\s+", "", t))
                elif t and lines[j].startswith(" ") and items:           # a continued item
                    items[-1] += " " + t
                else:
                    break
                j += 1
            tag = "ol" if ordered else "ul"
            out.append("<%s>%s</%s>" % (tag, "".join("<li>%s</li>" % inline(x) for x in items), tag)); i = j; continue
        im = re.match(r"!\[([^\]]*)\]\(([^)]+)\)\s*$", lines[i].strip())
        if im:                                                    # an image, with its alt text as the caption
            src = (IMG_BASE / im.group(2)).resolve().as_uri() if IMG_BASE else im.group(2)
            out.append('<figure><img src="%s" alt="%s"><figcaption>%s</figcaption></figure>' % (src, html.escape(im.group(1)), inline(im.group(1))))
            i += 1
            continue
        j = i                                                     # a paragraph: lines up to a blank or a block start
        while j < len(lines) and lines[j].strip() and not re.match(r"(#{1,3}\s|\||[-*]\s|\d+\.\s|<)", lines[j].strip()):
            j += 1
        out.append("<p>%s</p>" % inline(" ".join(x.strip() for x in lines[i:j]))); i = j
    return "\n".join(out)

def convert(md):
    foot = ""
    m = re.search(r"<!--\s*footer:\s*(.*?)\s*-->", md, re.S)
    if m:
        foot = html.escape(m.group(1), quote=False)
        md = md[:m.start()] + md[m.end():]
    pages = [p for p in re.split(r"(?m)^(?:---+|<div class=\"page\"></div>)\s*$", md) if p.strip()]
    body = "<main>\n" + "\n".join("<section class=\"page\">\n%s\n</section>" % blocks(p) for p in pages) + "\n</main>"
    fcss = foot.replace("\\", "\\\\").replace('"', '\\"')
    body = ("<style>@page { @bottom-center { content: \"%s · page \" counter(page) \" of \" counter(pages); font-size: 6.8pt; color: #333; "
            "font-family: \"Segoe UI\", Arial, sans-serif; } }</style>\n" % fcss) + body
    title = re.search(r"(?m)^#\s+(.*)$", md)
    title = html.escape(title.group(1)) if title else "Quick manual"
    return ("<!doctype html>\n<html lang=\"en-GB\"><head><meta charset=\"utf-8\">\n<title>%s</title>\n"
            "<style>%s</style></head>\n<body>\n%s\n</body></html>\n" % (title, CSS, body))

# ---- the PDF -----------------------------------------------------------------------------------------------

def to_pdf(html_path, pdf_path, edge):
    # (only these flags: with --user-data-dir or --virtual-time-budget added, this Edge writes nothing)
    cmd = [edge, "--headless", "--disable-gpu", "--print-to-pdf=%s" % pdf_path, "--no-pdf-header-footer",
           html_path.as_uri()]
    if pdf_path.exists():
        pdf_path.unlink()
    subprocess.run(cmd, check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
    size = -1                                       # Edge returns at once and a child writes the file a moment later
    for _ in range(120):
        time.sleep(0.5)
        if pdf_path.exists():
            now = pdf_path.stat().st_size
            if now and now == size:
                return
            size = now
    sys.exit("Edge wrote no PDF (is it at %s? pass --edge PATH)" % edge)

def measure(pdf_path):
    """How tall each section of the sheet is, read from the PDF: a section's footer ("page N of 8") is on its last page
    only, so a section is 273 mm for each page before that plus, on its last page, the lowest text above the footer.
    (A page of another size would not do: Chrome scales the content on anything but A4.) For trimming the sheet."""
    try:
        from pypdf import PdfReader
    except ImportError:
        sys.exit("--measure needs pypdf (pip install pypdf)")
    rd = PdfReader(str(pdf_path))
    print("section heights (mm; a page holds 273, the footer takes the last 6):")
    carried, first = 0, ""
    for pg in rd.pages:
        H = float(pg.mediabox.height)
        items = []
        def visit(text, cm, tm, fd, fs, items=items):
            if text.strip():
                items.append((tm[4] * cm[1] + tm[5] * cm[3] + cm[5], text.strip()))   # the glyph's y on the page: tm x cm
        pg.extract_text(visitor_text=visit)
        items.sort()
        if not first:
            first = next((t for y, t in sorted(items, reverse=True)), "")[:50]
        foot = next((y for y, t in items if re.search(r"page \d+ of \d+", t)), None)
        if foot is None:                                   # a continuation page: full
            carried += 273
            continue
        body = [y for y, t in items if y > foot + 6]
        used = (H - min(body) - 12 * 72 / 25.4) / 72 * 25.4 + 3 if body else 0   # the top margin off, a line's depth on
        total = carried + used
        print("  %5.0f mm  %s%s" % (total, first, "" if total <= 267 else "   OVER by %.0f" % (total - 267)))
        carried, first = 0, ""

def page_report(pdf_path):
    try:
        from pypdf import PdfReader
    except ImportError:
        data = pdf_path.read_bytes()
        return len(re.findall(rb"/Type\s*/Page\b", data)), []
    rd = PdfReader(str(pdf_path))
    firsts = []
    for pg in rd.pages:
        txt = [t.strip() for t in (pg.extract_text() or "").splitlines() if t.strip()]
        firsts.append(txt[0][:70] if txt else "(no text)")
    return len(rd.pages), firsts

def main(argv):
    md_path, html_only, do_measure, edge = SRC / "docs" / "QUICK.md", False, False, os.environ.get("PERF1_EDGE", EDGE)
    stem, font = "quick-manual", None
    args = list(argv)
    while args:
        a = args.pop(0)
        if a == "--html-only": html_only = True
        elif a == "--measure": do_measure = True
        elif a == "--edge": edge = args.pop(0)
        elif a == "--out": stem = args.pop(0)
        elif a == "--font": font = args.pop(0)
        else: md_path = Path(a)
    out_dir = SRC / "build"
    out_dir.mkdir(exist_ok=True)
    html_path, pdf_path = out_dir / (stem + ".html"), out_dir / (stem + ".pdf")
    global IMG_BASE
    IMG_BASE = md_path.resolve().parent
    html_text = convert(md_path.read_text(encoding="utf-8"))
    if font:                                        # (another sheet, e.g. the beginner's guide, in a larger type)
        html_text = html_text.replace("html { font-size: 9pt; }", "html { font-size: %s; }" % font, 1)
    html_path.write_text(html_text, encoding="utf-8", newline="\n")
    print("wrote", html_path, "(%d bytes)" % html_path.stat().st_size)
    if html_only:
        return 0
    to_pdf(html_path, pdf_path, edge)
    n, firsts = page_report(pdf_path)
    print("wrote", pdf_path, "(%d bytes)" % pdf_path.stat().st_size)
    for i, f in enumerate(firsts, 1):
        print("  page %d: %s" % (i, f))
    print("pages:", n, "" if n == WANT_PAGES else "(wanted %d)" % WANT_PAGES)
    if do_measure:
        measure(pdf_path)
    return 0 if n == WANT_PAGES or stem != "quick-manual" else 1

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
