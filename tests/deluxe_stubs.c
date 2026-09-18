/* The Deluxe loader only reaches the runtime to install a program and to pick
 * a save root. Neither belongs to what test_deluxe.c checks - which payload is
 * accepted, and that a refusal is a return value rather than an exit - so they
 * stand in here and the test links without the recompiled program. */
#include "cpu_state.h"
#include "common_rtl.h"
#include "snes/interp_bridge.h"

const DispatchEntry deluxe_g_dispatch_table[1];
const unsigned deluxe_g_dispatch_table_count = 0;
const RamRoutineGuard deluxe_g_ram_routine_guards[1];
const unsigned deluxe_g_ram_routine_guard_count = 0;

void cpu_select_program(const DispatchEntry *dispatch, unsigned count,
                        const RamRoutineGuard *guards, unsigned guard_count) {
  (void)dispatch; (void)count; (void)guards; (void)guard_count;
}
void interp_bridge_set_scheduler_aot_policy(int enabled) { (void)enabled; }
const char *RtlSaveRoot(void) { return "."; }
void RtlEnsureSaveDir(void) {}
void RtlSetSaveRoot(const char *root) { (void)root; }
