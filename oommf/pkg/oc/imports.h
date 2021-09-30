/* FILE: imports.h                    -*-Mode: c++-*-
 *
 *	Directives to include header files from outside the extension.
 *
 * NOTICE: Plase see the file ../../LICENSE
 *
 * Last modified on: $Date: 2014/10/27 21:44:21 $
 * Last modified by: $Author: donahue $
 *
 * NOTE: Code which requires tcl.h or tk.h should *not* #include
 * tcl.h or tk.h directly, but should rather #include either this
 * file or oc.h (which #includes this file).  This is necessary
 * to insure that macros such as TCL_THREADS are properly defined
 * during the #include processing of tcl.h.
 */

#ifndef _OC_IMPORTS
#define _OC_IMPORTS

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <limits>
#include <mutex>     // std::mutex, std::lock
#include "ocport.h"

/*
 * Need 'extern "C"' to guarantee that typedefs of different function
 * types established in tcl.h, tk.h, or here are always declared with
 * C linkage.
 */
#ifdef __cplusplus
extern "C" {
/*
 * Also, when compiling with a C++ compiler, create macro definitions
 * in tcl.h and tk.h as would be suitable for a standard C compiler
 * using stdarg.h.
 */
#  ifndef HAS_STDARG
#    define HAS_STDARG
#  endif
#endif

/***********************************************************************
 * Defensive programming against varying contents of different versions 
 * of tcl.h
 **********************************************************************/
#if OOMMF_THREADS
# ifndef TCL_THREADS
#  define TCL_THREADS /* Enable Tcl threads support API */
# endif
#endif

/*
 * BIG KLUDGE: If we are building under Cygwin, then a straight
 * include of tcl.h will evaluate macros assuming a unix build.  But
 * if we are using a standard Windows build of Tcl under Cygwin, then
 * the Tcl libraries use Windows-style interfaces, not unix.  This can
 * lead to runtime errors; one example: the thread mainline return
 * type, "Tcl_ThreadCreateType", is void in unix but unsigned int in
 * Windows.  Detect this combination, and pre-define the __WIN32__
 * macro so that the declarations in tcl.h match the run-time library.
 */
#if OC_SYSTEM_TYPE==OC_UNIX && OC_SYSTEM_SUBTYPE==OC_CYGWIN \
    && OC_TCL_TYPE==OC_WINDOWS && !defined(__WIN32__)
# define __WIN32__
# include <tcl.h>
# undef __WIN32__
#else
# include <tcl.h>
#endif

/*
 * Verify version of tcl.h matches the Tcl version for which ocport.h was
 * configured.  Otherwise libraries selected for linking may not be
 * compatible with headers used for compiling, leading to a run-time error.
 */

#if ((TCL_MAJOR_VERSION != CONFIG_TCL_MAJOR_VERSION) \
    || (TCL_MINOR_VERSION != CONFIG_TCL_MINOR_VERSION))
#  error "tcl.h version mismatch"
#endif

/*
 * Safety that all macros have a definition (even in Tcl 7.5).
 */

#ifndef TCL_PATCH_LEVEL
#define TCL_PATCH_LEVEL CONFIG_TCL_PATCH_LEVEL
#define TCL_RELEASE_SERIAL CONFIG_TCL_RELEASE_SERIAL
#define TCL_RELEASE_LEVEL CONFIG_TCL_RELEASE_LEVEL
#endif

/* For OOMMF 2, set new baseline requirement of Tcl 8.5 */
#if ((TCL_MAJOR_VERSION < 8) \
        || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION < 5)))
#  error "OOMMF 2 requires Tcl 8.5+"
#endif

#if !defined(_ANSI_ARGS_)
#    define _ANSI_ARGS_(x) x
#endif

/*
 * Tcl 7.5p0 had no prototype for Tcl_Free()
 */
#if (TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION == 5) \
	&& (TCL_RELEASE_LEVEL == 2) && (TCL_RELEASE_LEVEL == 0)
#define Tcl_Free Tcl_Ckfree
#endif

/*
 * Provide a fake implementation of Tcl_GetStringResult for pre-8
 * Tcl libraries.
 */
#if (TCL_MAJOR_VERSION < 8)
#define Tcl_GetStringResult(interp) (interp->result)
#endif

/*
 * Provide Tcl_GetChannelHandle() to pre-8.0 interpreters
 * The TCL_STORAGE_CLASS stuff makes sure we don't make
 * the MSVC compiler think that the Tcl library will be
 * supplying the function.
 *
 * NB: 8.1aX, 8.1b1  not supported here.
 */

