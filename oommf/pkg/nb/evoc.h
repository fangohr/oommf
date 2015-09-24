/* FILE: evoc.h                    -*-Mode: c++-*-
 *
 * Stuff that should be moved eventually into the oc extension.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2012-03-11 05:05:40 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_EVOC
#define _NB_EVOC

#include "oc.h"

#include <stdio.h>
#include <time.h>

/* End includes */     /* Optional directive to build.tcl */

/* Note: The following constants are correct to 37 decimal digits.  The
 *  compiler should down-convert as necessary to fit into a OC_REAL8m,
 *  which we implicitly assume here to have precision less than 37
 *  decimal digits.  We also provide OC_REALWIDE versions of the same
 *  constants, for use in OC_REALWIDE computations.  We also assume that
 *  the precision of OC_REALWIDE is less than 37 decimal digits.
 */
   
#ifndef PI
#define PI               OC_REAL8m(3.141592653589793238462643383279502884L)
#endif

#ifndef WIDE_PI
#define WIDE_PI        OC_REALWIDE(3.141592653589793238462643383279502884L)
#endif

#ifndef SQRT2
#define SQRT2            OC_REAL8m(1.414213562373095048801688724209698079L)
#endif

#ifndef WIDE_SQRT2
#define WIDE_SQRT2     OC_REALWIDE(1.414213562373095048801688724209698079L)
#endif

#ifndef SQRT1_2 /* 1/sqrt(2) */
#define	SQRT1_2          OC_REAL8m(0.7071067811865475244008443621048490393L)
#endif

#ifndef WIDE_SQRT1_2
#define	WIDE_SQRT1_2   OC_REALWIDE(0.7071067811865475244008443621048490393L)
#endif

#ifndef MU0     /* 4 PI 10^7 */
#define MU0             OC_REAL8m(12.56637061435917295385057353311801154e-7L)
#endif

#ifndef WIDE_MU0
#define WIDE_MU0      OC_REALWIDE(12.56637061435917295385057353311801154e-7L)
#endif

#define SCRATCH_BUF_SIZE 1024  // Size of scratch space buffers
#define ASCIIPTR_BUF_SIZE (POINTERASCIIWIDTH+16)
/// Room + extra for %p representation of pointers


extern int Verbosity;
#ifndef DEBUGLVL
#define DEBUGLVL 0  // 0 => No debug code, 100 => Full debug code
#endif 

// Several useful macros
#define OC_SQ(x) ((x)*(x))
#define OC_MIN(x,y) ((x)<(y)?(x):(y))
#define OC_MAX(x,y) ((x)>(y)?(x):(y))
#define OC_ROUND(x) ((x)<0 ? ceil((x)-0.5) : floor((x)+0.5))
/// Note: rint() is not in the ANSI C spec, and in any event
///  may vary depending on "prevalent rounding mode".  Be
///  aware, however, that the above has a bias towards the
///  larger magnitude integer (which could be removed by
///  rounding x+0.5 to the even integer).


//////////////////////////////////////////////////////////////////////////
// Class documentation structure
struct ClassDoc {
public:
  const OC_CHAR *classname;
  const OC_CHAR *maintainer;
  const OC_CHAR *revision;
  const OC_CHAR *revdate;
  ClassDoc(const OC_CHAR *new_classname,const OC_CHAR *new_maintainer,
	   const OC_CHAR *new_revision,const OC_CHAR *new_revdate);
};

//////////////////////////////////////////////////////////////////////////
// Tcl/Tk Interface

// C-pointer <--> ascii string conversion class.
//   The client should first call GetAsciiSize to determine how large
// a buffer is needed to hold the ascii-fied pointer string, then call
// PtrToAscii to get a string representation of the ptr.  This can be
// passed across to Tcl procs as needed.  Later, when a C routine is
// called by a Tcl proc having such a string, this string can be
// converted back to a valid ptr via AsciiToPtr.  NOTE: AsciiToPtr
// should ONLY be called with strings produced by PtrToAscii.
//
class Omf_AsciiPtr
{
private:
  static const ClassDoc class_doc;
  static const size_t ascii_string_width;
public:
  static size_t GetAsciiSize() { return ascii_string_width; }

