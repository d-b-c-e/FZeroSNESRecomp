# Launcher artwork

`img/boxart.tga` is the North American SNES F-Zero front cover, sourced from
[Libretro's SNES thumbnail collection](https://github.com/libretro-thumbnails/Nintendo_-_Super_Nintendo_Entertainment_System/blob/master/Named_Boxarts/F-Zero%20%28USA%29.png).
Retrieved September 7, 2026; converted losslessly from PNG to uncompressed RGB
TGA for the launcher's texture loader, at the original 512 × 357 dimensions.
The original artwork belongs to Nintendo; it is not covered by this project's
source-code license.

Full source checkouts may still stage this local file for ordinary builds.
The stock-only preview packaging tools OMIT it, reject it as payload, and
omit gameplay screenshots. The pinned launcher tolerates its absence and
draws its built-in vector placeholder; no replacement Nintendo artwork is
needed. Archive exports of future candidate commits also omit this cover,
gameplay screenshots and the BS Deluxe patch using `.gitattributes`.
These export rules do not remove tracked assets, old commits, clones or old
GitHub source archives. See [distribution audit](../docs/DISTRIBUTION-AUDIT.md)
for asset origins, unresolved terms and preview identity.

## CRT mask choices

`shaders/crt-soft.glslp` retains the original CRT appearance. The optional
`shaders/crt-soft-subtle.glslp` keeps its scanlines, warm tint and mask spacing,
but reduces the vertical mask contrast by 75% while preserving its continuous
average brightness. Select the subtle preset through the launcher's existing
shader picker; selecting the original preset restores the previous look.
No existing setting or default changes, including normal-aspect rendering.

The mask follows each view's source pixels rather than the whole desktop.
Stretching a lower-resolution side view onto a full monitor makes its mask
stripes wider than the center view. Sharp mask edges and raster resampling can
also produce broader apparent bands; an ultra-wide desktop alone does not
establish the cause. The subtle option lowers their intensity without changing
viewport geometry, horizontal scanlines or projection.