#if (TCL_MAJOR_VERSION < 8)
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS
EXTERN int Tcl_GetChannelHandle _ANSI_ARGS_((Tcl_Channel, int, ClientData*));
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT
#define Tcl_GetChannelOption(i,c,o,d) Tcl_GetChannelOption(c,o,d)
#endif

/*
 * In the tcl.h file released with Tcl 8.0.3 and Tcl 8.0.4, the
 * prototype for Tcl_AppInit is mistakenly declared with linkage
 * type __declspec(dllimport) on Windows platforms when tcl.h
 * is included in other apps, causing warnings from the MSVC
 * compiler.  The following declaration and macro work around
 * that bug.
 */

#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 0) \
    && (TCL_RELEASE_LEVEL == 2) && (TCL_RELEASE_SERIAL > 2) \
    && (TCL_RELEASE_SERIAL < 5) && (OC_SYSTEM_TYPE == OC_WINDOWS)
Tcl_AppInitProc Oc_AppInit;
#define Tcl_AppInit Oc_AppInit
#endif

/*
 * Provide Tcl_PkgPresent() to pre-8.0.6 interpreters
 * The TCL_STORAGE_CLASS stuff makes sure we don't make
 * the MSVC compiler think that the Tcl library will be
 * supplying the function.
 *
 * NB: 8.1aX, 8.1b1  not supported here.
 */

#if ((TCL_MAJOR_VERSION < 8) || ((TCL_MAJOR_VERSION == 8) \
    && (TCL_MINOR_VERSION == 0) && (TCL_RELEASE_SERIAL < 6)))
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS
EXTERN char * Tcl_PkgPresent _ANSI_ARGS_((Tcl_Interp *, char *, char *, int));
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT
#endif

/*
 * A "declaration" for panic first appears in the 8.1b2 tcl.h .
 * (Really it's a macro definition to Tcl_Panic. "panic" is deprecated.)
 * For earlier Tcl versions, supply the missing prototype, but use the
 * panic() supplied by the Tcl library.
 *
 * NB: 8.1aX, 8.1b1  not supported here.
 */

#if ((TCL_MAJOR_VERSION < 8) \
    || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 0)))
typedef void (Tcl_PanicProc) _ANSI_ARGS_(TCL_VARARGS(char *, format));
EXTERN void panic _ANSI_ARGS_(TCL_VARARGS(char *,format));
# define Tcl_Panic panic
#endif

/*
 * The Tcl_SaveResult(), Tcl_RestoreResult(), and Tcl_DiscardResult()
 * routines and the Tcl_SavedResult struct were introduced in Tcl 8.1.
 * Provide a simplified form of them for pre-8.1 interpreters.
 *
 * Likewise for Tcl_UtfToExternalDString().
 *
 * NB: 8.1aX, 8.1b1  not supported here.
 */
#if ((TCL_MAJOR_VERSION < 8) \
    || ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 0)))
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS
typedef struct Tcl_SavedResult {
    char *result;
    Tcl_FreeProc *freeProc;
} Tcl_SavedResult;
EXTERN void Tcl_DiscardResult _ANSI_ARGS_((Tcl_SavedResult *));
EXTERN void Tcl_RestoreResult _ANSI_ARGS_((Tcl_Interp *, Tcl_SavedResult *));
EXTERN void Tcl_SaveResult _ANSI_ARGS_((Tcl_Interp *, Tcl_SavedResult *));
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT
#define Tcl_UtfToExternalDString(e, s, l, d) \
        (Tcl_DStringInit(d), Tcl_DStringAppend(d, s, l))
#endif

#ifndef CONST84
#    define CONST84 const
#endif
#define OC_USING_CONST84 1

#ifndef CONST
#    define CONST const
#endif

#ifndef TCL_INTEGER_SPACE
#define TCL_INTEGER_SPACE 24
#endif

/*
 * Tcl_Tell() and Tcl_Seek() changed signatures in Tcl 8.4.  Define
 * a new type for their return type to bridge the difference.
 */
#ifdef TCL_WIDE_INT_TYPE
  typedef Tcl_WideInt Oc_SeekPos;
#else
  typedef int Oc_SeekPos;
#endif

#if OC_USE_TK
# include <tk.h>
#else  /* !OC_USE_TK */
  /*
   * User-requested no-Tk build.  Supply dummy replacements here to
   * satisfy linker.  Note that this code is inside an extern "C" block,
   * so function declarations here get C linkage.
   */
