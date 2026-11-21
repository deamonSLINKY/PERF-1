"""The top bar every PERF-1 web page shares: the logo, the page links, Buy me a coffee, in the same place on each page.

The bar is placed against the window (not inside each page's own layout), so it sits at the same spot whatever the page's
widths and paddings are. It is written between <!--p1top--> and <!--/p1top--> in each page's source; edit it here and run

    python -I tools/sync_top.py          (rewrites every page)
    python -I tools/sync_top.py --check  (exit 1 if a page's copy differs: the tests run this)
"""
import base64, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
LOGO = "data:image/png;base64," + base64.b64encode((ROOT / "web" / "img" / "slinky.png").read_bytes()).decode()

LINKS = [("emu", "Emulator"), ("editor", "Editor"), ("studio", "Studio"), ("pieces", "Library &amp; pieces"),
         ("install", "Install"), ("manual", "Manual")]

# (file, the page it is (None: the front page), how deep it sits: "../" for the pages one folder down)
PAGES = [("web/amb_installer.html", "install", "../"), ("web/emu/index.html", "emu", "../"),
         ("web/editor/index.html", "editor", "../"), ("web/pieces/index.html", "pieces", "../"),
         ("web/studio/index.html", "studio", "../"), ("docs/manual.html", "manual", "../"),
         ("tools/manual_html.py", "manual", "../"), ("tools/make_pages_site.py", None, "")]

H = 80      # the bar's height on a wide window
HN = 150    # on a phone (the links in two rows of three under the logo)

CSS = (
    "html{scrollbar-gutter:stable}"
    "#p1top{--p1-ink:#f2efe6;--p1-acc:#a58bff;--p1-line:#7253e6;--p1-dim:#cfc8ff}"
    "#p1top,#p1top *{all:unset;box-sizing:border-box}"
    "#p1top{display:block;position:absolute;top:0;left:0;right:0;height:%dpx;z-index:60;font:500 16px/1.2 \"Chakra Petch\",\"Helvetica Neue\",Arial,sans-serif;color:var(--p1-ink)}"
    "#p1top .in{display:flex;align-items:center;gap:16px;position:relative;max-width:1240px;height:100%%;margin:0 auto;padding:14px 16px 0}"
    "#p1top .home{display:inline-flex;align-items:center;gap:10px;cursor:pointer;font:700 21px/1 \"Chakra Petch\",\"Helvetica Neue\",Arial,sans-serif;letter-spacing:.06em;color:var(--p1-ink)}"
    "#p1top .home img{display:block;width:46px;height:46px;image-rendering:pixelated}"
    "#p1top .nav{display:flex;flex-wrap:nowrap;gap:18px;margin-left:auto;white-space:nowrap}"
    "#p1top .nav a{display:inline;cursor:pointer;color:var(--p1-ink);text-decoration:underline;text-decoration-color:var(--p1-line);text-decoration-thickness:2px;text-underline-offset:5px}"
    "#p1top .nav a:hover{color:var(--p1-acc)}"
    "#p1top .nav a[aria-current]{color:var(--p1-acc);text-decoration:none}"
    "#p1top .kofi{display:block;position:absolute;top:6px;right:16px;cursor:pointer;font-size:13px;color:var(--p1-dim);text-decoration:underline;text-decoration-color:var(--p1-line);text-underline-offset:3px}"
    "#p1top a:focus-visible{outline:2px solid var(--p1-acc);outline-offset:3px}"
    "#p1top-space{display:block;height:%dpx}"
    "@media (max-width:760px){#p1top{height:%dpx}#p1top-space{height:%dpx}#p1top .in{flex-wrap:wrap;align-content:flex-start;gap:8px 16px;padding-top:22px}"
    "#p1top .nav{display:grid;grid-template-columns:repeat(3,max-content);gap:10px 20px;margin-left:0;width:100%%}}"
    "@media print{#p1top,#p1top-space{display:none}}"
) % (H, H, HN, HN)


def block(page, up):
    nav = "".join('<a href="%s%s/"%s>%s</a>' % (up, key, ' aria-current="page"' if key == page else "", name) for key, name in LINKS)
    home = up if up else "./"
    cur = ' aria-current="page"' if page is None else ""
    return ('<!--p1top--><link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Chakra+Petch:wght@500;700&amp;display=swap">'
            '<style>' + CSS + '</style><div id="p1top"><div class="in">'
            '<a class="home" href="' + home + '"' + cur + '><img src="' + LOGO + '" width="46" height="46" alt="">PERF-1</a>'
            '<div class="nav" role="navigation" aria-label="Pages">' + nav + '</div>'
            '<a class="kofi" href="https://ko-fi.com/deamonslinky" target="_blank" rel="noopener">&#9749; Buy me a coffee</a>'
            '</div></div><div id="p1top-space"></div><!--/p1top-->')


PAT = re.compile(r"<!--p1top-->.*?<!--/p1top-->", re.S)


def main():
    check = "--check" in sys.argv
    bad = 0
    for rel, page, up in PAGES:
        p = ROOT / rel
        text = p.read_text(encoding="utf-8")
        n = len(PAT.findall(text))
        if n != 1:
            print(f"{rel}: {n} top bars (want 1)")
            bad += 1
            continue
        new = PAT.sub(lambda m: block(page, up), text)
        if new != text:
            if check:
                print(f"{rel}: its top bar differs (run tools/sync_top.py)")
                bad += 1
            else:
                with open(p, "w", encoding="utf-8", newline="") as f:
                    f.write(new)
                print(f"{rel}: written")
    if bad:
        sys.exit(1)
    print("top bars: %d pages %s" % (len(PAGES), "the same" if check else "in step"))


if __name__ == "__main__":
    main()
