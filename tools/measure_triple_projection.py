"""Check whether one race capture fits the experimental physical-ground model.

Usage: py -3 tools/measure_triple_projection.py <frame.bin>
This is a diagnostic; passing does not imply sprites/HUD can be projected.
"""

import argparse
import math

from measure_draw_distance import Capture, transform

WIDTH_MM = 708.4165965748336
HEIGHT_MM = 398.4843355733439
EYE_MM = 660.0
LOGICAL_WIDTH = 342
PERIOD = 1024.0


def wrapped_delta(a, b):
    return math.remainder(a - b, PERIOD)


def line(capture, y):
    value = transform(capture.matrix(y), capture.register(y, "m7sel"), y + 1)
    ox, oy, dx, dy = value[:4]
    scale = math.hypot(dx, dy) / 256
    return scale, ((ox + 128 * dx) / 256, (oy + 128 * dy) / 256), (dx, dy)


def distance(height, sine, cosine, y):
    screen_y = (112 - y) * HEIGHT_MM / 224
    denominator = EYE_MM * sine - screen_y * cosine
    if denominator <= 0:
        return None
    return height * (EYE_MM * cosine + screen_y * sine) / denominator


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture")
    args = parser.parse_args()
    capture = Capture(args.capture)
    far_y, near_y = 80, 180
    far_scale, far_center, _ = line(capture, far_y)
    near_scale, near_center, near_step = line(capture, near_y)
    if not far_scale > near_scale > 0:
        raise SystemExit("not a perspective Mode 7 race frame")
    slope = (1 / near_scale - 1 / far_scale) / (near_y - far_y)
    intercept = 1 / far_scale + slope * (112 - far_y)
    tangent = HEIGHT_MM * intercept / (224 * EYE_MM * slope)
    cosine = 1 / math.sqrt(1 + tangent * tangent)
    sine = tangent * cosine
    height = LOGICAL_WIDTH * HEIGHT_MM * cosine / (WIDTH_MM * 224 * slope)
    right = (near_step[0] / (256 * near_scale),
             near_step[1] / (256 * near_scale))
    forward = (-right[1], right[0])
    expected_delta = distance(height, sine, cosine, far_y) - \
        distance(height, sine, cosine, near_y)
    delta = [wrapped_delta(a, b) for a, b in zip(far_center, near_center)]
    projected = sum(d * f for d, f in zip(delta, forward))
    if projected < 0:
        forward = (-forward[0], -forward[1])
        projected = -projected
    forward_scale = projected / expected_delta
    near_distance = distance(height, sine, cosine, near_y)
    camera = (near_center[0] - forward[0] * near_distance * forward_scale,
              near_center[1] - forward[1] * near_distance * forward_scale)
    print(f"frame={capture.frame} pitch={math.degrees(math.atan(tangent)):.3f}deg "
          f"camera_height={height:.2f} texture units "
          f"forward_scale={forward_scale:.4f}")
    print("scanline  scale_error%  center_error_texels")
    for y in range(60, 211, 10):
        if capture.register(y, "bgmode") & 7 != 7:
            continue
        actual_scale, actual_center, _ = line(capture, y)
        screen_y = (112 - y) * HEIGHT_MM / 224
        predicted_scale = height * WIDTH_MM / (LOGICAL_WIDTH *
            (EYE_MM * sine - screen_y * cosine))
        along = distance(height, sine, cosine, y)
        predicted_center = (camera[0] + forward[0] * along * forward_scale,
                            camera[1] + forward[1] * along * forward_scale)
        error = math.hypot(*(wrapped_delta(a, b) for a, b in
                             zip(actual_center, predicted_center)))
        print(f"{y:>8} {100 * (predicted_scale / actual_scale - 1):>12.2f} "
              f"{error:>20.2f}")


if __name__ == "__main__":
    main()
