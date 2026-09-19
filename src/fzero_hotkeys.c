#include "fzero_hotkeys.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ieq(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    a++;
    b++;
  }
  return *a == 0 && *b == 0;
}

static int istarts(const char *s, const char *prefix) {
  while (*prefix) {
    if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
    s++;
    prefix++;
  }
  return 1;
}

static const char *skip_spaces(const char *s) {
  while (*s == ' ' || *s == '\t') s++;
  return s;
}

void FzeroHotkeyParse(const char *value, FzeroHotkeySpec *out) {
  if (!out) return;
  memset(out, 0, sizeof(*out));
  if (!value) return;

  const char *s = skip_spaces(value);
  for (;;) {
    /* Any order, any case: the launcher writes Ctrl before Alt before Shift,
     * but a config.ini edited by hand is still a config.ini. */
    if (istarts(s, "shift+")) { out->mods |= FZERO_HOTKEY_MOD_SHIFT; s += 6; }
    else if (istarts(s, "ctrl+")) { out->mods |= FZERO_HOTKEY_MOD_CTRL; s += 5; }
    else if (istarts(s, "alt+")) { out->mods |= FZERO_HOTKEY_MOD_ALT; s += 4; }
    else break;
    s = skip_spaces(s);
  }

  /* Trailing whitespace is trimmed but inner spaces are not: "Keypad +" is
   * one key name. */
  size_t len = strlen(s);
  while (len && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' ||
                 s[len - 1] == '\n'))
    len--;

  if (len == 0 || len >= sizeof(out->key)) {
    out->mods = 0;
    return;
  }
  memcpy(out->key, s, len);
  out->key[len] = '\0';
  if (ieq(out->key, "none") || ieq(out->key, "(unbound)") ||
      ieq(out->key, "unbound")) {
    out->key[0] = '\0';
    out->mods = 0;
    return;
  }
  out->bound = 1;
}

int FzeroIniReadString(const char *path, const char *section, const char *key,
                       char *out, size_t cap) {
  if (!path || !path[0] || !section || !key || !key[0] || !out || !cap)
    return 0;

  FILE *f = fopen(path, "r");
  if (!f) return 0;

  char line[512];
  int in_section = 0;
  int found = 0;
  while (!found && fgets(line, sizeof(line), f)) {
    const char *p = skip_spaces(line);
    if (*p == ';' || *p == '#' || *p == '\r' || *p == '\n' || *p == '\0')
      continue;
    if (*p == '[') {
      const char *close = strchr(p, ']');
      if (!close) continue;
      size_t n = (size_t)(close - p - 1);
      char name[64];
      if (n >= sizeof(name)) n = sizeof(name) - 1;
      memcpy(name, p + 1, n);
      name[n] = '\0';
      in_section = ieq(name, section);
      continue;
    }
    if (!in_section) continue;

    const char *eq = strchr(p, '=');
    if (!eq) continue;
    size_t klen = (size_t)(eq - p);
    while (klen && (p[klen - 1] == ' ' || p[klen - 1] == '\t')) klen--;
    char found_key[64];
    if (klen >= sizeof(found_key)) continue;
    memcpy(found_key, p, klen);
    found_key[klen] = '\0';
    if (!ieq(found_key, key)) continue;

    /* A trailing comment is not part of the value: the framework writer never
     * emits one, but a hand-edited file may. */
    snprintf(out, cap, "%s", eq + 1);
    char *cut = strpbrk(out, ";#\r\n");
    if (cut) *cut = '\0';
    found = 1;
  }
  fclose(f);
  return found;
}

int FzeroIniReadInt(const char *path, const char *section, const char *key,
                    int *out) {
  char value[64];
  if (!FzeroIniReadString(path, section, key, value, sizeof(value))) return 0;
  const char *s = skip_spaces(value);
  char *end = NULL;
  long parsed = strtol(s, &end, 10);
  if (end == s) return 0;
  if (out) *out = (int)parsed;
  return 1;
}

int FzeroHotkeyFromIni(const char *path, const char *name,
                       FzeroHotkeySpec *out) {
  char value[128];
  if (out) memset(out, 0, sizeof(*out));
  if (!FzeroIniReadString(path, "KeyMap", name, value, sizeof(value))) return 0;
  FzeroHotkeyParse(value, out);
  return 1;
}
