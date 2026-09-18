"""Measure widescreen Mode 7 draw distance against retail's streamed window.

Retail streams the Mode 7 tilemap. `$03:9243` keeps exactly one 1024x1024-unit
world square uploaded, anchored at `$00A8`/`$00AA` (`$00:97C3`: camera minus
512, plus the `$0A:ED00` look-ahead, slew-clamped by `$03:92AA`). The tilemap
is 128x128 tiles - 1024x1024 pixels - so the square fills it completely and the
map aliases the 8192x4096 world every 1024 units. Any sample outside the square
reads whatever tiles another part of the course left behind.

The stock 256-pixel viewport is what the square is sized for. A widened
viewport samples further to each side, so its margins read outside the square
and show course content that "pops in" once it reaches stock width.

Two numbers per aspect, over every race frame ($54=2, $55>=3) in a directory of
`tools/run_capture.py` captures:

  outside   share of Mode 7 pixels sampling outside the streamed square.
            A property of the camera and the viewport, so a renderer cannot
            change it.
  wrong     share whose live tilemap tile differs from the tile retail's own
            course tables give for that world position - the pop-in itself.
            The compositor resolves these from the tables, so it is the
            before/after figure.

`--verify` additionally checks the table walk against the live tilemap for
every cell inside the square, which must agree exactly.

    py -3 tools/measure_draw_distance.py captures/<name> [--verify]
"""
import argparse
import glob
import struct

LINE = 0x40 + 512 + 512 + 32
OFFSET_VRAM = LINE * 224
OFFSET_RAM = OFFSET_VRAM + 0x8000 * 2 + 256 * 224 * 4
OFFSET_FRAME = OFFSET_RAM + 0x20000
REGISTER = {"bgmode": 4, "m7sel": 12, "m7matrix": 30}
ASPECTS = (("4:3", 0), ("16:9", 43), ("21:9", 96), ("32:9", 213))


class Capture:
    """One `FzeroSourceFrame` as `FzeroRendererEndFrame` wrote it."""

    def __init__(self, path):
        with open(path, "rb") as handle:
            self.data = handle.read()
        if len(self.data) < OFFSET_FRAME + 4:
            raise SystemExit("%s: not a frame capture" % path)
        self.frame = struct.unpack_from("<I", self.data, OFFSET_FRAME)[0]
        self.bank7f = OFFSET_RAM + 0x10000
        self.grid = self.byte(0xB0) | (self.byte(0xB1) << 8)
        # $03:9346/$03:9381 select the streamed strip with ($14 & $03F0) and
        # ($12 & $03F0), so the square starts on a 16-unit block boundary.
        self.anchor_x = self.word(0xA8) & 0x1FF0
        self.anchor_y = self.word(0xAA) & 0x0FF0

    def register(self, line, name):
        return self.data[line * LINE + REGISTER[name]]

    def matrix(self, line):
        return struct.unpack_from("<8h", self.data, line * LINE + REGISTER["m7matrix"])

    def word(self, address):
        return struct.unpack_from("<H", self.data, OFFSET_RAM + address)[0]

    def byte(self, address):
        return self.data[OFFSET_RAM + address]

    def tilemap(self, tile_x, tile_y):
        """Live Mode 7 tilemap byte: the low half of VRAM word ty*128+tx."""
        return self.data[OFFSET_VRAM + (tile_y * 128 + tile_x) * 2]

    def bank_byte(self, address):
        return self.data[self.bank7f + (address & 0xFFFF)]

    def bank_word(self, address):
        return self.bank_byte(address) | (self.bank_byte(address + 1) << 8)

    def course_tile(self, world_x, world_y):
        """Retail's three-level course walk, $03:93BF..$03:93E7.

        ($B0),Y selects a block from a 32x16 grid of 256-unit cells; the block
        id times 32 picks one of sixteen 16-unit sub-rows in the $5000 pointer
        table; that sub-row lists sixteen pointers to 2x2 tile groups, stored
        (x0,y0) (x0,y1) (x1,y0) (x1,y1).
        """
        world_x &= 0x1FFF
        world_y &= 0x0FFF
        block = self.bank_byte(self.grid + ((world_y >> 8) & 15) * 32 +
                               ((world_x >> 8) & 31))
        row = self.bank_word(0x5000 + block * 32 + ((world_y >> 4) & 15) * 2)
        group = self.bank_word(row + ((world_x >> 4) & 15) * 2)
        return self.bank_byte(group + ((world_x >> 3) & 1) * 2 +
                              ((world_y >> 3) & 1))

    def inside_square(self, world_x, world_y):
        return ((world_x - self.anchor_x) & 0x1FFF) < 1024 and \
               ((world_y - self.anchor_y) & 0x0FFF) < 1024


