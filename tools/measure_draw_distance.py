"""Measure widescreen Mode 7 draw distance against retail's streamed window.

Retail streams the Mode 7 tilemap. `$03:9243` keeps exactly one 1024x1024-unit
world square uploaded, anchored at `$0020/$0022` (`$00:97C3`: camera minus 512,
plus the `$0A:ED00` look-ahead, clamped to +/-256). The tilemap is 128x128
tiles - 1024x1024 pixels - so the square fills it completely and the map
aliases the 8192x4096 world every 1024 units. Any sample outside that square
reads whatever tiles another part of the course left behind.

The stock 256-pixel viewport is what the square is sized for. A widened
viewport samples further to each side, so its margins read outside the square
and show course content that later "pops in" once it reaches stock width.

This reports, per aspect, the share of Mode 7 pixels whose sample falls outside
the streamed square, split into the stock centre columns and the widened
margins. It needs a directory of `tools/run_capture.py` frame captures; only
race frames ($54=2, $55>=3) are counted.

    py -3 tools/measure_draw_distance.py captures/<name> [captures/<name>...]
"""
import argparse
import glob
import struct
import sys

LINE = 0x40 + 512 + 512 + 32
OFFSET_VRAM = LINE * 224
OFFSET_RAM = OFFSET_VRAM + 0x8000 * 2 + 256 * 224 * 4
OFFSET_FRAME = OFFSET_RAM + 0x20000
REGISTER = {"bgmode": 4, "m7sel": 12, "m7matrix": 30}
ASPECTS = (("4:3", 0), ("16:9", 43), ("21:9", 96), ("32:9", 213))


class Capture:
    def __init__(self, path):
        with open(path, "rb") as handle:
            self.data = handle.read()
        if len(self.data) < OFFSET_FRAME + 4:
            raise SystemExit("%s: not a frame capture" % path)
        self.frame = struct.unpack_from("<I", self.data, OFFSET_FRAME)[0]

    def register(self, line, name):
        return self.data[line * LINE + REGISTER[name]]

    def matrix(self, line):
        return struct.unpack_from("<8h", self.data, line * LINE + REGISTER["m7matrix"])

    def word(self, address):
        return struct.unpack_from("<H", self.data, OFFSET_RAM + address)[0]

    def byte(self, address):
        return self.data[OFFSET_RAM + address]


def sign13(value):
    value &= 0x1FFF
    return value - 0x2000 if value & 0x1000 else value


def clip10(value):
    return (value | ~1023) if value & 0x2000 else (value & 1023)


def transform(matrix, control, scanline):
    """Mirror of FzeroMode7Transform; returns Q8 origin and per-pixel step."""
    centre_x, centre_y = sign13(matrix[4]), sign13(matrix[5])
    h = clip10(sign13(matrix[6]) - centre_x)
    v = clip10(sign13(matrix[7]) - centre_y)
    y = 255 - scanline if control & 2 else scanline
    origin_x = (matrix[0] * h & ~63) + (matrix[1] * y & ~63) + \
               (matrix[1] * v & ~63) + centre_x * 256
    origin_y = (matrix[2] * h & ~63) + (matrix[3] * y & ~63) + \
               (matrix[3] * v & ~63) + centre_y * 256
    step_x, step_y = matrix[0], matrix[2]
    if control & 1:
        origin_x += step_x * 255
        origin_y += step_y * 255
        step_x, step_y = -step_x, -step_y
    return origin_x, origin_y, step_x, step_y, centre_x, centre_y


def measure(capture, extra):
    """(outside, total) Mode 7 pixels for the centre and for the margins."""
    anchor_x = capture.word(0x20) & 0x1FFF
    anchor_y = capture.word(0x22) & 0x0FFF
    world_x, world_y = capture.word(0xB70), capture.word(0xB90)
    centre = [0, 0]
    margin = [0, 0]
    for line in range(224):
        if capture.register(line, "bgmode") & 7 != 7:
            continue
        matrix = capture.matrix(line)
        origin_x, origin_y, step_x, step_y, cx, cy = transform(
            matrix, capture.register(line, "m7sel"), line + 1)
        for screen_x in range(-extra, 256 + extra):
            texel_x = (origin_x + screen_x * step_x) // 256
            texel_y = (origin_y + screen_x * step_y) // 256
            sample_x = (world_x + texel_x - cx) & 0x1FFF
            sample_y = (world_y + texel_y - cy) & 0x0FFF
            outside = ((sample_x - anchor_x) & 0x1FFF) >= 1024 or \
                      ((sample_y - anchor_y) & 0x0FFF) >= 1024
            bucket = centre if 0 <= screen_x < 256 else margin
            bucket[0] += outside
            bucket[1] += 1
    return centre, margin


def report(folder):
    captures = []
    for path in sorted(glob.glob(folder + "/frame-*.bin")):
        capture = Capture(path)
        if capture.byte(0x54) == 2 and capture.byte(0x55) >= 3:
            captures.append(capture)
    if not captures:
        print("%s: no race captures" % folder)
        return
    print("%s: %d race captures" % (folder, len(captures)))
    print("  %-6s %6s %12s %12s %12s" %
          ("aspect", "width", "centre", "margins", "frame"))
    for name, extra in ASPECTS:
        centre = [0, 0]
        margin = [0, 0]
        for capture in captures:
            one, two = measure(capture, extra)
            centre[0] += one[0]; centre[1] += one[1]
            margin[0] += two[0]; margin[1] += two[1]
        whole = (centre[0] + margin[0]) / float(centre[1] + margin[1])
        print("  %-6s %6d %11.3f%% %11.3f%% %11.3f%%" % (
            name, 256 + 2 * extra, 100.0 * centre[0] / centre[1],
            100.0 * margin[0] / margin[1] if margin[1] else 0.0, 100.0 * whole))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folders", nargs="+")
    for folder in parser.parse_args().folders:
        report(folder)
