/* FILE: errhandlers.h                    -*-Mode: c++-*-
 *
 * Some error handling routines that should be phased out
 * and replaced with code in the oc extension.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011/11/09 00:50:02 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_ERRHANDLERS
#define _NB_ERRHANDLERS

#include "oc.h"

#include "evoc.h"

/* End includes */     /* Optional directive to build.tcl */

// Routine to print message to standard (user) output channel
int Message(const OC_CHAR *message_fmt,...);

// C-interface to Tcl/Tk error boxes.  The above routines map
// into this.
enum Nb_MessageType { NB_MSG_DEBUG, NB_MSG_INFO, NB_MSG_WARNING,
                      NB_MSG_ERROR };
OC_INT4m TkMessage(Nb_MessageType mt,const OC_CHAR *msg,OC_INT4m exitcode=1,
                const OC_CHAR *errCode=NULL);

//////////////////////////////////////////////////////////////////////////
// Error codes
extern const char* ErrArrOutOfBounds;
extern const char* ErrBadFile;
extern const char* ErrBadFileHeader;
extern const char* ErrBadFileOpen;
extern const char* ErrInputEOF;
extern const char* ErrOutputEOF;
extern const char* ErrBadParam;
extern const char* ErrDispInit;
extern const char* ErrMisc;
extern const char* ErrNoMem;
extern const char* ErrOverflow;
extern const char* ErrProgramming;

//////////////////////////////////////////////////////////////////////////
// Error handlers
// Note: Use preprocessor macros __FILE__ and __LINE__ to get
//       'filename' and 'lineno' respectively.
void FatalError(OC_INT4m errorcode,
		const OC_CHAR *membername,
		const ClassDoc &class_doc,
		const OC_CHAR *filename,OC_INT4m lineno,
		const OC_CHAR *fmt,...);
/// Doesn't return; exits with code 'errorcode'

void NonFatalError(const OC_CHAR *membername,
		   const ClassDoc &class_doc,
		   const OC_CHAR *filename,OC_INT4m lineno,
		   const OC_CHAR *fmt,...);
/// Prints warning message, blocks if Tk is enabled, and returns.

void ClassMessage(const OC_CHAR *membername,
		  const ClassDoc &class_doc,
		  const OC_CHAR *filename,OC_INT4m lineno,
		  const OC_CHAR *fmt,...);
/// Prints warning message and returns. Non-blocking

#ifndef NDEBUG
void ClassDebugMessage(const OC_CHAR *membername,
		       const ClassDoc &class_doc,
		       const OC_CHAR *filename,OC_INT4m lineno,
		       const OC_CHAR *fmt,...);
#else
inline void ClassDebugMessage(const OC_CHAR *,const ClassDoc &,
			      const OC_CHAR *,OC_INT4m,const OC_CHAR *,...) {}
#endif
/// Prints warning message and returns. Non-blocking

// Versions of the above for non-class functions:
void PlainWarning(const OC_CHAR *fmt,...);
void PlainError(OC_INT4m errorcode,const OC_CHAR *fmt,...);

// If you #define MEMBERNAME at the start of each member definition,
// then you can use the following macro to expand to 4 of the fields
// needed by the above error handlers.
#define STDDOC MEMBERNAME,class_doc,__FILE__,__LINE__

#endif // _NB_ERRHANDLERS
