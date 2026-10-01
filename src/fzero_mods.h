#pragma once
#include "fzero_video.h"
#include "recomp_launcher.h"
const RecompLauncherCModProvider *FzeroModsProvider(FzeroVideoSettings *settings,
                                                   const char *path);
const RecompLauncherCModProvider *FzeroModsProviderWheel(
    FzeroVideoSettings *settings, const char *video_path,
    const char *config_path, const char *wheel_guid,
    void (*write_ini)(const char *, const char *, const char *, const char *),
    int (*list_ffb_devices)(char names[][256], int max_devices),
    int (*read_wheel_axis)(const char *guid, int axis, int *value));
