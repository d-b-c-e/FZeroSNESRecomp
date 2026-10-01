# F-Zero consumer shutdown and unload contract

This is a source-integration gate, not approval to replace the toolkit DLL,
deploy a game build or enable physical force feedback. The starting consumer
is published `69968bb6c767bb1f7e0eefe63503e2837f7ea7a8`.

## Producer ownership

`src/sdl_main.c` calls `FzeroFfbInit` once on the main thread after window,
fullscreen and input setup. The launcher enumerates FFB names synchronously
before that initialization. The frame loop calls `FzeroFfbFrame`; pause and
overlay transitions call `FzeroFfbSilence`. No consumer FFB thread is created
and no host callback is registered with the toolkit. `InstallExitGuards()`
registers toolkit-owned handlers, not a consumer function pointer.

On exit, the frame loop has finished before `debug_server_shutdown`, audio
pause/closure and gamepad shutdown. `FzeroFfbShutdown` then runs before SDL
window/context destruction. There is no later frame/silence/discovery call in
that cleanup path. Neither consumer source file defines `DllMain`; no consumer
join or unload occurs under the Windows loader lock.

## Quiescence boundary

Every export-backed consumer entry (init, frame, silence, discovery and
shutdown) owns the same recursive call gate. Shutdown waits for an in-flight
consumer call to finish, closes discovery admission and marks force output
inactive before cleanup exports. Late frame, silence and discovery calls
cannot emit exports or load a module after that boundary. Repeated shutdown
is harmless. An explicit owner-thread `FzeroFfbInit` may deliberately start a
new session; it is not an exit callback and must not be scheduled by a producer
being stopped. The current host has no such post-shutdown init call.

The recursive gate permits init's existing cleanup call and same-thread
inactive reentry. Toolkit workers have no callback into the consumer; a future
consumer callback/thread integration must stop its producers before shutdown
and must not make a toolkit join depend on a callback waiting for this gate.

Cleanup retains this order: `PanicStop`, optional constant-burst release,
periodic/condition releases, `FreeDirectInput`, then `WheelFfb_Unload`/FreeLibrary.
Function pointers and model state are cleared afterward. Discovery's partial
loader-resolution failure now releases its module reference; it has not called
initialization, installed exit guards or started a watchdog.

## Required toolkit completion contract

The independently reviewed corrected toolkit must make `FreeDirectInput`
synchronous: stop admission/producers, remove/drain callbacks, join workers
without a timeout escape, and finish resource/logging cleanup before returning.
Only that return permits final module unload. The consumer's void ABI cannot
detect a backend that returns prematurely. Version numbers/export counts do
not prove that a corrected implementation is loaded.

The retained v0.13/native 0.6 DLL is unchanged; its historical 500 ms unchecked
wait does not establish completed shutdown. Binary adoption therefore remains
held until the shared owner supplies the exact corrected source/build hash and
the integration review verifies that contract. No toolkit binary, manifest,
header or dependency pin is replaced by this consumer candidate.

## Device-free regression

`tests/test_ffb_lifecycle.cpp` compiles the actual consumer implementation with
fake Win32 loader functions and fake export pointers. It never loads a DLL,
initializes SDL/DirectInput, installs a real exit handler or emits torque.
Rendezvous points hold a fake frame export and a fake synchronous shutdown:

- shutdown cannot enter cleanup while the frame call is in flight;
- unload cannot occur while fake worker completion is withheld;
- late frame/silence/discovery calls add no exports or module loads;
- completed shutdown return precedes final FreeLibrary;
- repeated shutdown and partial-resolution discovery cleanup are covered.

These assertions test the consumer boundary against a conforming fake backend.
They do not prove real driver behavior or the old binary's worker completion.
