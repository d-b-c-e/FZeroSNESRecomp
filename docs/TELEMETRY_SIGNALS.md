# F-Zero telemetry signal investigation

The desktop build currently sends a 324-byte Forza Horizon packet to the
configured UDP destination. It contains race state, approximate world-position
speed and distance, yaw, vehicle ordinal, energy as fuel, and digital driving
inputs. It does not claim suspension, tire, surface, collision, or actual
engine data. `currentEngineRpm` is a speed-derived dashboard approximation.

## Verified crash signal

The same energy-loss detector used for FFB finds four impact edges in the
September 28 recorded drive, at replay frames 3417, 5526, 6139, and 6169.
Its current threshold is a 32-unit drop in `$00C9`, an eight-frame cooldown,
and a boost-input exclusion. The recorded drops are 86, 40, 240, and 96 units.
These are repeatable game events; physical impact direction and wheel torque
are not encoded in the recording. The current FFB pulse uses a fixed magnitude
for all four, so severity scaling is a separate experiment.

For richer telemetry, an explicit event record containing frame, energy loss,
and a source/confidence tag is the least ambiguous route. The standard Forza
packet has acceleration and surface-rumble fields but no explicit crash field;
mapping energy loss to acceleration without measured deceleration would invent
physics. A later SimHub-facing adapter could consume event records alongside
the existing Forza dashboard stream. If acceleration is used instead, derive
it from world-position velocity and verify its direction and timing first.

## Surface/slowdown patch is not identified yet

Opt-in headless `FZERO_SURFACE_TRACE=1` logs per-frame player world position,
`$00C7`, energy and input during a verified replay. In the September 28 case,
`$00C7` is zero for 9,810 of 9,847 race frames and `0x10` for frames 3154–3190.
Mean wrapped world movement was 6.59 units/frame in 3100–3153, 6.93 while
`$00C7=0x10`, and 7.33 in 3191–3240. Energy increased by eight units at the
end of that interval. This is **not evidence of a slowdown surface**. The
existing FFB model's `ram[0xc7] != 0` “rough” boost is therefore provisional;
do not expose it as a named ground-texture telemetry channel yet.

The next useful recording deliberately crosses a known slowdown patch, with
course and approximate time noted. Compare world-position speed, energy,
`$00C7`, nearby player-state bytes and the Mode 7 course tile under the craft
before, during and after contact. Only after repeated crossings agree should
we populate Forza's `surfaceRumble[4]`/`onRumble[4]` or a dedicated surface
event. The tile map may distinguish a patch even if `$00C7` does not.

The raw trace is diagnostic and private: it includes position and controller
input, not the ROM. Do not commit a recorded trace, ROM or save state.
