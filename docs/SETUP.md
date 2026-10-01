# DBCE F-Zero SNES Unified Preview setup

One product, one launcher: wheel/FFB/telemetry/replay/triples are internal
features. Check the provided preview's manifest/source receipt against the
[identity policy](PRODUCT-IDENTITY.md). Historical upstream 1.8.3 packages are
not interchangeable with the unified preview. Existing executable/settings
names stay unchanged; this document does not authorize installing over them.

Run Setup.cmd, or FZeroSNESRecomp.exe --launcher, to open the existing launcher
even when Skip launcher on boot was saved. Select your own F-Zero USA ROM.

1. On Controls, select the steering device. In Mods > Controls > Racing wheel
   controls, bind axes and SNES buttons, including save-state and rewind.
   Use Live wheel input and Steering preview to check direction and pulse
   response before driving. Start with dead zone 0%, travel 100%, response 50%.
   Rewind must also be enabled under Settings.
   Selecting a different device reloads its wheel profile in the same launcher
   session, including the live preview and axis capture target. Edited settings
   for the previous wheel are saved to that wheel's profile before switching.
   Cancelling a capture leaves selection intact; Keyboard keeps the independent
   raw-wheel identity. Selecting a wheel does not enable force feedback.
2. FFB is optional. Select the explicit device in Force feedback; preserve
   saved overrides. Start modestly and test feel while seated. A missing or
   ambiguous device disables output rather than choosing another wheel.
3. Telemetry is optional: set [Telemetry] Enabled=1, Host=127.0.0.1, Port=8000
   in config.ini and choose Forza Horizon UDP with that port in the dashboard.
   See [README](../README.md) for which signals are measured and which are compatibility
   values. There is currently no launcher telemetry settings page.
4. Triple-screen projection is experimental. Select Span for a Surround/span
   surface or Separate for three equal landscape displays in one aligned row.
   Enter visible panel width, eye distance/height, side angles and gap in the
   mod. The user's 70-degree rig is a test profile, not a universal default.
   CRT shaders apply per panel; start with Shader None when diagnosing pacing.
   Side sprites/effects and complete physical-rig acceptance remain pending;
   existing three-monitor evidence covers sampled placement/backbuffer parity.
5. BS Deluxe is build-dependent. A stock-only candidate reports Unavailable.
   Keep separate stock/Deluxe saves and bring private imported data only to a
   separately reviewed local build. A saved toggle cannot add a missing module.
6. Under Settings > Sound choose original music or your own supported MSU-1
   pack. Under Settings > Display choose None for unfiltered pixels or a bundled
   shader. CRT-Geom remains a user import; it is not included in the candidate.

If a device or display layout changes, reopen the launcher and reselect the
saved device/layout. Use a single screen and Shader None to isolate rendering
problems. Retain all config files and saves when updating; open the launcher
with --launcher instead of deleting settings. Recorded replay instructions and
their software-only force boundaries are in [PLAYTHROUGH_RECORDING.md](PLAYTHROUGH_RECORDING.md).
