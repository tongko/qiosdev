#!/usr/bin/env python3
"""Bake a console font into a C array for the kernel framebuffer console.

Usage:
    python3 tools/genfont.py <font> <out.c> <out.h>
                             [--size WxH] [--pointsize N] [--threshold 0-255]
                             [--coverage] [--preview out.png]

<font> is either

    a PSF console font   /usr/share/kbd/consolefonts/default8x16.psfu.gz
                         (all 256 glyphs are taken from the font), or

    a TTF / OTF          resources/fonts/JetBrainsMonoNerdFont-Regular.ttf
                         (rendered with ImageMagick - Pillow is not needed;
                          ASCII 0x20..0x7E gets real glyphs, the rest is blank).

Two output formats:

    1 bit per pixel (default)  stride bytes per row, MSB leftmost.  Small and
                               crisp, but small anti-aliased text loses every
                               stroke that is not at least --threshold covered
                               (default 64), which is why a 50% threshold looks
                               broken.
    --coverage                 8 bits of coverage per pixel, w*h bytes per
                               glyph.  fb_console.c blends fg/bg by that
                               coverage, which is what makes a TTF look like it
                               does in a terminal.  Four times the data.

Never parse a font in the kernel - bake it here and embed the array.
"""
import gzip
import pathlib
import shutil
import subprocess
import sys

DEFAULT_W, DEFAULT_H = 8, 16
GLYPHS = 256
PSF_SUFFIXES = (".psf", ".psfu")
TTF_SUFFIXES = (".ttf", ".otf", ".ttc")


