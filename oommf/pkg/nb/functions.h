/* FILE: functions.h                    -*-Mode: c++-*-
 *
 * Non-class C++ functions defined by the Nb extension.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011-11-09 00:50:02 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_FUNCTIONS
#define _NB_FUNCTIONS

#include <stdarg.h>

#include "dstring.h"

/* End includes */     /* Optional directive to build.tcl */

// Dummy routine to force evaluation of temporaries.  Used as a
// hack to work around some compiler bugs.
// NOTE: The <stdarg.h> macros do not support a function signature
//       with no fixed parameters.  We don't use the parameters
//       anyway (see implementation in functions.cc), but to be
//       safe we implement this via overloading.
//       Add more as needed.
void Nb_NOP();
void Nb_NOP(OC_REAL8m);
void Nb_NOP(void*);

// Sign function
inline int Nb_Signum(double x) { return (x>0 ? 1 : (x<0 ? -1 : 0)); }

// Greatest Common Divisor
int Nb_Gcd(int m,int n);
Tcl_CmdProc NbGcdCmd;

// Greatest Common Divisor, OC_REALWIDE version
OC_REALWIDE Nb_GcdRW(OC_REALWIDE m,OC_REALWIDE n);

// Simple continued fraction like rational approximator
void Nb_RatApprox(double x,int steps,int &num,int &denom);
Tcl_CmdProc NbRatApprox;

// Version of RatApprox that takes error tolerance import
int Nb_FindRatApprox(OC_REALWIDE x,OC_REALWIDE y,
                     OC_REALWIDE relerr, OC_REALWIDE maxq,
                     OC_REALWIDE& p, OC_REALWIDE& q);

// Version of atof that allows 'D' or 'd' to be used to denote exponent.
double Nb_Atof(const char *nptr);

// Versions of strtod that allows 'D' or 'd' to be used to denote exponent
double Nb_Strtod(const char *nptr,char **endptr);
double Nb_Strtod(const unsigned char *nptr,unsigned char **endptr);

// Version of Nb_Atof with error checking.  Export value 'error'
// is set true if an error occurred, false otherwise.
double Nb_Atof(const char *nptr,OC_BOOL& error);

// Atan2 inverse, in where import "angle" is specified in degrees
void degcossin(double angle,double& cosine,double& sine);

// Routines to detect IEEE floating point NAN's and infinities.
int Nb_IsFinite(OC_REAL4 x);
int Nb_IsFinite(OC_REAL8 x);

// Routine to detect string containing nothing but whitespace.
int Nb_StrIsSpace(const char* str);

// Portable case insensitive string comparison, modeled after
// BSD's strcasecmp().
int Nb_StrCaseCmp(const char *s1, const char *s2);

// Portable thread-safe string tokenizer, modeled after BSD's strsep().
// Compare also to strtok().
char* Nb_StrSep(char **stringp, const char *delim);

// Map color strings to real [0,1] rgb values.
// Returns 1 on success, 0 on error.
OC_BOOL Nb_GetColor(const char* color,
		 OC_REAL8m& red,OC_REAL8m& green,OC_REAL8m& blue);
Tcl_CmdProc NbGetColor;

// C++ interface to the Tcl 'file nativename' functionality.
void Nb_TclNativeFilename(Nb_DString &filename);

// C++ interface to the Tcl 'file exists' functionality.
OC_BOOL Nb_FileExists(const char *path);

// C++ interface to the Tcl 'glob' command.
// Import "options" is a string of glob options (multiple options
// allowed), and globstr is the glob string.  Option -nocomplain is
// included by default.  The export results is a (possibly empty) list
// of matching files, sorted by lsort.  The return value is the number
// of elements in the list.
OC_INT4m Nb_TclGlob(const char* options,const char* globstr,
                    Nb_List<Nb_DString>& results);

// C++ interface to Oc_TempName
Nb_DString Nb_TempName(const char* baseprefix = "_",
		       const char* suffix = "",
		       const char* basedir = "");

// C++ interface to "Oc_Main GetOOMMFRootDir".  The return value is
// a convenience pointer to the buffer area of required import ab.
const char* Nb_GetOOMMFRootDir(Oc_AutoBuf& ab);

// Versions of fopen, remove, and rename that pass file names
// through Nb_TclNativeFilename().
FILE *Nb_FOpen(const char *path,const char *mode);
int Nb_Remove(const char *path);
void Nb_Rename(const char *oldpath,const char* newpath);

// Interfaces to Tcl_Write.  The first is intended mainly
// as an interface layer in case we decide in the future to
// use Tcl_WriteObj in place of Tcl_Write.  If bytecount
// is <0, then buf should be NULL terminated, and everything
// up to the NULL byte is output.  The return value is the
// number of bytes written, or -1 in case of error.
OC_INDEX Nb_WriteChannel(Tcl_Channel channel,const char* buf,OC_INDEX bytecount);
OC_INDEX Nb_FprintfChannel(Tcl_Channel channel,char* buf,OC_INDEX bufsize,
			 const char* format, ...);
/// Nb_FprintfChannel requires a workspace buffer 'buf' to store
/// the formatted string passed on to Tcl_Write.  The size of this
/// buffer in specified in bytecount.  If the formatted string is
/// bigger than bytecount, a panic occurs inside Oc_Vsnprintf (which
/// is called by this function).  For convenience, the client may
/// specify NULL for buf, in which case internal storage is used.
/// But bytecount must still be appropriately specified.

// Interface to system user id information.  Tcl provides
// a version of this in the tcl_platform array, but some
// versions of Tcl on some platforms just grab this from
// the USER environment variable, which is not reliable.
// (In particular, the user may lie!)
// NB: Unix version of Nb_GetUserName is not thread safe!
// NB: Numeric uid is only available on Unix.
#if (OC_SYSTEM_TYPE == OC_UNIX)
OC_BOOL Nb_GetUserId(OC_INT4m& numeric_id);
Tcl_CmdProc NbGetUserId;
#endif
#if (OC_SYSTEM_TYPE == OC_WINDOWS) || (OC_SYSTEM_SUBTYPE == OC_CYGWIN)
OC_BOOL Nb_GetUserName(Nb_DString& string_id);
int Nb_GetPidUserName(OC_INT4m pid,Nb_DString& username);
Tcl_CmdProc NbGetUserName;
Tcl_CmdProc NbGetPidUserName;
#endif

//////////////////////////////////////////////////////////////////////////
// Routines to interchange two values
template<class T> void Nb_Swap(T &x,T &y);  // Explicit, separate declaration
/// to help HP CC auto-instantiation.

// Include definition in header to aid GCC's auto-instantiation.
template<class T> void Nb_Swap(T &x,T &y) {
  T temp=x; x=y; y=temp;
}

#endif /* _NB_FUNCTIONS */
