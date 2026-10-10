#!/usr/bin/env python3
"""Generates src/Web/PagesGz.h from the INDEX_PAGE literal in src/Web/Pages.h.

Run after every edit to Pages.h:

    python tools/gzip_page.py

The sim page is served to phones joined to an access point with no
internet, so the phone keeps its Wi-Fi radio in power-save and every
TCP burst waits for a radio wake-up (100-300 ms). Gzip cuts the
transfer 4-6x, which keeps the page load under a second.
"""
import gzip
import re
import sys
from pathlib import Path

WEB_DIR = Path(__file__).resolve().parent.parent / "src" / "Web"
SOURCE = WEB_DIR / "Pages.h"
OUTPUT = WEB_DIR / "PagesGz.h"


def main():
    match = re.search(rb'R"PAGE\((.*?)\)PAGE";', SOURCE.read_bytes(), re.S)
    if not match:
        sys.exit("INDEX_PAGE literal not found in " + str(SOURCE))
    page = match.group(1)
    compressed = gzip.compress(page, 9)
    rows = []
    for i in range(0, len(compressed), 16):
        rows.append("    " + ",".join(str(b) for b in compressed[i:i + 16]) + ",")
    body = "\n".join(rows).rstrip(",")
    OUTPUT.write_text(
        "#pragma once\n"
        "\n"
        "// Generated from Pages.h (INDEX_PAGE) by tools/gzip_page.py.\n"
        "// Regenerate after every edit to the page source.\n"
        "const unsigned char PAGE_GZ[] PROGMEM = {\n"
        + body + "\n"
        "};\n"
        "constexpr const unsigned int PAGE_GZ_LEN = sizeof(PAGE_GZ);\n"
    )
    print("page bytes=%d gzip bytes=%d ratio=%.2f"
          % (len(page), len(compressed), len(compressed) / float(len(page))))


if __name__ == "__main__":
    main()