# ---------------------------------------------------------------- PSF input
def load_psf(path, cw, ch, coverage=False):
    data = gzip.open(path, "rb").read() if str(path).endswith(".gz") else pathlib.Path(path).read_bytes()

    if data[:2] == b"\x36\x04":  # PSF1
        mode, charsize = data[2], data[3]
        width, height = 8, charsize
        offset = 4
    elif data[:4] == b"\x72\xb5\x4a\x86":  # PSF2
        header = data[8:32]
        headersize, count, charsize, height, width = (
            int.from_bytes(header[0:4], "little"),
            int.from_bytes(header[8:12], "little"),
            int.from_bytes(header[12:16], "little"),
            int.from_bytes(header[16:20], "little"),
            int.from_bytes(header[20:24], "little"),
        )
        if count < GLYPHS:
            raise SystemExit(f"{path}: only {count} glyphs, need {GLYPHS}")
        offset = headersize
    else:
        raise SystemExit(f"{path}: not a PSF font (magic {data[:4].hex()})")

    stride = (width + 7) // 8
    plain = [data[offset + g * charsize:offset + g * charsize + stride * height] for g in range(GLYPHS)]

    if not coverage:
        return [(width, height)] + plain

    # Expand the 1-bit font to 0/255 coverage so both formats share one renderer.
    out = []
    for glyph in plain:
        cov = bytearray(width * height)
        for row in range(height):
            bits = glyph[row * stride:(row + 1) * stride]
            for col in range(width):
                if bits[col // 8] & (0x80 >> (col % 8)):
                    cov[row * width + col] = 255
        out.append(bytes(cov))
    return [(width, height)] + out


# ---------------------------------------------------------------- TTF input
def _magick():
    conv = shutil.which("magick") or shutil.which("convert")
    if conv is None:
        raise SystemExit(
            "rasterising a TTF needs ImageMagick (pacman -S imagemagick),\n"
            "or export the font to PSF/BDF and use that instead"
        )
    return conv


def _render(conv, ttf, pt, text, w, h, y):
    out = subprocess.run(
        [conv, "-size", f"{w}x{h}", "xc:black", "-font", str(ttf), "-pointsize", str(pt),
         "-fill", "white", "-gravity", "northwest", "-annotate", f"+0+{y}", text, "-depth", "8", "gray:-"],
        capture_output=True,
    )
    if out.returncode != 0 or len(out.stdout) != w * h:
        raise SystemExit(f"ImageMagick failed on {text!r}: {out.stderr.decode(errors='replace')[:200]}")
    return out.stdout


def _ink_box(img, w, h, thr=127):
    box = None
    for r in range(h):
        row = img[r * w:(r + 1) * w]
        for c, p in enumerate(row):
            if p > thr:
                box = (r, r, c, c) if box is None else (min(box[0], r), max(box[1], r), min(box[2], c), max(box[3], c))
    return box


def load_ttf(path, cw, ch, pointsize=None, threshold=64, coverage=False):
    """Rasterise ASCII with ImageMagick.

    A single-glyph render is positioned by its own bounding box, so drawing each
    character on its own makes the x-height letters hang from the top instead of
    sitting on a gemeinsame baseline.  Every glyph is therefore rendered
    together with a reference 'M' in the same image: characters inside one
    render always share a baseline, so the M's ink bottom is that render's
    baseline and the glyph's ink is shifted so all of them land on the same row.
    """
    conv = _magick()
    probe_h = ch * 2 + 16
    probe_w = cw * 4 + 32
    # cap, ascender, descenders, tallest verticals, and the high marks
    # (backtick, caret, quotes) - the probe decides how much vertical room the
    # auto-fit needs, so it has to contain whatever reaches highest and lowest.
    PROBE = 'Mgljpqy|`^\'"' 
    # Anything above this counts as ink when measuring where a glyph sits.  It
    # must be far below --threshold: a lone backtick is a couple of very light
    # pixels and would otherwise be measured as an empty glyph.
    box_thr = 8

    def box(img, c_from=0, c_to=None):
        c_to = probe_w if c_to is None else c_to
        found = None
        for r in range(probe_h):
            for c in range(c_from, c_to):
                if img[r * probe_w + c] > box_thr:
                    found = (r, r, c, c) if found is None else (
                        min(found[0], r), max(found[1], r), min(found[2], c), max(found[3], c))
        return found

    def measure(pt):
        m = box(_render(conv, path, pt, "M", probe_w, probe_h, 0))
        mm = box(_render(conv, path, pt, "MM", probe_w, probe_h, 0))
        if not (m and mm):
            return None
        adv = mm[3] - m[3]                      # one advance, measured in one render
        if adv <= 0:
            return None
        img = _render(conv, path, pt, PROBE, probe_w, probe_h, 0)
        m_ref = box(img, 0, adv)                # the M, sharing a baseline with the rest
        whole = box(img)
        if m_ref is None or whole is None:
            return None
        return {"adv": adv, "below": whole[1] - m_ref[1], "span": whole[1] - whole[0] + 1}

    if pointsize is not None:
        met = measure(pointsize)
        if met is None:
            raise SystemExit(f"{path}: nothing rendered at pointsize {pointsize}")
    else:
        met = None
        for pt in range(ch + 4, 4, -1):
            cand = measure(pt)
            if cand is not None and cand["adv"] <= cw and cand["span"] <= ch - 2:
                met, pointsize = cand, pt
                break
        if met is None:
            raise SystemExit(f"{path}: no point size fits a {cw}x{ch} cell, pass --size")

    adv = met["adv"]
    baseline_row = max(1, ch - 2 - met["below"])
    print(f"  {pathlib.Path(path).name}: {pointsize}pt -> {cw}x{ch} cell, advance {adv}px, "
          f"baseline row {baseline_row}, {met['below']}px below{', coverage' if coverage else ''}")
    if adv > cw:
        print(f"  warning: advance {adv}px does not fit the {cw}px cell")

    stride = (cw + 7) // 8
    glyphs = []
    widest = 0
    for code in range(GLYPHS):
        rows = bytearray(cw * ch if coverage else stride * ch)
        if 0x20 <= code <= 0x7E:
            # ImageMagick's text parser eats a lone backslash, so double it.
            img = _render(conv, path, pointsize,
                          "M" + ("\\\\" if chr(code) == "\\" else chr(code)), probe_w, probe_h, 0)
            m_box = box(img, 0, adv)            # reference capital on this render's baseline
            g_box = box(img, adv, probe_w)      # the glyph itself
            if m_box is not None and g_box is not None:
                dy = baseline_row - m_box[1]    # one shared baseline for every glyph
                if g_box[0] + dy < 0:
                    # A mark that reaches above the cell (the backtick, on some
                    # faces) would be dropped entirely; slide it down instead.
                    dy = -g_box[0]
                gw = g_box[3] - g_box[2] + 1
                widest = max(widest, gw)
                dx = max(0, (cw - gw) // 2) - g_box[2]
                for r in range(g_box[0], g_box[1] + 1):
                    cr = r + dy
                    if not (0 <= cr < ch):
                        continue
                    for c in range(g_box[2], g_box[3] + 1):
                        cc = c + dx
                        if not (0 <= cc < cw):
                            continue
                        v = img[r * probe_w + c]
                        if coverage:
                            rows[cr * cw + cc] = v
                        elif v > threshold:
                            rows[cr * stride + cc // 8] |= 0x80 >> (cc % 8)
        glyphs.append(bytes(rows))
    if widest > cw:
        print(f"  warning: widest glyph is {widest}px, cell is only {cw}px (try --size {widest}x{ch})")
    return [(cw, ch)] + glyphs


# ---------------------------------------------------------------- output
def emit(glyphs, source, coverage):
    width, height = glyphs[0]
    how = "8 bits of coverage per pixel." if coverage else f"{(width + 7) // 8} byte(s) per row, MSB leftmost."
    lines = [
        f"/* Generated by tools/genfont.py from {source} - do not edit by hand.",
        f" * Cell {width}x{height}, {how} */",
        "#include <kernel/devices/font8x16.h>",
        "",
        "const uint8_t font8x16[FONT8X16_GLYPHS][FONT8X16_BYTES] = {",
    ]
    for i, g in enumerate(glyphs[1:]):
        printable = chr(i) if 0x20 <= i < 0x7F else "."
        lines.append("\t{ %s }, /* 0x%02x '%s' */" % (", ".join("0x%02x" % b for b in g), i, printable))
    lines += ["};", ""]
    return "\n".join(lines)


def header(width, height, coverage):
    stride = (width + 7) // 8
    if coverage:
        detail = f"one byte of coverage (0..255) per pixel, {width} bytes per row"
        nbytes = width * height
    else:
        detail = f"{stride} byte(s) per row ({stride * 8} px of width, MSB leftmost), {height} rows"
        nbytes = stride * height
    return f"""#pragma once

/* Console font: {detail}, cell {width}x{height}.
 * Regenerate with tools/genfont.py. */

#include <stdint.h>

#define FONT8X16_W {width}
#define FONT8X16_H {height}
#define FONT8X16_GLYPHS {GLYPHS}
#define FONT8X16_STRIDE {stride}
#define FONT8X16_BYTES {nbytes}
#define FONT8X16_COVERAGE {1 if coverage else 0}

extern const uint8_t font8x16[FONT8X16_GLYPHS][FONT8X16_BYTES];
"""


def write_preview(glyphs, path, coverage, cols=16):
    """A PGM/PBM of every baked glyph, so the result can be eyeballed."""
    width, height = glyphs[0]
    stride = (width + 7) // 8
    rows = (GLYPHS + cols - 1) // cols
    iw, ih = cols * width, rows * height
    img = bytearray(iw * ih)
    for py in range(ih):
        cell, y = divmod(py, height)
        for px in range(iw):
            gx, x = divmod(px, width)
            g = glyphs[1:][cell * cols + gx]
            if coverage:
                img[py * iw + px] = g[y * width + x]
            else:
                bit = (g[y * stride + x // 8] >> (7 - (x % 8))) & 1
                img[py * iw + px] = 255 if bit else 0
    if coverage:
        raw = path.with_suffix(".pgm")
        raw.write_bytes(b"P5\n%d %d\n255\n" % (iw, ih) + bytes(img))
    else:
        packed = bytearray()
        for py in range(ih):
            for i in range(0, iw, 8):
                b = 0
                for j in range(8):
                    b = (b << 1) | (0 if img[py * iw + i + j] else 1)
                packed.append(b)
        raw = path.with_suffix(".pbm")
        raw.write_bytes(b"P4\n%d %d\n" % (iw, ih) + bytes(packed))
    subprocess.run([_magick(), str(raw), "-scale", "300%", str(path)], check=True)
    raw.unlink()
    print(f"  preview: {path}")


def main() -> int:
    args = sys.argv[1:]
    positional = []
    size = (DEFAULT_W, DEFAULT_H)
    pointsize = None
    threshold = 64
    coverage = False
    preview = None

    i = 0
    while i < len(args):
        arg = args[i]
        if arg == "--size":
            i += 1
            w, _, h = args[i].partition("x")
            size = (int(w), int(h))
        elif arg == "--pointsize":
            i += 1
            pointsize = int(args[i])
        elif arg == "--threshold":
            i += 1
            threshold = int(args[i])
        elif arg == "--coverage":
            coverage = True
        elif arg == "--preview":
            i += 1
            preview = pathlib.Path(args[i])
        elif arg.startswith("--"):
            print(f"unknown option {arg}\n")
            print(__doc__)
            return 2
        else:
            positional.append(arg)
        i += 1

    if len(positional) != 3:
        print(__doc__)
        return 2

    src, out_c, out_h = pathlib.Path(positional[0]), pathlib.Path(positional[1]), pathlib.Path(positional[2])
    name = src.name[:-3] if src.name.endswith(".gz") else src.name
    suffix = pathlib.Path(name).suffix.lower()
    if suffix in PSF_SUFFIXES:
        glyphs = load_psf(src, *size, coverage)
    elif suffix in TTF_SUFFIXES:
        glyphs = load_ttf(src, *size, pointsize, threshold, coverage)
    else:
        print(f"{src}: expected a PSF or TTF/OTF font (got '{suffix}')")
        return 2

    out_c.parent.mkdir(parents=True, exist_ok=True)
    out_h.parent.mkdir(parents=True, exist_ok=True)
    out_c.write_text(emit(glyphs, src.name, coverage))
    out_h.write_text(header(*glyphs[0], coverage))
    w, h = glyphs[0]
    print(f"  {out_h}: {GLYPHS} glyphs, {w}x{h}, {'coverage' if coverage else '1 bit'}")
    print(f"  {out_c}: {out_c.stat().st_size} bytes")
    if preview is not None:
        write_preview(glyphs, preview, coverage)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
