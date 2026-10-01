/* Unit test only: no SDL init, GL context, game, display or device access.
 * Compile with pinned recomp-ui launcher_gl.c and its headers. Missing
 * images return before its first GL call. */
#include "launcher_gl.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  FILE *present=fopen(argv[1],"rb");
  if (present) { fclose(present); return 3; }
  LauncherTexture a=launcher_texture_load(argv[1]);
  LauncherTexture b=launcher_texture_load_colorkey(argv[1],0);
  if (a.id || a.w || a.h || b.id || b.w || b.h) return 1;
  int w=7,h=9;
  if (launcher_image_load_rgba(argv[1],&w,&h)) return 1;
  if (w!=7 || h!=9) return 1;
  launcher_texture_free(&a);
  launcher_texture_free(&b);
  puts("Pinned missing-cover loader returns empty texture without GL context");
  return 0;
}