  static void PtrToAscii(const void* ptr,char* buf);
  /// It is the responsibility of the calling routine to insure
  /// that buf is dimensioned to at least GetAsciiSize() bytes.

  // Analogue to the previous, but with auto-sized Oc_AutoBuf objects.
  static void PtrToAscii(const void* ptr,Oc_AutoBuf& buf) {
    if(buf.GetLength()<GetAsciiSize()) buf.SetLength(GetAsciiSize());
    PtrToAscii(ptr,buf.GetStr());
  }

  static void* AsciiToPtr(const char* buf);
  /// NOTE 1: ONLY strings produced by Omf_AsciiPtr::PtrToAscii
  ///  should ever be sent to this routine. 
  /// Note 2: For consistency, I would prefer to make this function
  ///    static void AsciiToPtr(const char* buf,void * &ptr)
  ///  but gcc 2.7.2.1 (at least) screws up implicit casting
  ///  to void *&, apparently producing a temporary which never
  ///  gets written back into ptr.  So the above assignment seems
  ///  a safer option.  (The alternative is to be _real_ sure
  ///  that the client routine does an explicit cast to void *&.)
};

//////////////////////////////////////////////////////////////////////////
// Oc_Times function
//   This is a portable replacement for the Unix times(2) function
class Oc_TimeVal; // Forward declaration
void Oc_Times(Oc_TimeVal& cpu_time,Oc_TimeVal& wall_time,OC_BOOL reset=0);
Tcl_CmdProc OcTimes;

//////////////////////////////////////////////////////////////////////////
// Oc_TimeVal class
//  NOTE: This class supports only non-negative times.
class Oc_TimeVal {
  friend void Oc_Times(Oc_TimeVal&,Oc_TimeVal&,OC_BOOL);
private:
  static const ClassDoc class_doc;
  OC_TIMEVAL_TICK_TYPE ticks_per_second;
  OC_TIMEVAL_TICK_TYPE max_ticks;  // Largest allowed value for ticks
  /// The total number of ticks is overflow*(max_ticks+1)+ticks.
  OC_TIMEVAL_TICK_TYPE ticks;
  OC_TIMEVAL_TICK_TYPE overflow;
  void Print(FILE* fptr);  // For debugging
public:
  void Reset();
  void Reset(OC_TIMEVAL_TICK_TYPE _ticks_per_second,
	     OC_TIMEVAL_TICK_TYPE _max_ticks);

  Oc_TimeVal();
  Oc_TimeVal(const Oc_TimeVal& time);
  Oc_TimeVal(OC_TIMEVAL_TICK_TYPE _ticks_per_second,
	     OC_TIMEVAL_TICK_TYPE _max_ticks);

  OC_TIMEVAL_TICK_TYPE GetTicksPerSecond() const { return ticks_per_second; }
  OC_TIMEVAL_TICK_TYPE GetMaxTicks() const { return max_ticks; }
  OC_TIMEVAL_TICK_TYPE GetTicks() const { return ticks; }
  void SetTicks(OC_TIMEVAL_TICK_TYPE _ticks);
  OC_TIMEVAL_TICK_TYPE GetOverflow() const { return overflow; }
  void SetOverflow(OC_TIMEVAL_TICK_TYPE _overflow) { overflow=_overflow; }
  OC_BOOL IsValid() const;

  double GetTime() const; // Returns time in seconds, in floating point
  operator double() const { return GetTime(); }

  Oc_TimeVal& operator=(const Oc_TimeVal& time);
  Oc_TimeVal& operator+=(const Oc_TimeVal& time);
  Oc_TimeVal& operator-=(const Oc_TimeVal& time);
  friend OC_BOOL AreCompatible(const Oc_TimeVal& time1,
			    const Oc_TimeVal& time2);
  friend Oc_TimeVal operator+(const Oc_TimeVal& time1,
			      const Oc_TimeVal& time2);
  friend Oc_TimeVal operator-(const Oc_TimeVal& time1,
			      const Oc_TimeVal& time2);
  friend Oc_TimeVal GetBigger(const Oc_TimeVal& time1,
                              const Oc_TimeVal& time2);
  friend Oc_TimeVal GetSmaller(const Oc_TimeVal& time1,
                               const Oc_TimeVal& time2);
};

#endif // _NB_EVOC
