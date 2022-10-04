/* FILE: evoc.h                    -*-Mode: c++-*-
 *
 * Stuff that should be moved eventually into the oc extension.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/06/20 19:16:01 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_EVOC
#define _NB_EVOC

#include "oc.h"

#include <cstdio>


#if (OC_SYSTEM_TYPE == OC_UNIX)
#include <sys/time.h>  // Some of these may be OS dependent...
#include <sys/times.h>
#include <unistd.h>
# if defined(__APPLE__) && defined(__MACH__)  // Mac OS X
# include <mach/mach.h>
# include <mach/mach_time.h>
# endif // Mac OS X
#endif // OC_SYSTEM_TYPE

#include <ctime>

using std::ceil;
using std::floor;

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

// Napping
inline void Oc_MilliSleep(unsigned int milliseconds)
{
  // May want to change this to use C++ sleep_for function
  //  #include <chrono>
  //  #include <thread>
  //  std::this_thread::sleep_for(std::chrono::milliseconds(x));
  Tcl_Sleep(milliseconds);
}

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
// Macros for timing functions:
//
// For Unix platforms, use the following macros:
//   OC_HAS_TIMES
//   OC_HAS_CLOCK
//   OC_HAS_GETTIMEOFDAY
// If OC_HAS_TIMES is defined, then Oc_Times will use the system times()
// command.  Otherwise, if OC_HAS_CLOCK is defined, then clock() will be
// used to determine the cpu time; if OC_HAS_GETTIMEOFDAY is defined, then
// gettimeofday() will be used to set the wall (elapsed) time.
// On Windows platforms, clock() and GetTickCount() are used.
// Alternately, define
//   NO_CLOCKS
// to have these routines always return 0.  This always overrides the
// other macros.

///// TEMPORARY MACROS, TO BE FIXED UP BY DGP ////////////////////
#if !defined(NO_CLOCKS) && (OC_SYSTEM_TYPE != OC_WINDOWS)
#  ifndef CLK_TCK
#    ifdef _SC_CLK_TCK
#      define CLK_TCK sysconf(_SC_CLK_TCK)
#    elif defined(HZ)
#      define CLK_TCK HZ
#    endif
#  endif
#  ifdef CLK_TCK
#    define OC_HAS_TIMES
#  endif
#  ifdef CLOCKS_PER_SEC
#    define OC_HAS_CLOCK
#  endif
#  define OC_HAS_GETTIMEOFDAY
#endif

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

//////////////////////////////////////////////////////////////////////////
// Oc_Ticks class
//   Oc_Ticks is a system-specific lightweight class for time values.
// Clock queries are performed using fast, mutex-free methods.  The
// Seconds() member function exports the internal state in seconds.
//
// For Mac OS X, try
//  #include <mach/mach.h>
//  #include <mach/mach_time.h>
//    mach_absolute_time() * info from mach_timebase_info

#ifdef NO_CLOCKS
class Oc_Ticks {
public:
  void ReadWallClock() {}
  Oc_Ticks& operator+=(const Oc_Ticks&) {}
  Oc_Ticks& operator-=(const Oc_Ticks&) {}
  double Seconds() const { return 0.0; }
};

#elif (OC_SYSTEM_TYPE == OC_WINDOWS)
class Oc_Ticks {
public:
  void ReadWallClock() {
    QueryPerformanceCounter(&tick);
    // Alternatives are GetTickCount() and GetTickCount64()
  }
  Oc_Ticks& operator+=(const Oc_Ticks& other) {
    tick.QuadPart += other.tick.QuadPart;  return *this;
  }
  Oc_Ticks& operator-=(const Oc_Ticks& other) {
    tick.QuadPart -= other.tick.QuadPart;  return *this;
  }
  double Seconds() const {
    return double(tick.QuadPart)*CONVERT_TO_SECONDS;
  }
private:
  static double GetTickPeriod();
  static const double CONVERT_TO_SECONDS;
  LARGE_INTEGER tick;
  // LARGE_INTEGER is a 64-bit union consisting of a 32-bit DWORD
  // LowPart and a 32-bit LONG HighPart, or alternatively 64-bit
  // LONGLONG QuadPart.  The code above assumes that the compiler
  // supports 64-bit integers (i.e., LONGLONG).
};

#elif defined(__APPLE__) && defined(__MACH__)
// Mac OS X
class Oc_Ticks {
public:
  void ReadWallClock() {
    tick = mach_absolute_time();
    // The alternative is to use gettimeofday(), which appears
    // to be of similar speed, but is presumably less accurate.
  }
  Oc_Ticks& operator+=(const Oc_Ticks& other) {
    tick += other.tick;  return *this;
  }
  Oc_Ticks& operator-=(const Oc_Ticks& other) {
    tick -= other.tick;  return *this;
  }
  double Seconds() const {
    return double(tick)*CONVERT_TO_SECONDS;
  }
private:
  static double GetTickPeriod();
  static const double CONVERT_TO_SECONDS;
  uint64_t tick;
};

#elif defined(__linux__) && defined(CLOCK_REALTIME_COARSE)
class Oc_Ticks {
public:
  void ReadWallClock() {
    clock_gettime(CLOCK_REALTIME_COARSE,&tick);
    // Precision is nanoseconds.  Resolution can be determined
    // by calling clock_getres().
    // NOTE: Constant CLOCK_REALTIME is more portable than
    //       CLOCK_REALTIME_COARSE (the former is POSIX, the latter
    //       Linux specific), but on my Linux tests CLOCK_REALTIME has
    //       overhead similar to gettimeofday().  There are also
    //       CLOCK_MONOTONIC and CLOCK_MONOTONIC_COARSE options, with
    //       the analogous portability and speed properties.
    //       CLOCK_REALTIME_COARSE may be a sliver faster than
    //       CLOCK_MONOTONIC_COARSE, which is why it is used here but
    //       CLOCK_MONOTONIC_COARSE could be used instead if the needs
    //       arises --- in particular, the CLOCK_MONOTONIC variants are
    //       more robust against changes to the system clock.
  }
  Oc_Ticks& operator+=(const Oc_Ticks& other) {
    tick.tv_sec += other.tick.tv_sec;
    long test = 1000000000L - other.tick.tv_nsec;
    if(tick.tv_nsec < test) {
      tick.tv_nsec += other.tick.tv_nsec;
    } else {
      tick.tv_nsec -= test;
      ++tick.tv_sec;
    }
    return *this;
  }
  Oc_Ticks& operator-=(const Oc_Ticks& other) {
    tick.tv_sec -= other.tick.tv_sec;
    if(other.tick.tv_nsec <= tick.tv_nsec) {
      tick.tv_nsec -= other.tick.tv_nsec;
    } else {
      tick.tv_nsec = 1000000000 - (other.tick.tv_nsec - tick.tv_nsec);
      --tick.tv_sec;
    }
    return *this;
  }
  double Seconds() const {
    return double(tick.tv_sec) + 1e-9*double(tick.tv_nsec);
  }
private:
  struct timespec tick;
};


#elif defined(OC_HAS_GETTIMEOFDAY)
class Oc_Ticks {
public:
  void ReadWallClock() {
    gettimeofday(&tick,NULL);
    // precision is microseconds
  }
  Oc_Ticks& operator+=(const Oc_Ticks& other) {
    tick.tv_sec += other.tick.tv_sec;
    suseconds_t test = 1000000 - other.tick.tv_usec;
    if(tick.tv_usec < test) {
      tick.tv_usec += other.tick.tv_usec;
    } else {
      tick.tv_usec -= test;
      ++tick.tv_sec;
    }
    return *this;
  }
  Oc_Ticks& operator-=(const Oc_Ticks& other) {
    tick.tv_sec -= other.tick.tv_sec;
    if(other.tick.tv_usec <= tick.tv_usec) {
      tick.tv_usec -= other.tick.tv_usec;
    } else {
      tick.tv_usec = 1000000 - (other.tick.tv_usec - tick.tv_usec);
      --tick.tv_sec;
    }
    return *this;
  }
  double Seconds() const {
    return double(tick.tv_sec) + 1e-6*double(tick.tv_usec);
  }
private:
  struct timeval tick;
};

#elif defined(OC_HAS_TIMES)
class Oc_Ticks {
public:
  void ReadWallClock() {
    struct tms dummy;
    tick = times(&dummy);
    // precision is CLK_TCK or sysconf(_SC_CLK_TCK)
  }
  Oc_Ticks& operator+=(const Oc_Ticks& other) {
    tick += other.tick;
    return *this;
  }
  Oc_Ticks& operator-=(const Oc_Ticks& other) {
    tick -= other.tick;
    return *this;
  }
  double Seconds() const {
    return double(tick)*(1.0/double(CLK_TCK));
  }
private:
  clock_t tick;
};

#else
# error No timer found; fix or define NO_CLOCKS
#endif // NO_CLOCKS


#endif // _NB_EVOC
