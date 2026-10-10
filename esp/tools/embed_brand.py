#!/usr/bin/env python3
"""Generates src/Web/Brand.h from the resized brand images in assets/.

Pipeline (run after replacing a brand image):

    1. Save the original in assets/ (company.png, product.png,
       machine.png).
    2. Resize to web size as JPEG (see the resize step in the
       project notes): logos ~180-200 px wide, product photo
       ~400 px wide, JPEG quality 65-75.
    3. Run: python tools/embed_brand.py

The images are served by the sim web UI itself, so the page
renders fully on devices joined to the sim AP, which has no
internet access.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
OUTPUT = ROOT / "src" / "Web" / "Brand.h"

IMAGES = [
    ("COMPANY", "company.jpg"),
    ("PRODUCT", "product.jpg"),
    ("MACHINE", "machine.jpg"),
]


def main():
    chunks = [
        "#pragma once",
        "",
        "// Generated from assets/*.jpg by tools/embed_brand.py.",
        "// Brand imagery served by the sim web UI so the page",
        "// renders fully on devices joined to the sim AP, which",
        "// has no internet access. Regenerate after replacing",
        "// the images in assets/.",
        "",
        "#include <stdint.h>",
        "",
    ]
    for name, fname in IMAGES:
        data = (ASSETS / fname).read_bytes()
        chunks.append(f"// {fname} ({len(data)} bytes)")
        chunks.append(
            f"const unsigned char BRAND_{name}[] PROGMEM = {{"
        )
        rows = []
        for i in range(0, len(data), 16):
            rows.append(
                "    "
                + ",".join(str(b) for b in data[i:i + 16])
                + ","
            )
        chunks.append("\n".join(rows).rstrip(","))
        chunks.append("};")
        chunks.append(
            f"constexpr const unsigned int BRAND_{name}_LEN"
            " = sizeof(BRAND_" + name + ");"
        )
        chunks.append("")
    OUTPUT.write_text("\n".join(chunks))
    total = sum((ASSETS / f).stat().st_size for _, f in IMAGES)
    print(f"brand images: {total} bytes total")


if __name__ == "__main__":
    main()
