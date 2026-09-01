/*
 * NeurOne SW-02 Platform Trap
 * Document: NP-SW-CI-001 §4.8 (phase 8, closes OI-SWCI-21)
 * SW item:  SW-02 (i.MX RT1062 main processor) — IEC 62304 Class B
 *
 * The single failure primitive for a SW-02 platform symbol that has no driver.
 *
 * ── Why a trap and not a plausible return value ─────────────────────────────
 *
 * NP-SW-CI-001 §4.4 argues that writing drivers to turn a build leg green is
 * the wrong trade.  It does not follow that a stub which returns 0, or "not
 * present", or "impedance OK" is the right one — that is the same trade with
 * the failure hidden instead of avoided.  A stub that answers plausibly is
 * indistinguishable at the call site from a driver that is wrong, and several
 * of these seams gate stimulation: np_mod_visual_hal_hall_lifted() returning
 * "not lifted", np_mod_vns_hal_impedance_check() returning a passing value, or
 * np_proto_hal_get_proto_pubkey() returning a zero key each converts a missing
 * driver into a defeated interlock.
 *
 * So every stub traps, and the trap is the designed safe state rather than an
 * improvised one.  np_platform_unimplemented() masks interrupts and spins,
 * which stops task_safety_heartbeat's 200 ms SPI heartbeat to the safety MCU.
 * The safety MCU's 1.5 s watchdog then cuts every stimulation enable line in
 * under 50 ms (CLAUDE.md §4.2, NP-FW-HUB-001 §2).  The stub layer's failure
 * mode is therefore the same state the system already enters when the main
 * processor dies for any other reason — a path that is independently
 * implemented, independently classified (Class C), and independently tested.
 *
 * ── What this is not ────────────────────────────────────────────────────────
 *
 * It is not a fallback, a mock, or a bring-up convenience to be left in.  An
 * image linked against firmware/platform/ halts on the first platform call it
 * makes, which for np_application is np_hal_get_device_session_count() a few
 * instructions into np_hub_control_app_main().  That is deliberate: the image
 * exists so that SW-02 can be *linked* and its platform gap *counted*
 * (NP-SW-CI-001 §4.8), not so that it can be run.
 */

#ifndef NP_PLATFORM_TRAP_H
#define NP_PLATFORM_TRAP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Does not return.  `symbol` is the name of the platform function that was
 * called; it is stored in a file-scope volatile before the spin so a debugger
 * attached after the fact can read which seam was reached without a live
 * backtrace.
 */
void np_platform_unimplemented(const char *symbol) __attribute__((noreturn));

/* Used by every definition in np_platform_stub.c.  __func__ rather than a
 * literal so the name cannot drift from the function it labels. */
#define NP_PLATFORM_TRAP()   np_platform_unimplemented(__func__)

#ifdef __cplusplus
}
#endif

#endif /* NP_PLATFORM_TRAP_H */
