/*
 * np_ld_script_limits.h — host-test limit shared by the linker-script readers
 *
 * HOST TEST CODE ONLY.  The bootloader image test and the application link
 * agreement test each slurp a GNU ld script into a buffer of this size.
 */

#ifndef NP_LD_SCRIPT_LIMITS_H
#define NP_LD_SCRIPT_LIMITS_H

#define LD_MAX_BYTES  (256U * 1024U)

#endif /* NP_LD_SCRIPT_LIMITS_H */