# define TK_MAJOR_VERSION 0
# define TK_MINOR_VERSION 0
  inline int Tk_Init(Tcl_Interp*) { return TCL_ERROR; }
  inline int Tk_SafeInit(Tcl_Interp*) { return TCL_ERROR; }
  inline void Tk_Main(int argc,char* argv[],Tcl_AppInitProc* appInitProc) {
    Tcl_Main(argc,argv,appInitProc);
    /*
     * Note that Tcl_Main gets #define'd below to Tk_Main, so keep this
     * declaration ahead of that.
     */
  }
  inline void Tk_InitConsoleChannels(Tcl_Interp*) {}
  inline int Tk_CreateConsoleWindow() { return TCL_ERROR; }
#endif /* OC_USE_TK */

/*
 * Verify version of tk.h matches the Tk version for which ocport.h was
 * configured.  Otherwise libraries selected for linking may not be
 * compatible with headers used for compiling, leading to a run-time error.
 */
# if ((TK_MAJOR_VERSION != CONFIG_TK_MAJOR_VERSION) \
    || (TK_MINOR_VERSION != CONFIG_TK_MINOR_VERSION))
#  error "tk.h version mismatch"
# endif

/*
 * Safety that all macros have a definition (even in Tk 4.1).
 */

#ifndef TK_PATCH_LEVEL
#define TK_PATCH_LEVEL CONFIG_TK_PATCH_LEVEL
#define TK_RELEASE_SERIAL CONFIG_TK_RELEASE_SERIAL
#define TK_RELEASE_LEVEL CONFIG_TK_RELEASE_LEVEL
#endif

/*
 * Tk_SafeInit() was introduced with Tk 8.0
 */

#if (TK_MAJOR_VERSION < 8)
#define Tk_SafeInit ((Tcl_PackageInitProc *) NULL)
#endif

/*
 * Tk_InitConsoleChannels() and Tk_CreateConsoleWindow() first worked
 * in Tk 8.2.0.
 */
#if (TK_MAJOR_VERSION < 8) \
	|| ((TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION < 2)) \
	|| ((OC_SYSTEM_TYPE == OC_UNIX) && (OC_SYSTEM_SUBTYPE != OC_DARWIN))
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS
EXTERN void Tk_InitConsoleChannels _ANSI_ARGS_((Tcl_Interp *));
#define Tk_CreateConsoleWindow(interp) (0)
#undef TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT
#endif

/*
 * A pretty ugly hack to get around exit crashes in Tk 8.1.x on MS Windows.
 * The Tk Dll cleanup routines try to call Tcl_GetThreadData, but calls
 * to Tcl_* crash, unless the Tk DLL has called Tcl_InitStubs.  This is
 * normally done in Tk_Init, but since we're "statically" linking Tk,
 * we sometimes omit Tk_Init.  The routine Tk_Main also calls Tcl_InitStubs
 * from within the Tk DLL, so if we always call Tk_Main, we're safe.
 */
#if (OC_SYSTEM_TYPE == OC_WINDOWS) && (TK_MAJOR_VERSION == 8) \
        && (TK_MINOR_VERSION == 1) && (TK_RELEASE_LEVEL == 2) \
        && (TK_RELEASE_SERIAL < 2)
#define Tcl_Main Tk_Main
#endif

#ifdef __cplusplus
}	/* end of extern "C" */
#endif

/* End includes */     /* Optional directive to pimake */

/*
 * Utility routines
 */


//////////////////////////////////////////////////////////////////////////
// Random number generator routines.  The Oc_Random class (with
// Oc_RandomState helper) is an implementation of the GLIBC random()
// function.  This class provides the default functionality of the
// OMF_RANDOM macro declared in ocport.h
//    The Oc_UnifRand() function returns a random value in [0,1] with
// unif. distrib.  It is a wrapper for OMF_RANDOM.  It may be
// (re)initialized by calling Oc_Srand() or Oc_Srand(seed).  In the
// first case the seed is determined by sampling the system clock.

class Oc_RandomState {
  // Support class for Oc_Random
public:
  Oc_RandomState() : ihead(-1) {
    Init(1); // Default seed
  }
private:
  friend class Oc_Random;

  enum { SIZE = 32, SEP  =  3, WARMUP = 310 };

  int ihead;
  OC_UINT4m arr[SIZE+SEP];

  void Init(OC_UINT4m seed);
  OC_UINT4m Step();

