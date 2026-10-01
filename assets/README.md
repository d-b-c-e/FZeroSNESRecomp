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
