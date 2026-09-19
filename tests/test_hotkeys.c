#include "fzero_hotkeys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr, "%d: %s\n", __LINE__, #e); exit(1); } } while (0)

static void write_file(const char *path, const char *text) {
  FILE *f = fopen(path, "wb");
  CHECK(f != NULL);
  CHECK(fwrite(text, 1, strlen(text), f) == strlen(text));
  CHECK(fclose(f) == 0);
}

int main(void) {
  FzeroHotkeySpec s;

  /* A plain key, the shape the launcher writes for the two defaults. */
  FzeroHotkeyParse("F7", &s);
  CHECK(s.bound && s.mods == 0 && !strcmp(s.key, "F7"));

  /* Modifier prefixes, in any order and any case, and stacked. */
  FzeroHotkeyParse("Ctrl+R", &s);
  CHECK(s.bound && s.mods == FZERO_HOTKEY_MOD_CTRL && !strcmp(s.key, "R"));
  FzeroHotkeyParse("shift+ALT+ctrl+F12", &s);
  CHECK(s.bound && !strcmp(s.key, "F12") &&
        s.mods == (FZERO_HOTKEY_MOD_SHIFT | FZERO_HOTKEY_MOD_ALT |
                   FZERO_HOTKEY_MOD_CTRL));
  FzeroHotkeyParse("Alt+Return", &s);
  CHECK(s.bound && s.mods == FZERO_HOTKEY_MOD_ALT && !strcmp(s.key, "Return"));

  /* A key name with a space inside it survives; surrounding space does not. */
  FzeroHotkeyParse("  Keypad +  ", &s);
  CHECK(s.bound && s.mods == 0 && !strcmp(s.key, "Keypad +"));

  /* Every spelling of "no key". */
  FzeroHotkeyParse("", &s);
  CHECK(!s.bound && s.key[0] == 0);
  FzeroHotkeyParse("None", &s);
  CHECK(!s.bound && s.key[0] == 0);
  FzeroHotkeyParse("(unbound)", &s);
  CHECK(!s.bound && s.key[0] == 0);
  FzeroHotkeyParse(NULL, &s);
  CHECK(!s.bound);
  /* A modifier with nothing after it is not a binding. */
  FzeroHotkeyParse("Ctrl+", &s);
  CHECK(!s.bound && s.mods == 0);

  /* ---- ini scanning ---------------------------------------------------- */
  const char *path = "test-hotkeys.ini";

  /* Missing file: not present, so the caller keeps its built-in default. */
  remove(path);
  CHECK(FzeroHotkeyFromIni(path, "SaveStateMenu", &s) == 0);

  write_file(path,
             "; a comment\n"
             "[General]\n"
             "SaveStateMenu = F1\n"   /* wrong section: must be ignored */
             "\n"
             "[KeyMap]\n"
             "Fullscreen=Alt+Return\n"
             "SaveStateMenu   =   Shift+F9   ; trailing comment\n"
             "Rewind=(unbound)\n");

  CHECK(FzeroHotkeyFromIni(path, "SaveStateMenu", &s) == 1);
  CHECK(s.bound && s.mods == FZERO_HOTKEY_MOD_SHIFT && !strcmp(s.key, "F9"));
  /* Case-insensitive key match, the way the framework parser compares. */
  CHECK(FzeroHotkeyFromIni(path, "savestatemenu", &s) == 1 && s.bound);
  /* Present but unbound is NOT the same as absent: the player took the key
   * away on purpose and a default must not come back. */
  CHECK(FzeroHotkeyFromIni(path, "Rewind", &s) == 1 && !s.bound);
  CHECK(FzeroHotkeyFromIni(path, "Turbo", &s) == 0);

  /* A file with no [KeyMap] at all reads as absent, not as unbound. */
  write_file(path, "[Graphics]\nVSync=1\n");
  CHECK(FzeroHotkeyFromIni(path, "SaveStateMenu", &s) == 0);

  remove(path);
  puts("config.ini [KeyMap] parsing passed");
  return 0;
}
