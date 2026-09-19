#pragma once

#include <stddef.h>

/*
 * The two system hotkeys this host shares with the launcher.
 *
 * recomp-ui's Controls page edits config.ini's [KeyMap] section — the same
 * spelling the framework desktop host's config.c parses ("F7", "Ctrl+R",
 * "Shift+P", "Alt+Return", "Keypad +", "(unbound)"). F-Zero does not link
 * that parser: it carries its own Config and would collide with it. So the
 * small part of the format this host actually needs lives here, split so the
 * part that can be tested without SDL is tested without SDL — the modifier
 * prefixes, the unbound spellings and the ini scan are all in this file's
 * pure half, and only turning a key NAME into an SDL keycode needs SDL.
 */

enum {
  FZERO_HOTKEY_MOD_SHIFT = 1u << 0,
  FZERO_HOTKEY_MOD_CTRL = 1u << 1,
  FZERO_HOTKEY_MOD_ALT = 1u << 2
};

typedef struct FzeroHotkeySpec {
  int bound;      /* 0 for an empty value, "None" or "(unbound)" */
  unsigned mods;  /* FZERO_HOTKEY_MOD_* */
  char key[48];   /* SDL key name, as SDL_GetKeyFromName takes it */
} FzeroHotkeySpec;

/* Parse one [KeyMap] value. Always writes *out; an unparsable or unbound
 * value leaves out->bound zero. Modifier prefixes may appear in any order and
 * any case, matching the framework parser. */
void FzeroHotkeyParse(const char *value, FzeroHotkeySpec *out);

/*
 * Read [KeyMap] <name> out of an ini file. Returns 1 when the key was present
 * (even when its value is unbound), 0 when the file or the key is missing —
 * so a caller can tell "the player unbound it" from "no config.ini yet" and
 * apply its built-in default only in the second case.
 */
int FzeroHotkeyFromIni(const char *path, const char *name,
                       FzeroHotkeySpec *out);

/*
 * Read one value out of an ini file. The launcher writes back through
 * recomp-ui's launcher_ini_kv_write(), which edits a single line and leaves
 * the rest of the file alone; this is the matching read.
 *
 * Returns 1 when [section] Key was present. `out` is only written then, so a
 * caller can pre-load its default and ignore the return.
 */
int FzeroIniReadString(const char *path, const char *section, const char *key,
                       char *out, size_t cap);
int FzeroIniReadInt(const char *path, const char *section, const char *key,
                    int *out);
