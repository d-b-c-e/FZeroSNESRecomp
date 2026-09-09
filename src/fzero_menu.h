#pragma once
#include "fzero_video.h"
#ifdef __cplusplus
extern "C" {
#endif
bool FzeroMenuInit(void *window, void *renderer, FzeroVideoSettings *settings,
                   bool *enabled, bool *compare, int *running, const char *path,
                   bool vulkan);
bool FzeroMenuEvent(const void *event);
bool FzeroMenuOpen(void);
void FzeroMenuDraw(void);
void FzeroMenuShutdown(void);
#ifdef __cplusplus
}
#endif
