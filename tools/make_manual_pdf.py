#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The whole manual as a PDF: docs/manual.html (tools/manual_html.py, with its print stylesheet) printed by Edge headless on Windows
to build/manual.pdf; tools/make_pages_site.py copies it to the site as manual/PERF-1-manual.pdf, next to the quick manual.
  python tools/make_manual_pdf.py [--edge PATH]     (PERF1_EDGE in the environment also names Edge)"""
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE.parent
sys.path.insert(0, str(HERE))
from make_quick_manual import EDGE, to_pdf  # noqa: E402  (the same Edge call and the wait for the file to settle)


def main(argv):
    edge = os.environ.get("PERF1_EDGE", EDGE)
    if "--edge" in argv:
        edge = argv[argv.index("--edge") + 1]
    subprocess.run([sys.executable, "-I", str(HERE / "manual_html.py")], check=True)
    pdf = SRC / "build" / "manual.pdf"
    to_pdf(SRC / "docs" / "manual.html", pdf, edge)
    data = pdf.read_bytes()
    pages = len(re.findall(rb"/Type\s*/Page[^s]", data))
    print("wrote", pdf, "(%d bytes, about %d pages)" % (len(data), pages))
    return 0 if pages > 10 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
