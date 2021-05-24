/* FILE: xp.h                    -*-Mode: c++-*-
 *
 *	The OOMMF eXtra Precision extension public header file.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 */

#ifndef _XP
#define _XP

// Constructed files (such as xpport.h) can raise false circular
// dependency errors in pimake CSourceFile Dependencies calls that
// originate outside this package.  A two part workaround is:
//
//    1) The public header xp.h explicitly #include's each constructed
//       file.  These should precede any other #include's.  If there are
//       multiple constructed files, say a.h and b.h, and if b.h
//       #include's a.h, then #include "a.h" should precede #include
//       "b.h" in this file.
//
//    2) Files inside the xp package don't #include the public header
//       xp.h, but instead #include a private internal header, xpint.h,
//       as needed.  The public header xp.h also includes xpint.h.
//
// When an external dependency check looks inside xp.h, part 1 will
// insure that if xpport.h doesn't exist then the subsequent sourcing of
// xp/makerules.tcl will be triggered while processing xp.h.  Any
// CSourceFile Dependencies occuring inside the xp/makerules.tcl file
// will then be grafted onto the dependency graph at xp.h.  (The
// grafting protects against infinite recursion arising from potential
// circular dependencies.)  Since by part 2 none of the files in the xp
// package have a dependency on xp.h, no false circular dependencies are
// encountered.
#include "xpport.h"  // Part 1
#include "xpint.h"   // Part 2

#endif /* _XP */
