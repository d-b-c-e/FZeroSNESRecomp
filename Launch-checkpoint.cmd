@echo off
setlocal
set "PATH=C:\msys64\mingw64\bin;%PATH%"
set "SNESRECOMP_AUTOCLOSE_FRAMES="
set "SNESRECOMP_INPUT_SCRIPT="
set "FZERO_VIEWPORT_SCRIPT="
set "FZERO_VIDEO_CONFIG="
set "SDL_VIDEODRIVER="
set "SDL_AUDIODRIVER="
cd /d "%~dp0build-release"
"%~dp0build-release\FZeroSNESRecomp.exe"
endlocal
