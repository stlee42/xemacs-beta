/* Synched up with: Not in FSF. */

#include "decosf3-2.h"

#ifndef NOT_C_CODE
#include "/usr/include/sys/lc_core.h"
#include "/usr/include/reg_types.h"
#endif

#define re_compile_pattern sys_re_compile_pattern
#define re_search sys_re_search
#define re_search_2 sys_re_search_2
#define re_match_2 sys_re_match_2
#define re_max_failures sys_re_max_failures
#define re_set_syntax sys_re_set_syntax
#define re_set_registers sys_re_set_registers
#define re_compile_fastmap sys_re_compile_fastmap
#define re_match sys_re_match
#define regcomp sys_regcomp
#define regexec sys_regexec
#define regerror sys_regerror
#define regfree sys_regfree
#define regex_t sys_regex_t
#define regoff_t sys_regoff_t
#define regmatch_t sys_regmatch_t

#define SYSTEM_MALLOC

/* Some V4.0* versions before V4.0B don't detect rename properly. */
#ifndef HAVE_RENAME
#define HAVE_RENAME
#endif

/* Digital Unix 4.0 has a realpath, but it's buggy.  And I
   *do* mean buggy. */
#undef HAVE_REALPATH

#define LIBS_DEBUG
