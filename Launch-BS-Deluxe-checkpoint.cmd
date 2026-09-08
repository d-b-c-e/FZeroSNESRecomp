@echo off
setlocal
set "PATH=C:\msys64\mingw64\bin;%PATH%"
set "SNESRECOMP_AUTOCLOSE_FRAMES="
set "SNESRECOMP_INPUT_SCRIPT="
set "SNESRECOMP_LLE_INTERP_TARGET="
set "SNESRECOMP_LLE_INTERP_TARGET_FILE="
set "FZERO_VIEWPORT_SCRIPT="
set "FZERO_VIDEO_CONFIG="
set "FZERO_DELUXE_DATA="
set "SDL_VIDEODRIVER="
set "SDL_AUDIODRIVER="
cd /d "%~dp0build-deluxe"
"%~dp0build-deluxe\FZeroSNESRecomp.exe"
endlocal
