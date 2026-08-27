cmake_minimum_required(VERSION 3.20)
project(FZeroSNESRecomp C)
set(CMAKE_C_STANDARD 11)

# Both dependencies are pinned submodules of this repository (see
# .gitmodules); `bash tools/bootstrap.sh` initializes them. Point
# SNESRECOMP_ROOT at a worktree instead when developing framework changes
# alongside the game.
set(SNESRECOMP_ROOT "${CMAKE_SOURCE_DIR}/snesrecomp" CACHE PATH
    "Path to the snesrecomp checkout used by this game")
set(RECOMP_UI_ROOT "${CMAKE_SOURCE_DIR}/recomp-ui" CACHE PATH
    "Path to the shared recomp-ui launcher checkout")
set(SNESRECOMP_SDL_BACKEND "SDL3" CACHE STRING
    "Desktop SDL backend (SDL3 or SDL2)")
include(${SNESRECOMP_ROOT}/runner/runner.cmake)
include(${RECOMP_UI_ROOT}/recomp_ui.cmake)

# Release version stamp: configure with -DSNESRECOMP_BUILD_VERSION=<ver>.
# Unset (local builds) the define is omitted and the host defaults to "dev".
# NOTE: quote it when configuring - PowerShell rewrites an unquoted
# -DSNESRECOMP_BUILD_VERSION=0.1.0 into "0".
set(SNESRECOMP_BUILD_VERSION "" CACHE STRING
    "Release version stamp baked into crash reports (empty = dev)")

set(FZERO_GEN_DIR "${CMAKE_SOURCE_DIR}/src/gen" CACHE PATH
    "Directory containing the private generated F-Zero C sources")
file(GLOB FZERO_GEN_SOURCES CONFIGURE_DEPENDS "${FZERO_GEN_DIR}/*.c")
if(NOT FZERO_GEN_SOURCES)
    message(FATAL_ERROR
        "${FZERO_GEN_DIR} is empty - stage a verified fzero.sfc and run "
        "`bash tools/regen.sh` before building.")
endif()

list(REMOVE_ITEM SNESRECOMP_RUNNER_SOURCES
    "${SNESRECOMP_ROOT}/runner/src/keybinds.c"
    "${SNESRECOMP_ROOT}/runner/src/host_report.c")

# The headless host keeps its own host_report stubs: it links no SDL, and
# host_report.c includes desktop/sdl_compat.h.
add_executable(FZeroSNESRecompHeadless
    ${SNESRECOMP_RUNNER_SOURCES}
    ${FZERO_GEN_SOURCES}
    src/headless_host.c
    src/headless_main.c
    src/fzero_runtime.c)

# The desktop host takes the REAL host_report.c (always-on breadcrumb ring,
# crash minidump writer, build-version stamp) and the REAL keybinds.c, so the
# launcher's Controls page and the game read the same keybinds.ini.
add_executable(FZeroSNESRecomp
    ${SNESRECOMP_RUNNER_SOURCES}
    "${SNESRECOMP_ROOT}/runner/src/host_report.c"
    "${SNESRECOMP_ROOT}/runner/src/keybinds.c"
    ${FZERO_GEN_SOURCES}
    src/sdl_main.c
    src/fzero_runtime.c)

function(configure_fzero_target target)
    target_include_directories(${target} PRIVATE
        "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/recomp"
        ${SNESRECOMP_RUNNER_INCLUDE_DIRS})
    target_compile_definitions(${target} PRIVATE
        SYSTEM_VOLUME_MIXER_AVAILABLE=0
        SNESRECOMP_TRACE=$<BOOL:${SNESRECOMP_ENABLE_TRACE}>
        SNESRECOMP_REVERSE_DEBUG=0)
    if(NOT MSVC)
        # The recompiled C is machine-generated; quiet its warnings and
        # tolerate the K&R-style implicit declarations clang rejects.
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:C>:-w;-Wno-implicit-function-declaration>)
        target_link_libraries(${target} PRIVATE m)
    endif()
    target_link_libraries(${target} PRIVATE ${SNESRECOMP_RUNNER_LIBRARIES})
endfunction()

configure_fzero_target(FZeroSNESRecompHeadless)
configure_fzero_target(FZeroSNESRecomp)
# Hosts include "desktop/sdl_compat.h" rather than <SDL.h> so both majors build.
snesrecomp_target_sdl(FZeroSNESRecomp)
recomp_target_launcher_ui(FZeroSNESRecomp CONSOLE snes)

if(SNESRECOMP_BUILD_VERSION)
    target_compile_definitions(FZeroSNESRecomp PRIVATE
        SNESRECOMP_BUILD_VERSION="${SNESRECOMP_BUILD_VERSION}")
endif()