  // For debugging. Returns 1 on success, 0 if string length n is too
  // short. The minimal length for n to store the full state is
  // (SIZE+SEP)*9 (e.g., (32+3)*9 = 315).
  int ReadRNGState(char* str,size_t n) {
    const size_t statelen = sizeof(arr)/sizeof(arr[0]);
    if(n<9*statelen) return 0; // str too short
    for(size_t i=0;i<statelen;++i) {
      sprintf(str+9*i,"%08X ", arr[i] & 0xFFFFFFFF);
    }
    str[9*statelen-1] = '\0';
    return 1;
  }

};

class Oc_Random {
  // An implementation of glibc random(), based on notes
  // by Peter Selinger, "The GLIBC random number generator,"
  // 4-Jan-2007.
  // NOTE: There is a static variable holding a global generator state.
  //   This state is shared program-wide, across threads.  Mutexes
  //   protect against re-entrancy problems, but if multiple threads
  //   access Oc_Random then the results can vary from run-to-run,
  //   depending on the relative access order between threads.  Also,
  //   mutex locking may slow generation of "random" numbers.  An
  //   alternative is for each thread to hold its own generator state,
  //   and use the Random(Oc_RandomState&) and associated
  //   Srandom(Oc_RandomState&,OC_UINT4m) calls.  This should be faster,
  //   and can provide run-to-run repeat-ability, but take care in
  //   initializing each state so that the threads don't mirror each
  //   other.  Also, the OMF_RANDOM macro in ocport.h, which allows the
  //   end user to provide an alternative random number generator, is
  //   not supported by this alternative, non-global state interface.
  //   Code using the alterative interface can use the
  //   OMF_RANDOM_IS_DEFAULT macro to detect replacement of the default
  //   random number generator.

public:
  static OC_INT4m MaxValue() { return 0x7fffffff; }
  /// NB: This function is referenced by procs.tcl to build
  ///     ocport.h.  Changes to MaxValue() may need reflection
  ///     there as well.

  // Mutex-locked calls to global random state
  static void Srandom(OC_UINT4m seed) {
#if OOMMF_THREADS
    std::lock_guard<std::mutex> lck(random_state_mutex);
#endif // OOMMF_THREADS
    state.Init(seed);
  }
  static OC_INT4m Random() {
#if OOMMF_THREADS
    std::lock_guard<std::mutex> lck(random_state_mutex);
#endif // OOMMF_THREADS
    OC_UINT4m step_result = state.Step();
    return static_cast<OC_INT4m>(step_result>>1);
  }

  // Calls using caller-defined random state.  NB: These are not
  // mutex-locked.  The caller is responsible for protecting against
  // re-entrancy --- which may allow for faster access.
  static void Srandom(Oc_RandomState& mystate,OC_UINT4m seed) {
    mystate.Init(seed);
  }
  static OC_INT4m Random(Oc_RandomState& mystate) {
    OC_UINT4m step_result = mystate.Step();
    return static_cast<OC_INT4m>(step_result>>1);
  }

  // For debugging. Returns 1 on success, 0 if string length n is too
  // short. The minimal length for n to store the full state is
  // (SIZE+SEP)*9 (e.g., (32+3)*9 = 315).
  static int ReadRNGState(char* str,size_t n) {
#if OOMMF_THREADS
    std::lock_guard<std::mutex> lck(random_state_mutex);
#endif // OOMMF_THREADS
    return state.ReadRNGState(str,n);
  }
  int ReadRNGState(Oc_RandomState& mystate,char* str,size_t n) {
    return mystate.ReadRNGState(str,n);
  }
  
private:
#if OOMMF_THREADS
  static std::mutex random_state_mutex;  // Thread-safe hack.
#endif // OOMMF_THREADS
  static Oc_RandomState state;  // global random state
};

void Oc_Srand();
void Oc_Srand(unsigned int seed);
double Oc_UnifRand();
double Oc_UnifRand(Oc_RandomState& mystate);
/*
 * Oc_UnifRand() random value in [0,1] with unif. distrib.
 * The random number generator may be (re)initialized by
 * calling Oc_Srand() or Oc_Srand(seed).  In the first case
 * the seed is determined by sampling the system clock.
 */

/* Utility filesystem command */
//  Oc_FSync mimics the unix fysnc() or Windows
// _commit()/FlushFileBuffers() routines, which force data
// buffered by the OS to disk.  Note that this code may block,
// so only call when necessary.
// The return value is 0 on success, -1 on error.
int Oc_Fsync(Tcl_Channel chan);

/* Utility math commands. */
double Oc_Polynomial(double x, double *coef, int N);
double Oc_Erf(double x);