def sign13(value):
    value &= 0x1FFF
    return value - 0x2000 if value & 0x1000 else value


def clip10(value):
    return (value | ~1023) if value & 0x2000 else (value & 1023)


def transform(matrix, control, scanline):
    """Mirror of FzeroMode7Transform; returns Q8 origin, step and centre."""
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
    """[outside, wrong, total] for the centre columns and for the margins."""
    world_x, world_y = capture.word(0xB70), capture.word(0xB90)
    centre, margin = [0, 0, 0], [0, 0, 0]
    for line in range(224):
        if capture.register(line, "bgmode") & 7 != 7:
            continue
        origin_x, origin_y, step_x, step_y, cx, cy = transform(
            capture.matrix(line), capture.register(line, "m7sel"), line + 1)
        for screen_x in range(-extra, 256 + extra):
            texel_x = (origin_x + screen_x * step_x) // 256
            texel_y = (origin_y + screen_x * step_y) // 256
            sample_x = (world_x + texel_x - cx) & 0x1FFF
            sample_y = (world_y + texel_y - cy) & 0x0FFF
            bucket = centre if 0 <= screen_x < 256 else margin
            bucket[2] += 1
            if capture.inside_square(sample_x, sample_y):
                continue
            bucket[0] += 1
            live = capture.tilemap((texel_x & 1023) >> 3, (texel_y & 1023) >> 3)
            if live != capture.course_tile(sample_x, sample_y):
                bucket[1] += 1
    return centre, margin


def verify(capture):
    """Table walk against the live tilemap for every cell inside the square."""
    agree = total = 0
    for row in range(128):
        sample_y = capture.anchor_y + row * 8
        for column in range(128):
            sample_x = capture.anchor_x + column * 8
            total += 1
            agree += capture.course_tile(sample_x, sample_y) == \
                capture.tilemap((sample_x & 1023) >> 3, (sample_y & 1023) >> 3)
    return agree, total


def report(folder, do_verify):
    captures = []
    for path in sorted(glob.glob(folder + "/frame-*.bin")):
        capture = Capture(path)
        if capture.byte(0x54) == 2 and capture.byte(0x55) >= 3:
            captures.append(capture)
    if not captures:
        print("%s: no race captures" % folder)
        return
    print("%s: %d race captures" % (folder, len(captures)))
    if do_verify:
        agree = total = 0
        for capture in captures:
            one, two = verify(capture)
            agree += one
            total += two
        print("  course tables vs live tilemap inside the square: "
              "%d/%d cells, %.4f%%" % (agree, total, 100.0 * agree / total))
    print("  %-6s %6s  %9s %9s  %9s %9s" %
          ("aspect", "width", "outside", "wrong", "outside", "wrong"))
    print("  %-6s %6s  %19s  %19s" % ("", "", "centre columns", "widened margins"))
    for name, extra in ASPECTS:
        centre, margin = [0, 0, 0], [0, 0, 0]
        for capture in captures:
            one, two = measure(capture, extra)
            for i in range(3):
                centre[i] += one[i]
                margin[i] += two[i]
        share = lambda part, total: 100.0 * part / total if total else 0.0
        print("  %-6s %6d  %8.3f%% %8.3f%%  %8.3f%% %8.3f%%" % (
            name, 256 + 2 * extra,
            share(centre[0], centre[2]), share(centre[1], centre[2]),
            share(margin[0], margin[2]), share(margin[1], margin[2])))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folders", nargs="+")
    parser.add_argument("--verify", action="store_true")
    arguments = parser.parse_args()
    for folder in arguments.folders:
        report(folder, arguments.verify)
