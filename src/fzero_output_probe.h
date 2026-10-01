/* Explicit replay-only readbacks. These synchronize the GPU and deliberately
 * force selected verified frames to present; never use their pacing as a
 * benchmark. They sample default backbuffers before swap, not desktop scanout. */
#ifdef _WIN32
#include <windows.h>
#endif
static const char *g_output_probe_dir;
static const long kOutputProbeFrames[] = {1000, 3000, 6000};
static unsigned g_output_probe_index;
static long g_output_probe_frame;
static bool g_output_probe_failed;

static bool output_probe_due(long frame) {
  return g_output_probe_dir && g_output_probe_index < 3 &&
      frame == kOutputProbeFrames[g_output_probe_index];
}

static void output_probe_hex(const uint8_t *data, size_t size, char out[65]) {
  uint8_t hash[32]; sha256_compute(data, size, hash);
  for (unsigned i = 0; i < 32; ++i) sprintf(out + i * 2, "%02x", hash[i]);
}

static void output_probe(SDL_Window *window, const char *role,
                         const uint8_t *cpu, int cpu_width, int cpu_height,
                         int width, int height, bool ready, bool shader_loaded) {
  if (!g_output_probe_frame) return;
  if (!cpu || !ready || width <= 0 || height <= 0 ||
      width > 8192 || height > 4096) {
    g_output_probe_failed = true;
    fprintf(stderr, "[fzero-output-probe] invalid %s output at frame %ld\n",
            role, g_output_probe_frame);
    return;
  }
  size_t size = (size_t)width * height * 4;
  uint8_t *rgba = malloc(size);
  if (!rgba) { g_output_probe_failed = true; return; }
  GLint framebuffer, read_buffer, default_read, alignment, row_length;
  GLint skip_rows, skip_pixels, pack_buffer;
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &framebuffer);
  glGetIntegerv(GL_READ_BUFFER, &read_buffer);
  glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
  glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
  glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
  glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glGetIntegerv(GL_READ_BUFFER, &default_read);
  glReadBuffer(GL_BACK);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_SKIP_ROWS, 0);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  GLenum error = glGetError();
  glPixelStorei(GL_PACK_ALIGNMENT, alignment);
  glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
  glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
  glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, pack_buffer);
  glReadBuffer(default_read);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
  glReadBuffer(read_buffer);
  if (error != GL_NO_ERROR) {
    fprintf(stderr, "[fzero-output-probe] GL error %u for %s\n", error, role);
    g_output_probe_failed = true; free(rgba); return;
  }
  char cpu_hash[65], gl_hash[65], path[2048];
  output_probe_hex(cpu, (size_t)cpu_width * cpu_height * 4, cpu_hash);
  output_probe_hex(rgba, size, gl_hash);
  size_t coloured = 0;
  for (size_t i = 0; i < size; i += 4)
    if (rgba[i] || rgba[i + 1] || rgba[i + 2]) ++coloured;
  snprintf(path, sizeof(path), "%s/%ld-%s.rgba", g_output_probe_dir,
           g_output_probe_frame, role);
  FILE *raw = fopen(path, "wb");
  bool written = raw && fwrite(rgba, size, 1, raw) == 1;
  if (raw && fclose(raw)) written = false;
  free(rgba);
  int x = 0, y = 0, w = 0, h = 0;
  SDL_GetWindowPosition(window, &x, &y); SDL_GetWindowSize(window, &w, &h);
  SDL_Rect display_bounds = {0};
#if SNESRECOMP_SDL3
  SDL_DisplayID display = SDL_GetDisplayForWindow(window);
  bool display_ok = display && SDL_GetDisplayBounds(display, &display_bounds);
  bool display_primary = display == SDL_GetPrimaryDisplay();
#else
  int display = SDL_GetWindowDisplayIndex(window);
  bool display_ok = display >= 0 && SDL_GetDisplayBounds(display, &display_bounds) == 0;
  bool display_primary = display == 0;
#endif
  snprintf(path, sizeof(path), "%s/probes.jsonl", g_output_probe_dir);
  FILE *log = fopen(path, "a");
  char gdi_device[128] = {0};
  bool native_foreground = false;
#if defined(_WIN32) && SNESRECOMP_SDL3
  HMONITOR monitor = SDL_GetPointerProperty(SDL_GetDisplayProperties(display),
      SDL_PROP_DISPLAY_WINDOWS_HMONITOR_POINTER, NULL);
  MONITORINFOEXA info = {0}; info.cbSize = sizeof(info);
  if (monitor && GetMonitorInfoA(monitor, (MONITORINFO *)&info)) {
    size_t out = 0;
    for (const char *in = info.szDevice; *in && out + 2 < sizeof(gdi_device); ++in) {
      if (*in == '\\' || *in == '"') gdi_device[out++] = '\\';
      gdi_device[out++] = *in;
    }
  }
  HWND handle = SDL_GetPointerProperty(SDL_GetWindowProperties(window),
      SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
  native_foreground = handle && GetForegroundWindow() == handle;
#endif
  if (log) {
    fprintf(log, "{\"frame\":%ld,\"role\":\"%s\",\"cpu_sha256\":\"%s\","
        "\"cpu_width\":%d,\"cpu_height\":%d,\"gl_sha256\":\"%s\","
        "\"gl_width\":%d,\"gl_height\":%d,\"coloured_pixels\":%zu,"
        "\"shader_loaded\":%s,\"written\":%s,\"display_ok\":%s,"
        "\"window_id\":%u,\"display_id\":%u,\"display_primary\":%s,\"window_bounds\":[%d,%d,%d,%d],"
        "\"display_bounds\":[%d,%d,%d,%d],\"keyboard_focus\":%s,"
        "\"input_focus\":%s,\"mouse_focus\":%s,\"minimized\":%s,"
        "\"gdi_device\":\"%s\",\"native_foreground\":%s}\n",
        g_output_probe_frame, role, cpu_hash, cpu_width, cpu_height, gl_hash,
        width, height, coloured, shader_loaded ? "true" : "false",
        written ? "true" : "false", display_ok ? "true" : "false",
        SDL_GetWindowID(window), (unsigned)display,
        display_primary ? "true" : "false", x, y, w, h,
        display_bounds.x, display_bounds.y, display_bounds.w, display_bounds.h,
        SDL_GetKeyboardFocus() == window ? "true" : "false",
        SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS ? "true" : "false",
        SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_FOCUS ? "true" : "false",
        SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED ? "true" : "false",
        gdi_device, native_foreground ? "true" : "false");
    if (fclose(log)) written = false;
  } else written = false;
  if (!written || !display_ok || !coloured) g_output_probe_failed = true;
}