// Math routines; std:: version self-select by type
// Note: With some compilers, #include <cmath> without a "using"
// statement will cause math library calls to revert to the old C
// standard where the argument(s) is converted to a double and the
// double version of the routine is called.  Note that the alternative
// of explicitly calling for example std::sin(x) requires an extension
// to the std namespace for x other than float, double, or long double.
using std::atan;
using std::atan2;
using std::ceil;
using std::exp;
using std::expm1;
using std::fabs;
using std::floor;
using std::hypot;
using std::log;
using std::log1p;
using std::pow;
using std::sqrt;
using std::signbit;  // Return 1 if import is negative, 0 if positive
using std::copysign;

// How should atan2(y,x) respond to y=x=0 input? The pedantic answer is
// to produce a domain error, since there is no sensible way to extend
// atan2 to (0,0). However, atan2 was originally introduced to ease
// conversion from Cartesian to polar coordinates (IBM FORTRAN-IV,
// 1961), and in that setting any finite return from atan2(0,0) is OK,
// because the corresponding radius value is zero. Many math libraries
// (e.g., GNU and Microsoft) take this stance. The IEEE 1003.1-2001
// reference, which claims ISO compatibility, goes further and defines
// the behavior wrt signed zeros. The Linux man page (release 3.53) for
// atan2, which claims C99 and POSIX.1-2001 compliance, matches the IEEE
// spec, as does the Intel x87 FPATAN (floating-point partial
// arctangent) instruction.
//   However, some C++ references state that std::atan2(0.,0.) should
// result in a domain error. Code build with the Intel C++ compiler
// returns 0 for atan2(0,0), but sets errno to 33 (domain error).  This
// behavior is unhelpful in the common use case of coordinate conversion
// noted above. The Tcl atan2 man page (from Tcl 8.3 through at least
// Tcl 8.6) state that imports "x and y cannot both be 0", but doesn't
// specify behavior if they are.
//   Implementation dependent behavior hinders portability and code
// validation. The following Oc_Atan2 function, modeled on the x87
// FPATAN instruction, is supplied as a workaround. It responds to y=x=0
// as follows:
//      y     x     Oc_Atan2(y,x)
//    +0.0  +0.0      y = +0.0
//    -0.0  +0.0      y = -0.0
//    +0.0  -0.0      atan2(y,-1) which should be +pi
//    -0.0  -0.0      atan2(y,-1) which should be -pi
template<typename T>
T Oc_Atan2(T y,T x) {
  if(y!=0 || x!=0) return atan2(y,x);
  if(signbit(x)) { // x == -0.0
    return atan2(y,-1.0);
  }
  return y;  // x == +0.0
}

// Three parameter hypot which guards against overflow and underflow.
// TODO: Compare this code to James L. Blue, "A Portable Fortran Program
// to Find the Euclidean Norm of a Vector," ACM Transactions on
// Mathematical Software, 4, 15-23 (1978).
template <typename T>
T Oc_Hypot(T x,T y,T z) {
  x=fabs(x); y=fabs(y); z=fabs(z);
  if(y<x) {
    T t = y; y = x; x = t;
  }
  if(z<y) {
    T t = z; z = y; y = t;
  }
  if(y<x) {
    T t = y; y = x; x = t;
  }
  if(z>sqrt(std::numeric_limits<T>::max()/3.)) { // z too big to square
    x /= z;
    y /= z;
    return z*sqrt(x*x+y*y+1);
  }
  if(x<sqrt(std::numeric_limits<T>::min())) { // x too small to square
    if(z<x*sqrt(std::numeric_limits<T>::max())) {
      y /= x;
      z /= x; 
      return x*sqrt(1.0+y*y+z*z);
    }
    if(z<y*sqrt(std::numeric_limits<T>::max())) {
      x /= y;
      z /= y; 
      return y*sqrt(x*x+1.0+z*z);
    }
  }
  return sqrt(x*x+y*y+z*z);
}

#ifdef __cplusplus
extern "C" {
#endif

/* Wrappers for the above. */
Tcl_CmdProc OcSrand;
Tcl_CmdProc OcUnifRand;
Tcl_CmdProc OcReadRNGState;
Tcl_CmdProc OcAtan2;
#if OC_TCL_TYPE==OC_WINDOWS && defined(__CYGWIN__)
Tcl_CmdProc OcCygwinChDir;
#endif //  OC_WINDOWS && __CYGWIN__

#ifdef __cplusplus
}	/* end of extern "C" */
#endif

#endif /* _OC_IMPORTS */
