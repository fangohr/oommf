/* FILE: doubledouble.cc
 * Main source file for Xp_DoubleDouble class
 *
 *  Algorithms based on TJ Dekker, "A floating-point technique for
 *  extending the available precision," Numer. Math. 18, 224-242 (1971),
 *  and Jonathan Richard Shewchuk, "Adaptive precision floating-point
 *  arithmetic and fast robust geometric predicates," Discrete and
 *  Computational Geometry, 18, 305-363 (1997).
 *
 *  See also SM Rump, T Ogita, S Oishi, "Accurate Floating-Point
 *  Summation Part I: Faithful Rounding," Siam J. Sci. Comput., 31,
 *  189-224 (2008), and SM Rump, T Ogita, S Oishi, "Accurate
 *  Floating-Point Summation Part II: Sign, K-Fold Faithful and Rounding
 *  to Nearest," Siam J. Sci. Comput., 31, 1269-1302 (2008).
 *
 *  See also NOTES VI, 22-Nov-2012 through 19-Jan-2013, pp 107-137.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * NOTE: 32-bit Visual C++ apparently can't handle alignment
 *       properly for pass-by-value SSE.  In this case we
 *       need to fall back to non-SSE implementation.
 *
 *
 * ACCURACY ESTIMATES:
 *
 * For inputs "in-range", there are no known examples where the error
 * for the algebraic operations (+, -, *, /, sqrt) is more than 0.5
 * ULP.  For the transcendental operations the worst known errors are
 * slightly more than 0.5 ULP, but substantially less than 1.0 ULP.
 *
 * In this context, ULP is defined relative to the lead bit in the upper
 * word of the double-double result.  To be specific, for result "z", if
 * 2^n <= |z| < 2^(n+1), then 1 ULP = 2^(n-2*p) where p is the precision
 * of the underlying double type (i.e., p=53 for the usual IEEE 8-byte
 * float type).  (The nominal precision of the double-double type is
 * 2*p+1.)
 *
 * "In-range" inputs means the magnitudes of the inputs are bounded away
 * from 0 and infinity; the particular bounds depend on the operations,
 * but in the worst-case mean, for an input x, 2*XP_DD_TINY < |x| <
 * XP_DD_SPLITMAX, where XP_DD_TINY and XP_DD_SPLITMAX are declared in
 * the system-specific (constructed) header file ddstub.h.  These values
 * are
 *
 *   XP_DD_TINY = 2^(t+2*p)
 *
 *   XP_DD_SPLITMAX = 2^(H-1-(p+1)/2)
 *
 * where t is the smallest exponent with floating-point representation
 * such that 2^t > 0, and H is the largest exponent such that 2^(H-1)
 * has a finite floating-point representation.  For the usual IEEE
 * 8-byte floating-point type, t=-1074 and H=1024.  (In the ddstub.h
 * header, t is XP_DDFLOAT_VERYTINY_EXP, and H is XP_DDFLOAT_HUGE_EXP.
 * There is an additional value declared in ddstub.h,
 * XP_DDFLOAT_TINY_EXP, sometimes referenced as T, for which 2^T is the
 * smallest (in magnitude) normalized value (i.e., larger than any
 * subnormal).  For 8-byte IEEE floating point, T=-1022.)
 *
 * If the macro XP_RANGE_CHECK is true, and the underlying
 * double-arithmetic is IEEE compliant, then "in-range" is expanded
 * (with the same accuracy) to include the full representable finite
 * double-double range.  Positive and negative infinities are also
 * handled appropriately.  NaNs are also supported, albeit with no
 * distiction between quiet and signaling NaNs.  Support for signed
 * zeros is not complete; in particular addition and renormalization
 * operations may lose the sign on zeros.  (Full signed zero support
 * could probably be added, but the speed penalty may be significant
 * enough that a macro separate from XP_RANGE_CHECK should be
 * introduced to control whether or not it is engaged.)
 *
 * WARNING: Most C++ compilers have options that relax some of the IEEE
 * floating-point specifications.  Use with care.  Some of these options
 * change the settings of the processor floating point control word(s),
 * disabling for example gradual underflow (i.e., enable flush to zero).
 * These settings can carry between libraries, so that even if the
 * Xp_DoubleDouble code itself is compiled without the offending option,
 * problems may still occur if Xp_DoubleDouble is linked to code
 * compiled with that option.
 */

// Enable long double printf support in MinGW.  The
// __USE_MINGW_ANSI_STDIO macro needs to be set before
// the system header files are #include'd.
#if defined(__MINGW32__) && !defined(__USE_MINGW_ANSI_STDIO)
# define __USE_MINGW_ANSI_STDIO 1
#endif

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <iomanip>
#include <iostream>
#include <limits>  // For std::numeric_limits<T>::infinity()
#include <string>

#include <cstring>

#include "doubledouble.h"

/* End includes */

// Specify std namespace to get automatic overloading of basic math
// library functions.  We want this rather than explicitly calling, say,
// std::atan(x) because if x is a non-base type (e.g. an mpfr type) then
// we want overloading outside to a function outside of the std
// namespace.
using namespace std;

////////////////////////////////////////////////////////////////////////
// For some unknown reason, Microsoft Visual C++ versions 9 through 11
// generate faster code with /fp:precise than /fp:fast.  If this
// changes with later releases of Visual C++, add a version check
// around the following pragma.  The speed increase with Visual C++ 9
// is about 50% for both x87 and SSE in routines that use a lot of
// multiplication.  (OTOH, operator+ is more or less unaffected.)
#if defined(_MSC_VER)
# pragma float_control ( precise, on, push )
#endif // _MSC_VER

////////////////////////////////////////////////////////////////////////

// Workarounds for some missing or misnamed functions in various compilers
#if defined(_MSC_VER) || defined(__DMC__)
#define snprintf _snprintf
#endif // _MSC_VER || __DMC__


// Compiler version-specific optimization bug workarounds
// NOTE: There are some compiler-specific macros in
// oommf/pkg/nb/xpfloat.h for controlling optimizations involving
// floating-point non-associativity that might be useful in this context
// too.
#if defined(__GNUC__) && __GNUC__ == 4 \
  && !defined(__PGIC__) && !defined(__INTEL_COMPILER)
// Builds using gcc (GCC) 4.8.5 20150623 (Red Hat 4.8.5-4) with
// options -O2 and -march=native together on some platforms cause
// failures in sqrt and the trig functions.  OTOH, -O1 appears okay.
// To be safe, just push optimization level down to 1 for all g++ 4
// subversions.
//   Note that this control is in effect for the full file.
# pragma GCC optimize ("-O1")
#endif
#if defined(__GNUC__) && defined(__GNUC_MINOR__) \
  && !defined(__PGIC__) && !defined(__INTEL_COMPILER) \
  && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
// The -ffp-contract=off control, which disables contraction of
// floating point operations (such as changing a*b+c into an fma
// instruction) appears in gcc 4.6.
# pragma GCC optimize ("-ffp-contract=off")
#endif
#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
# pragma fp_contract (off)
#endif

////////////////////////////////////////////////////////////////////////

// Workaround for extra *range* in the PC_53 (53-bit mantissa) mode
// of the x87 floating point processor:
#ifndef XP_DDFLOAT_EXTRA_RANGE
# if defined(_MSC_VER) && \
   ((defined(_M_IX86_FP) && _M_IX86_FP < 2) || \
    (!defined(_M_IX86_FP) && defined(_M_IX86)))
#  define XP_DDFLOAT_EXTRA_RANGE 1
# else
#  define XP_DDFLOAT_EXTRA_RANGE 0
# endif
#endif
// An alternative to these macro checks would be to have buildstub
// detect the extra range at runtime and drop the appropriate value
// for XP_DDFLOAT_EXTRA_RANGE into the ddstub.h file.

#if XP_RANGE_CHECK && XP_DDFLOAT_EXTRA_RANGE
inline XP_DDFLOAT_TYPE XP_FLATTEN_FLOAT(XP_DDFLOAT_TYPE x) {
  volatile XP_DDFLOAT_TYPE y = x;  return y;
}
#else
# define XP_FLATTEN_FLOAT(x) (x)
#endif
// The story here is that when the x87 is in PC_53 mode all operations
// are properly rounded for 53-bit mantissa precision, but the
// extended range of the x87's extended mode (15 bits vs. 11 bits) is
// preserved.  This means that intermediate values that would under-
// or over-flow in true 8-byte IEEE arithmetic may stay finite.  The
// proper INF or 0 value is only obtained when the value is written
// out of register into memory.  This behavior breaks algorithms that
// rely on overflows to detect too large values, and also breaks
// algorithms that try to do precise multi-word rounding on underflow.
// A workaround is to force intermediate values to memory at strategic
// places.  Here this is done by forcing the value through a volatile
// variable --- see the definition for the XP_FLATTEN_FLOAT above.  An
// alternative is to pass the value through a function call, but when
// I tried this with an essentially NOP function placed at the bottom
// of this file, it failed to fix the extended range problem.  It
// might work to put the function body in a different module, but it
// is not certain that that would be faster than using the volatile.
//   The only platform+compiler setting that I know of that exhibits
// this problem is Windows + Visual C++ with x87 floating point.  This
// problem goes away on all x86 platforms if SSE arithmetic is used.
// All 64-bit versions of Visual C++ produce (only?) SSE floating
// point, and the 32-bit version of Visual C++ 2012 and later produce
// SSE floating point by default (x87 floating point may be enabled in
// this last case via the /arch:IA32 compile switch).  So in practice
// this particular problem should be fading away.
//  When active, the XP_FLATTEN_FLOAT calls do sport a performance
// penalty, but most are in non-critical code sections.  The one
// exception is inside the Xp_DoubleDouble::Split routine.  But this
// appears to unavoidable if one is using legacy hardware (x87).  An
// alternative to using XP_FLATTEN_FLOAT calls is to use a "strict"
// rather than "precise" floating-point model, either via the /fp:strict
// command line switch to cl, or otherwise via the pragmas
//   #pragma float_control ( precise, on, push )
//   #pragma fenv_access ( on )
// The latter though really hurts x87 floating point performance; some
// of the Xp_DoubleDouble functions are slowed by a factor of two or
// three.
//   The Visual C++ docs imply that under /arch:SSE2 the compiler may
// emit both SSE and x87 opcodes, but in testing I have not come
// across any extra-range problems in Visual C++ binaries produced
// using the /arch:SSE2 switch.  OTOH I've only tested on machines
// that support SSE2, so if the compiler is producing multiply code
// paths (one for SSE2+x87 and one for x87 only) then I wouldn't see
// this.

// There is a duplicate of the following function in doubledouble.cc.
// Might want to find a way to maintain only one copy.
std::ostream& operator<<(std::ostream& os,const Xp_DoubleDouble& x) {
  std::ios_base::fmtflags original_flags = os.flags();
  if((original_flags & std::ios_base::scientific)
     == std::ios_base::scientific) {
    // Use scientific format
    os << ScientificFormat(x,static_cast<int>(os.width()),
                           static_cast<int>(os.precision()));
  } else {
    const int fieldwidth = HexFloatWidth();
    os << "[" << std::setw(fieldwidth) << HexFloatFormat(x.Hi())
       << "," << std::setw(fieldwidth) << HexFloatFormat(x.Lo()) << "]";
  }
  os.flags(original_flags);
  return os;
}

void Xp_DoubleDouble::ReadHexFloat
(const char* buf,
 const char*& endptr)
{ // Reads two hex-float values from buf, makes a DoubleDouble value
  // from them, and returns a pointer to the first character beyond.
  // Throws a std::string on error.
  const char* cptr;
  XP_DDFLOAT_TYPE val0 = ReadOneHexFloat(buf,cptr);
  if(cptr == 0) {
    std::string msg = "Bad input to ReadHexFloat (A): " + std::string(buf);
    throw msg;
  }
  XP_DDFLOAT_TYPE val1 = ReadOneHexFloat(cptr,cptr);
  if(cptr == 0) {
    std::string msg = "Bad input to ReadHexFloat (B): " + std::string(buf);
    throw msg;
  }
  if(Xp_IsFinite(val0)) {
    TwoSum(val0,val1,a0,a1);
  } else {
    a0 = a1 = val0;
  }

  while(*cptr != ']' && *cptr != '\0') ++cptr;
  if(*cptr == ']') ++cptr;
  endptr = cptr;
}

class PowersOfTen {
  // Stores non-negative powers of ten, in triple-double format, for
  // quick reference.  Uses LdExp10 to compute the values, so accuracy
  // is limited by that routine.
private:
  struct Triplet {
    XP_DDFLOAT_TYPE a0, a1, a2;
    Triplet(XP_DDFLOAT_TYPE b0,XP_DDFLOAT_TYPE b1,XP_DDFLOAT_TYPE b2)
      : a0(b0), a1(b1), a2(b2) {}
  };
  std::vector<Triplet> powers_of_ten;
public:
  PowersOfTen(int maxN);
  int IsLessThan(XP_DDFLOAT_TYPE x0,XP_DDFLOAT_TYPE x1,XP_DDFLOAT_TYPE x2,
                 int power) const;
  // Returns 1 if (x0,x1,x2) < 10^power, 0 otherwise
};

PowersOfTen::PowersOfTen(int maxN)
{ // Implicitly assume maxN >= 1
  powers_of_ten.push_back(Triplet(1.,0.,0.));  // 10^0
  powers_of_ten.push_back(Triplet(10.,0.,0.)); // 10^1
  Triplet t(0.,0.,0.);
  for(int i=2;i<=maxN;++i) {
    Xp_DoubleDouble::LdExp10(1.,0.,0.,i,t.a0,t.a1,t.a2);
    powers_of_ten.push_back(t);
  }
}

int PowersOfTen::IsLessThan
(XP_DDFLOAT_TYPE x0,XP_DDFLOAT_TYPE x1,XP_DDFLOAT_TYPE x2,
 int power) const
{
  assert(power<static_cast<int>(powers_of_ten.size()));
  const Triplet& t = powers_of_ten[power];
  if(x0<t.a0) return 1;
  if(x0>t.a0) return 0;
  if(x1<t.a1) return 1;
  if(x1>t.a1) return 0;
  if(x2<t.a2) return 1;
  return 0;  // x is >= t
}

template <int N>
class Base10Digits {
private:
  int tenbite;
  XP_DDFLOAT_TYPE invten_a0, invten_a1, invten_a2;
  std::vector<unsigned char> digs;
public:
  Base10Digits() {
    tenbite = static_cast<int>(floor(pow(10,N)+0.5));
    Xp_DoubleDouble::SloppyRecip(double(tenbite),0.,0.,
                                 invten_a0,invten_a1,invten_a2);
    digs.resize(N*(tenbite+1)); // +1 safety
    for(int i=0;i<tenbite;++i) {
      int itmp = i;
      for(int j=N-1;j>=0;--j) {
        digs[N*i+j] = '0' + itmp%10;
        itmp /= 10;
      }
    }
    for(int j=N-1;j>=0;--j) {
      digs[N*tenbite+j] = '?';
    }
  }
  const unsigned char* operator[](int k) const {
    return &(digs[N*k]);
  }
  int PowerOfTenBite() const { return tenbite; }
  void InvTen(XP_DDFLOAT_TYPE& a0,XP_DDFLOAT_TYPE& a1,
              XP_DDFLOAT_TYPE& a2) const {
    a0=invten_a0;
    a1=invten_a1;
    a2=invten_a2;
  }
};

const double xpddlog_10_2 = log10(2.0);

std::string
ScientificFormat(const Xp_DoubleDouble& x,int width,int precision)
{
  // Some values for development testing.  Test these for precision
  // values across the range from 0 to 35.
  //
  // Xp_DoubleDouble::DD_PI
  // ldexp(Xp_DoubleDouble::DD_PI,-HUGE_EXP+2+MANTISSA_PRECISION)
  // ldexp(Xp_DoubleDouble::DD_PI,520)
  // ldexp(Xp_DoubleDouble::DD_PI,HUGE_EXP-2)
  //
  // ldexp(Xp_DoubleDouble::DD_PI,-HUGE_EXP+2+MANTISSA_PRECISION-LDEXPOFF)
  //   with LDEXPOFF = 3, 6, 59, others?  These will underflow; check for
  //   proper handling of mantissa from frexp.
  //
  // -999999.75
  //   Check for proper rounding for precision = 0 through 5.
  //
  // Xp_DoubleDouble(1,-XP_DDFLOAT_EPSILON/2) at precisions >= 15
  // Xp_DoubleDouble(1,-XP_DDFLOAT_EPSILON/4) at precisions >= 15
  // Xp_DoubleDouble(1,-XP_DDFLOAT_EPSILON/12) at precisions >= 16
  //   Check handling of a0 overestimation at digit boundary.
  //
  const int BITESIZE=3;
  static const Base10Digits<BITESIZE> digitchars; // Initialized exactly once
  const int power_of_ten_bite = digitchars.PowerOfTenBite(); // 10^BITESIZE

  // Triple-double representation of 1/10^BITESIZE.
  Xp_DoubleDouble invten; XP_DDFLOAT_TYPE invten_a2;
  digitchars.InvTen(invten.a0,invten.a1,invten_a2);

  // Computation breakpoints.  Values larger than or equal to break1
  // need to use full remainder computations, as explained in NOTES
  // VIII, 20/21-Aug-2018, pp. 5-9; see (9).  Any integer value smaller
  // than break2 can be held exactly in a single XP_DDFLOAT_TYPE
  // variable.  Value break3 is the value corresponding to break1 for
  // single XP_DDFLOAT_TYPE variables.
  // Test case for break2: 14119048864730643
  static const XP_DDFLOAT_TYPE break1
    = pow(2,(2*XP_DDFLOAT_MANTISSA_PRECISION+1)
          -int(ceil(BITESIZE/xpddlog_10_2)))*power_of_ten_bite;
  static const XP_DDFLOAT_TYPE break2 = XP_DDFLOAT_POW_2_MANTISSA;
  static const XP_DDFLOAT_TYPE break3
    = pow(2,XP_DDFLOAT_MANTISSA_PRECISION
          -int(ceil(BITESIZE/xpddlog_10_2)))*power_of_ten_bite;

  if(!Xp_IsFinite(x.a0)) {
    std::string ystr;
    if(Xp_IsPosInf(x.a0)) {
      ystr = "Inf";
    } else if(Xp_IsNegInf(x.a0)) {
      ystr = "-Inf";
    } else {
      ystr = "NaN";
    }
    return ystr;
  }
  // Determine standard fill width.  The actual fill width will be
  // larger if the power-of-ten exponent magnitude in larger than 999.
  // Example:
  //
  //   -3.14159e+000
  //
  // One place for the sign, leading digit, and '.', then precision
  // spaces followed by five spaces for the exponent.  Subtract one if
  // precision is 0, because in that case we drop the '.'.
  char ybuf[1024];
  const int fillwidth = 8 + precision + (precision>0 ? 0 : -1);
  assert(fillwidth+16<static_cast<int>(sizeof(ybuf)));
  /// 16 is safety for big exponent
  const int fillstart = (fillwidth<width ? width-fillwidth : 0);
  for(int i=0;i<fillstart;++i) ybuf[i]=' '; // Left fill with spaces
  char* const yfill = ybuf + fillstart;
  const int expstart = fillstart + fillwidth - 5; // Relative to ybuf

  // Special case handling for zero
  if(x.a0 == 0.0) {
    int offset = 0;
    yfill[offset++]=(signbit(x.a0) ? '-' : ' ');
    yfill[offset++] = '0';
    if(precision>0) yfill[offset++] = '.';
    for(int i=0;i<precision;++i) yfill[offset++] = '0';
    strcpy(yfill + offset,"e+000");
    return std::string(ybuf);
  }

  // Compute power-of-ten exponent necessary to convert x to an integer
  // with the specified number of digits.  In principle rounding in the
  // multiplication by xpddlog_10_2 could cause error, but an explicit
  // check for base 2 exponents shows that this is not a problem for
  // binary exponents smaller than 146,964,308.
  //   Digits beyond the precision of the variable are typically not
  // meaningful, so restrict the active precision count to the number of
  // decimal digs required to exactly specify the binary representation.
  // See Steele and White (1990/2004), DOI: 10.1145/989393.989431
  static const int max_active_precision
    = 2 + int(floor((2*XP_DDFLOAT_MANTISSA_PRECISION+1)*xpddlog_10_2));
  static const PowersOfTen ten_cache(max_active_precision+1);
  const int active_precision
    = (precision < max_active_precision ? precision : max_active_precision);
  int exponent;
  XP_DDFLOAT_TYPE mant = fabs(XP_FREXP(x.a0,&exponent));
  assert(mant > 0.0);
  while(mant < 1.0) {
    // Underflow case
    mant *= 2;
    --exponent;
  }
  // The power of ten for the import x is log_10(x), but that is
  // expensive to compute.  Instead, we compute the underestimate based
  // on log(2)*t < log(1+t) for 0<t<1.  This may cause the computed
  // powten to be low by 1, but that relatively rare case will be
  // corrected by the mantissa overflow check which follows.  BTW, if
  // desired a better underestimate to log(1+t) on [0,1] is a*t+b*t^2
  // with a=2*log2-0.5, b = log2-a with max error of about 0.014, as
  // compared to 0 <= log(1+t)-log(2)*t < 0.06.
  //   This approximate ignores the contribution of x.a1.  This only
  // has an effect if x.a0 is a power of ten exactly and x.a1 is
  // negative.  This presumably rare event is handled by the mantissa
  // underflow test below.
  XP_DDFLOAT_TYPE powten_estimate = (mant-1.0+exponent)*xpddlog_10_2;
  int powten = static_cast<int>(powten_estimate);
  if((powten_estimate-powten)<XP_DDFLOAT_EPSILON*xpddlog_10_2) {
    // In this boundary case, first try working one power lower.  If
    // necessary this will be corrected by the mantissa overflow check.
    --powten;
  }

  int adjpow = -1*powten+active_precision;
  Xp_DoubleDouble y; XP_DDFLOAT_TYPE y_a2(0.0);
  Xp_DoubleDouble::LdExp10(x.a0,x.a1,0.0,adjpow,y.a0,y.a1,y_a2);
  if(signbit(y)) {
    yfill[0]='-';
    y.a0 *= -1;
    y.a1 *= -1;
    y_a2 *= -1;
  } else {
    yfill[0]=' ';
  }
  // The following round-to-integer is mostly not needed, but an
  // example where it is is pi*pow(2,-950) with precision=31.
  Xp_DoubleDouble ysave(y); XP_DDFLOAT_TYPE ysave_a2(y_a2);
  const static XP_DDFLOAT_TYPE round_adj = 0.5-XP_DDFLOAT_EPSILON/4;
  Xp_DoubleDouble::ThreeIncrement(y.a0,y.a1,y_a2,round_adj);
  Xp_DoubleDouble::Floor(y.a0,y.a1,y_a2,y.a0,y.a1,y_a2);

  // Mantissa overflow check.  This handles the case where the powten
  // estimate is low by 1, and also the case where the previous rounding
  // operation pushed y up to the next power of ten.  I don't think
  // both can happen.
  if(!ten_cache.IsLessThan(y.a0,y.a1,y_a2,active_precision+1)) {
    // Do over
    Xp_DoubleDouble::LdExp10(ysave.a0,ysave.a1,ysave_a2,-1,y.a0,y.a1,y_a2);
    ++powten;
    Xp_DoubleDouble::ThreeIncrement(y.a0,y.a1,y_a2,round_adj);
    Xp_DoubleDouble::Floor(y.a0,y.a1,y_a2,y.a0,y.a1,y_a2);
  }
  assert(ten_cache.IsLessThan(y.a0,y.a1,y_a2,active_precision+1)
         && !ten_cache.IsLessThan(y.a0,y.a1,y_a2,active_precision));

  // Right fill with zeros for digits beyond active_precision
  int ydig=precision+2+(precision>0?1:0);
  for(int i=active_precision;i<precision;++i) {
    yfill[--ydig] = '0';
  }

  // Compute remainders and write out digits, right to left.
  while(y.a0 != 0.0) {
    XP_DDFLOAT_TYPE remainder;
    if(y.a0>=break1) {
      // Use triple-double computation
      Xp_DoubleDouble tmp; XP_DDFLOAT_TYPE tmp_a2;
      Xp_DoubleDouble::SloppyProd(y.a0,y.a1,y_a2,
                                  invten.a0,invten.a1,invten_a2,
                                  tmp.a0,tmp.a1,tmp_a2);
      XP_DDFLOAT_TYPE a0 = XP_FLOOR(tmp.a0);
      XP_DDFLOAT_TYPE a0r = tmp.a0 - a0;
      XP_DDFLOAT_TYPE a1 = XP_FLOOR(tmp.a1);
      XP_DDFLOAT_TYPE a1r = tmp.a1 - a1;
      XP_DDFLOAT_TYPE a2 = XP_FLOOR(tmp_a2);
      XP_DDFLOAT_TYPE a2r = tmp_a2 - a2;
      Xp_DoubleDouble::Normalize(a0,a1,a2,a0,a1,a2);
      Xp_DoubleDouble::Normalize(a0r,a1r,a2r,a0r,a1r,a2r);
      Xp_DoubleDouble::ThreeSum(a0,a1,a2,floor(a0r),0,0,y.a0,y.a1,y_a2);
      remainder = a0r - floor(a0r);
      remainder = floor(remainder*power_of_ten_bite+0.5);
    } else if(y.a0>=break2) {
      // Use simplified double-double remainder computation
      Xp_DoubleDouble tmp = y/power_of_ten_bite;
      /// Multiplying by invten is not significantly faster.
      y = floor(tmp);
      tmp -= y;
      tmp.DownConvert(remainder);
      remainder = floor(remainder*power_of_ten_bite+0.5);
    } else if(y.a0>=break3) {
      // Use exact single-double remainder computation
      XP_DDFLOAT_TYPE tmp = floor(y.a0/power_of_ten_bite);
      remainder = y.a0 - power_of_ten_bite*tmp;
      y = tmp;
    } else {
      // Use simplified double-double remainder computation
      remainder = y.a0/power_of_ten_bite;
      y = floor(remainder);
      remainder -= y.a0;
      remainder = floor(remainder*power_of_ten_bite+0.5);
    }
    if(!(0<=remainder && remainder<=power_of_ten_bite-1)) {
      fprintf(stderr,"REMAINDER ERROR: %g\n",double(remainder));
      fprintf(stderr,"break1 = %g\n",double(break1));
      fprintf(stderr,"break2 = %g\n",double(break2));
      fprintf(stderr,"break3 = %g\n",double(break3));
      fprintf(stderr,"y.a0 = %g\n",double(y.a0));
      exit(99);
    }

    const unsigned char* digstr
      = digitchars[static_cast<int>(remainder)];
    if(ydig>3+BITESIZE) {
      for(int i=BITESIZE-1;i>=0;--i) yfill[--ydig] = digstr[i];
    } else {
      // Handle decimal point and left zero trimming
      for(int i=BITESIZE-1;i>=0;--i) {
        if(ydig==3) {
          yfill[--ydig]='.';
        }
        if(ydig>=2) {
          yfill[--ydig] = digstr[i];
        } else { // ydig == 1
          assert(digstr[i]=='0');
          break;
        }
      }
    }
  }
  while(ydig>2) yfill[--ydig]='?';  // Safety

  // Fill power-of-ten exponent field.  Note that the value powten may
  // have been adjusted by lead digit rounding.
  snprintf(ybuf+expstart,sizeof(ybuf)-expstart,"e%+04d",powten);

  return std::string(ybuf);
}


int
Xp_DoubleDouble::QuickTest()
{
  int error_count = 0;
  Xp_DoubleDouble log2check(2.0,0.0);
  log2check = log(log2check);
  if(log2check.a0 != hires_log2[0] ||
     log2check.a1 != hires_log2[1]) {
    ++error_count;
    Xp_DoubleDouble truth;
    truth.a0 = hires_log2[0]; truth.a1 = hires_log2[1];
    cerr << " Log2: " << log2check << "\n"
         << "TRUTH: " << truth << "\n";
  }

  Xp_DoubleDouble loghalfcheck(-0.5,0.0);
  loghalfcheck = log1p(loghalfcheck);
  if(loghalfcheck.a0 != -hires_log2[0] ||
     loghalfcheck.a1 != -hires_log2[1]) {
    ++error_count;
    Xp_DoubleDouble truth;
    truth.a0 = -hires_log2[0]; truth.a1 = -hires_log2[1];
    cerr << "-Log2: " << loghalfcheck << "\n"
         << "TRUTH: " << truth << "\n";
  }

  Xp_DoubleDouble picheck(1.0,0.0);
  picheck = 4*atan(picheck);
  if(picheck.a0 != hires_pi[0] ||
     picheck.a1 != hires_pi[1]) {
    ++error_count;
    Xp_DoubleDouble truth;
    truth.a0 = hires_pi[0]; truth.a1 = hires_pi[1];
    cerr << "   pi: " << picheck << "\n"
         << "TRUTH: " << truth << "\n";
  }

  // SinCos check for large inputs.  Break the 64-bit constants up
  // into two pieces to workaround Borland C++ digestion bug.
#if XP_DDFLOAT_MANTISSA_PRECISION == 53
  const XP_DDFLOAT_TYPE SinTest[2] = {
    LdExp(8700223823437620.,-53),
    LdExp(-7046851665223794.,-110)
  };
  const XP_DDFLOAT_TYPE CosTest[2] ={
    LdExp(4662936343848225.,-54),
    LdExp(4889264888245350.,-109)
  };
#elif XP_DDFLOAT_MANTISSA_PRECISION == 64
  const XP_DDFLOAT_TYPE SinTest[2] = {
    LdExp( 17818058390000000000.L +   400245660.L,-64),
    LdExp(-10461902220000000000.L + -9884676933.L,-130)
  };
  const XP_DDFLOAT_TYPE CosTest[2] = {
    LdExp(  9549693630000000000.L +  2201165078.L,-65),
    LdExp(-11432264270000000000.L + -5994877666.L,-132)
  };
#else
#  error Unsupported floating point mantissa precision
#endif
  XP_DDFLOAT_TYPE scinput = 63*pow(XP_DDFLOAT_TYPE(2.0),50);
  Xp_DoubleDouble sincheck = sin(Xp_DoubleDouble(scinput));
  Xp_DoubleDouble coscheck = cos(Xp_DoubleDouble(scinput));
  if(sincheck.a0 != SinTest[0] ||
     sincheck.a1 != SinTest[1]) {
    ++error_count;
    Xp_DoubleDouble truth;
    truth.a0 = SinTest[0]; truth.a1 = SinTest[1];
    cerr << "sin(" << scinput << "): "
         << sincheck << "\n"
         << "           TRUTH: " << truth << "\n";
  }
  if(coscheck.a0 != CosTest[0] ||
     coscheck.a1 != CosTest[1]) {
    ++error_count;
    Xp_DoubleDouble truth;
    truth.a0 = CosTest[0]; truth.a1 = CosTest[1];
    cerr << "cos(" << scinput << "): "
         << coscheck << "\n"
         << "           TRUTH: " << truth << "\n";
  }

  return error_count;
}

// Size of unit-in-the-last-place.  This code assumes that Hi and Lo
// values are close-packed, which is what we usually want.
XP_DDFLOAT_TYPE Xp_DoubleDouble::ULP() const
{
  if(!Xp_IsFinite(a0)) return 0.0; // What else?
  if(a0 == 0.0) {
     // Note: pow may flush to zero, so use LdExp instead.
    return LdExp(1,XP_DDFLOAT_VERYTINY_EXP);
  }
  int exp;
  XP_DDFLOAT_TYPE mant =  XP_FREXP(a0,&exp);
  exp -= 2*XP_DDFLOAT_MANTISSA_PRECISION+1;
  exp = (exp < XP_DDFLOAT_VERYTINY_EXP ? XP_DDFLOAT_VERYTINY_EXP : exp);
  if((mant == 0.5 && a1<0) || (mant == -0.5 && a1>0)) {
    // If |mant| is exactly 0.5, then ULP depends on sign of lower
    // word.
    --exp;
  }
  if(exp<XP_DDFLOAT_VERYTINY_EXP) {
    exp = XP_DDFLOAT_VERYTINY_EXP; // Underflow protection
  }
  return LdExp(1,exp);
}

// Compute difference, relative to refulp.  Returns absolute difference
// if refulp is zero.
XP_DDFLOAT_TYPE
Xp_DoubleDouble::ComputeDiffULP(const Xp_DoubleDouble& ref,
                                XP_DDFLOAT_TYPE refulp) const {
  XP_DDFLOAT_TYPE result0 = Hi() - ref.Hi();
  XP_DDFLOAT_TYPE result1 = Lo() - ref.Lo();
  if(refulp != 0.0) {
    if(refulp<1.0/XP_DDFLOAT_POW_2_MANTISSA) {
      // Protect against compilers that mess up division by subnormals
      refulp *= XP_DDFLOAT_POW_2_MANTISSA;
      result0 *= XP_DDFLOAT_POW_2_MANTISSA; // Assume this doesn't overflow
      result1 *= XP_DDFLOAT_POW_2_MANTISSA;
    }
    result0 /= refulp;  // Underflow protection
    result1 /= refulp;
  }
  return result0 + result1;
}

// Non-SSE implementation //////////////////////////////////////////////
#if !XP_DD_USE_SSE

// Constants
#if XP_DDFLOAT_MANTISSA_PRECISION == 53
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_pi[5] = {
  LdExp(7074237752028440.,-51),
  LdExp(4967757600021511.,-105),
  LdExp(-8753721960665020.,-161),
  LdExp(5857755168774013.,-215),
  LdExp(5380502254069925.,-269)
};
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_log2[3] = {
  LdExp(6243314768165359.,-53),
  LdExp(7525737178955839.,-108),
  LdExp(6673460182522164.,-163)
};
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_log2_mant[3] = {
  // XP_DDFLOAT_MANTISSA_PRECISION * log(2)
  LdExp(5170245042386938.,-47),
  LdExp(6835002668432489.,-103),
  LdExp(-6295489808196385.,-157)
};
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_sqrt2[3] = {
  LdExp( 6369051672525773., -52),
  LdExp(-7843040109683798.,-106),
  LdExp( 6048680740045173.,-160)
};
#elif XP_DDFLOAT_MANTISSA_PRECISION == 64
#if !defined(__BORLANDC__) && !defined(__DMC__)
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_pi[5] = {
  LdExp(14488038916154245685.L,-62),
  LdExp(-17070460982340324539.L,-128),
  LdExp(-13253407314641263712.L,-193),
  LdExp(9434946175018387604.L,-260),
  LdExp(11890695741997477624.L,-325)
};
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_log2[3] = {
  LdExp(12786308645202655660.L,-64),
  LdExp(-15596301547560248643.L,-130),
  LdExp(17528896448286305674.L,-200)
};
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_log2_mant[3] = {
  // XP_DDFLOAT_MANTISSA_PRECISION * log(2)
  LdExp(12786308645202655660.L,-58),
  LdExp(-15596301547560248643.L,-124),
  LdExp(17528896448286305674.L,-194)
};
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_sqrt2[3] = {
  LdExp(13043817825332782212.L, -63),
  LdExp(12896923290648804670.L,-128),
  LdExp(16968162430378918503.L,-194)
};
#else // __BORLANDC__ || __DMC__
// Borland C++ 5.5.1 has a bug that causes scanning and printing of
// long doubles to drop the last couple of bits.  A hacky workaround is
// to feed the values in as two half-words, like so:
// (The Digital Mars C++ compiler 8.5.7 suffers from a similar bug.)
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_pi[5] = {
  LdExp( 14488038910000000000.L +  6154245685.L,-62),
  LdExp(-17070460980000000000.L + -2340324539.L,-128),
  LdExp(-13253407310000000000.L + -4641263712.L,-193),
  LdExp(  9434946175000000000.L +    18387604.L,-260),
  LdExp( 11890695740000000000.L +  1997477624.L,-325)
};
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_log2[3] = {
  LdExp( 12786308640000000000.L +  5202655660.L,-64),
  LdExp(-15596301540000000000.L + -7560248643.L,-130),
  LdExp( 17528896440000000000.L +  8286305674.L,-200)
};
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_log2_mant[3] = {
  LdExp( 12786308640000000000.L +  5202655660.L,-58),
  LdExp(-15596301540000000000.L + -7560248643.L,-124),
  LdExp( 17528896440000000000.L +  8286305674.L,-194)
};
const XP_DDFLOAT_TYPE Xp_DoubleDouble::hires_sqrt2[3] = {
  LdExp(13043817825000000000.L  +  332782212.L, -63),
  LdExp(12896923290000000000.L  +  648804670.L,-128),
  LdExp(16968162430000000000.L  +  378918503.L,-194)
};
#endif // __BORLANDC__ || __DMC__

#else
#  error Unsupported mantissa precision
#endif

// Externally accessible values:
const Xp_DoubleDouble
Xp_DoubleDouble::DD_SQRT2(Xp_DoubleDouble::hires_sqrt2[0],
                          Xp_DoubleDouble::hires_sqrt2[1]);
const Xp_DoubleDouble
Xp_DoubleDouble::DD_LOG2(Xp_DoubleDouble::hires_log2[0],
                         Xp_DoubleDouble::hires_log2[1]);
const Xp_DoubleDouble
Xp_DoubleDouble::DD_PI(Xp_DoubleDouble::hires_pi[0],
                       Xp_DoubleDouble::hires_pi[1]);
const Xp_DoubleDouble
Xp_DoubleDouble::DD_HALFPI(0.5*Xp_DoubleDouble::hires_pi[0],
                           0.5*Xp_DoubleDouble::hires_pi[1]);

////////////////////////////////////////////////////////////////////////
/// Start value safe block (associative floating point optimization
/// disallowed).
////////////////////////////////////////////////////////////////////////

// Note: We want the "bread-and-butter" member routines for
// Xp_DoubleDouble, namely OrderedTwoSum, TwoSum, and Split, to be
// inlined.  But we also have to be careful that the floating point
// operations are done in the order prescribed, else the results will be
// incorrect.  Since these routines are private, they should only be
// called from within this file; therefore, we don't need to define them
// in the doubledouble.h header file, but instead we can put the
// definitions here, at the top of doubledouble.cc, and still have them
// inlined everywhere they are used.  Plus we can require that this
// file, and this file alone, be compiled with special compiler command
// line flags specifying precise floating point.

inline void Xp_DoubleDouble::OrderedTwoSum
(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y,
 XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v)
 {
  // This routine is intended for the case |x|>=|y|, because
  // if |x| >= |y|, then u + v is exactly x + y.  It also
  // handles properly the case where x==0 regardless of y,
  // in which case y->u and 0->v.  One common use of this
  // routine is to normalize a multi-part value.
#ifndef NDEBUG
  if(fabs(x)<fabs(y) && x!=0.0 && Xp_IsFinite(x) && Xp_IsFinite(y)) {
    // Note that NaNs fail fabs(x)<fabs(y)
    cerr << "\nERROR: Bad input to OrderedTwoSum: x=" << x
         << ", y=" << y << "\n";
    throw("Bad input to OrderedTwoSum.");
  }
#endif
  // Note: If x and y are finite, but x+y overflows, then u will be
  // infinite, v will be -u, and u + v will be NaN.
  u = x + y;
  XP_DDFLOAT_TYPE t1 = u - x;
  v = y - t1;
}

inline void Xp_DoubleDouble::OrderedTwoSum
(XP_DDFLOAT_TYPE& x,XP_DDFLOAT_TYPE& y) {
  // Equivalent to OrderedTwoSum(x,y,x,y)
#ifndef NDEBUG
  if(fabs(x)<fabs(y) && x!=0.0 && Xp_IsFinite(x) && Xp_IsFinite(y)) {
    // Note that NaNs fail fabs(x)<fabs(y)
    cerr << "\nERROR: Bad input to OrderedTwoSum: x=" << x
         << ", y=" << y << "\n";
    throw("Bad input to OrderedTwoSum.");
  }
#endif
  XP_DDFLOAT_TYPE tx = x;
  x  += y;
  tx -= x;
  y  += tx;
}

inline void Xp_DoubleDouble::TwoSum
(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y,
 XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v)
{ // If x and y are exact, then u + v is exactly x + y
  u = x + y;
  XP_DDFLOAT_TYPE t1 = u - x;
  XP_DDFLOAT_TYPE t2 = u - t1;
  XP_DDFLOAT_TYPE t3 = y - t1;
  XP_DDFLOAT_TYPE t4 = x - t2;
  v = t4 + t3;
}

inline void Xp_DoubleDouble::TwoDiff
(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y,
 XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v)
{ // If x and y are exact, then u + v is exactly x - y
  u = x - y;
  XP_DDFLOAT_TYPE t1 = u - x;
  XP_DDFLOAT_TYPE t2 = u - t1;
  XP_DDFLOAT_TYPE t3 = y + t1;
  XP_DDFLOAT_TYPE t4 = x - t2;
  v = t4 - t3;
}

inline void Xp_DoubleDouble::Split // Prep for multiplication
(XP_DDFLOAT_TYPE x,
 XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v) {
  // The "splitmagic" constant, when mulitplies against x, produces x
  // plus a half-mantissa-width shifted version of x.  Presumably, on
  // current processors, is it faster to do the multiply by splitmagic
  // than to do a shift via ldexp and an add, because the shift
  // involves cracking out the exponent bits, adjusting them, and then
  // plastering it back up.  The "splitmagic" constant is
  //
  //    splitmagic = 1 + 2^((mantissa_precision+1)/2)
  //
  //    If |x| is close to the range limit for its type (i.e., if
  // MAX/|x| < 2^(mantissa_width/2)), then the product splitmagic*x
  // will overflow to infinity, and the return values will be NaNs.
  // If one wants, one can check for this case before calling this
  // function, and scale input x down by 2^(mantissa_width/2))
  // beforehand and scale outputs u and v up by the same amount
  // afterwards.  Of course, this takes cycles and is (presumably)
  // only needed in rather rare circumstances.
  //
#if XP_DDFLOAT_MANTISSA_PRECISION == 53
  const XP_DDFLOAT_TYPE splitmagic = 134217728.0;
#elif XP_DDFLOAT_MANTISSA_PRECISION == 64
  const XP_DDFLOAT_TYPE splitmagic = 4294967297.0L;
#else
#  error Unsupported floating point mantissa precision
#endif
  XP_DDFLOAT_TYPE t = splitmagic*x;
  u = t - x;
  u = t - u;  // u = t-(t-x) == t+(x-t)
  v = x - u;
}

///////////////////////////////////////////////////////////
// NOTE: If a FMA instruction is available, then TwoProd
// can be HUGELY simplified to
//    u = x*y
//    v = FMA(x,y,-u)
//
// Ref: Rump, Ogita, and Oishi, Siam J. Sci. Comput., 31,
// 189-224 (2008).
//
///////////////////////////////////////////////////////////
//
#ifndef XP_DEKKER_MULTIPLY
# define XP_DEKKER_MULTIPLY 0
#endif
#if !XP_DEKKER_MULTIPLY
void Xp_DoubleDouble::TwoProd
(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y,
 XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v)
{ // Multiplies x*y, storing double-width result in [u,v]
  // The version in TJ Dekker, Numer Math, 1971 has (outside the Split
  // calls) 4 real mults and 5 real adds, as compared to this
  // version which has 5 real mults and 4 real adds.  The Dekker
  // version (see below) doesn't explicitly compute x*y.
  //   However, on current hardware (2013), this version appears to
  // be about 10% faster, probably because of the x*y product up
  // front packs better with the other operations inside the hardware
  // core arithmetic units.  Another nice feature of this version as
  // compared to the Dekker version is that it sets export u to inf
  // on overflow, as opposed to the Dekker version that sets both u
  // and v to NaN.
  //
  // WARNING: If |x| or |y| are too big, then overflow can occur in
  // the Split calls.  This can happen even though the product x*y is
  // properly finite.  This occurrence is indicated (in both
  // algorithms) by the export value v being a NaN.
  u = XP_FLATTEN_FLOAT(x*y);
#if XP_USE_FMA == 0
  XP_DDFLOAT_TYPE x0,x1,y0,y1;
  Split(x,x0,x1); // WARNING! If x or y are close to the range limits
  Split(y,y0,y1); // for XP_DDFLOAT_TYPE, then returns can be NaNs.
  v = XP_FLATTEN_FLOAT(x0*y0);
  v = XP_FLATTEN_FLOAT(v - u);
  v += (x0*y1);
  v += (x1*y0);
  v += (x1*y1);
#else // XP_USE_FMA
  v = std::fma(x,y,-u); // std::fma is part of the C++11 standard
#endif // XP_USE_FMA
}
#else // TJ Dekker version
void Xp_DoubleDouble::TwoProd
(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y,
 XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v)
{
  XP_DDFLOAT_TYPE x0,x1,y0,y1;
  Split(x,x0,x1); // WARNING! If x or y are close to the range limits
  Split(y,y0,y1); // for XP_DDFLOAT_TYPE, then returns can be NaNs.
  const XP_DDFLOAT_TYPE p = XP_FLATTEN_FLOAT(x0*y0);
  const XP_DDFLOAT_TYPE s = XP_FLATTEN_FLOAT(x0*y1 + x1*y0);
  u = p + s;
  v = u - p;
  v -= s;
  v = XP_FLATTEN_FLOAT(x1*y1 - v);
}
#endif //XP_DEKKER_MULTIPLY

void Xp_DoubleDouble::SquareProd
(XP_DDFLOAT_TYPE x,
 XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v)
{ // Analogous to TwoProd, but computes x*x
  u = x*x;
  XP_DDFLOAT_TYPE x0,x1;
  Split(x,x0,x1);
  v = x0*x0;
  v -= u;
  v += 2*(x0*x1);
  v += (x1*x1);
}

// The following Rescale routines protect against underflow rounding
// errors.  See NOTES VI, 16-Aug-2013, pp 196-197.
void Xp_DoubleDouble::Rescale
(const XP_DDFLOAT_TYPE x,const XP_DDFLOAT_TYPE y,
 const XP_DDFLOAT_TYPE rescale, // Should be a power-of-two
 XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v)
{
  // Rescale
  XP_DDFLOAT_TYPE test = XP_FLATTEN_FLOAT(x*rescale);
  if(!Xp_IsFinite(test)) {
    u = v = test;
  } else if(fabs(test) < XP_DDFLOAT_MIN) {
    // Possible underflow rounding error.
    // Warning: This code may need protection from over-aggressive
    // compiler optimization.
    XP_DDFLOAT_TYPE tmp = test/rescale; // Note: 1.0/rescale may overflow
    XP_DDFLOAT_TYPE err = x - tmp; // Underflow error
    err += y;
    err = XP_FLATTEN_FLOAT(err*rescale);
    u = test + err;
    v = 0.0;
  } else {
    // Even if (x,y) is normalized on import, if y*rescale rounds due
    // to underflow then (x*rescale,y*rescale) might not be normalized.
    OrderedTwoSum(test,XP_FLATTEN_FLOAT(y*rescale),u,v);
  }
}

void Xp_DoubleDouble::Rescale
(const XP_DDFLOAT_TYPE xi,const XP_DDFLOAT_TYPE yi,const XP_DDFLOAT_TYPE zi,
 const XP_DDFLOAT_TYPE rescale, // Should be a power-of-two
 XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v)
{ // This is same as previous Rescale, but with a triple-component
  // rather than double-component input.  The output in both cases
  // is two doubles.

  // Normalize imports
  XP_DDFLOAT_TYPE x,y,z;
  OrderedTwoSum(xi,yi,x,y);
  OrderedTwoSum(y,zi,y,z);

  // Rescale
  XP_DDFLOAT_TYPE testx = XP_FLATTEN_FLOAT(x * rescale);
  XP_DDFLOAT_TYPE testy = XP_FLATTEN_FLOAT(y * rescale);
  if(!Xp_IsFinite(testx)) {
    u = v = testx;
  } else if(fabs(testy) < XP_DDFLOAT_MIN) {
    // Possible underflow rounding error.  Note: If there is a gap
    // between x and y, then testx may be considerably larger than
    // XP_DDFLOAT_MIN*XP_DDFLOAT_POW_2_MANTISSA*2 and yet have the
    // result profit from using this underflow rounding correction.
    // Warning: This code may need protection from over-aggressive
    // compiler optimization.
    XP_DDFLOAT_TYPE tmpx = testx/rescale; // Note: 1.0/rescale may overflow
    XP_DDFLOAT_TYPE errx = x - tmpx; // Potential underflow error
    XP_DDFLOAT_TYPE tmpy = testy/rescale;
    XP_DDFLOAT_TYPE erry = y - tmpy; // Potential underflow error
    XP_DDFLOAT_TYPE err_total = XP_FLATTEN_FLOAT((errx + erry + z)*rescale);
#if 0
    OrderedTwoSum(testx,err_total,u,err_total);
    OrderedTwoSum(u,testy + err_total,u,v);
#else
    OrderedTwoSum(testx,testy+err_total,u,v);
#endif
  } else {
    // Even if (x,y) is normalized, (x*rescale,y*rescale) might not
    // be, if y*rescale rounds due to underflow.
    OrderedTwoSum(testx,testy,u,v);
  }
}

// Coalesce prepares three XP_DDFLOAT_TYPE elements to be reduced to
// two.  The value on import may overlap, but it is assumed that |a0| >=
// |a1| >= |a2|.  This code computes
//
//   OrderedTwoSum(a1,a2,a1,a2);
//   OrderedTwoSum(a0,a1,a0,a1);
//
// though reordered a bit to run slightly faster.  To finish the
// reduction process, the calling routine should do
//
//   a1 += a2;
//
inline void Xp_DoubleDouble::Coalesce
(XP_DDFLOAT_TYPE& a0, // Import/export
 XP_DDFLOAT_TYPE& a1, // Import/export
 XP_DDFLOAT_TYPE& a2) // Import/export
{
  XP_DDFLOAT_TYPE u,v,s;
  u = a1 + a2;
  v = a1 - u;
  s = a0;
  a0 += u;
  a2 += v;
  s -= a0;
  a1 = s + u;
}


// CoalescePlus is a version of Coalesce that completes
// the reduction to two XP_DDFLOAT_TYPE values,
// requires the exports to be distinct from the imports,
// and *also* handles the special cases
//     a0 + xi + xi^2 where xi = ULP(a0)/2
//  and
//    (+/-)2^n*(1 - xi/2 - xi^2/2) for integer n.
inline void Xp_DoubleDouble::CoalescePlus
(const XP_DDFLOAT_TYPE a0, // Import
 const XP_DDFLOAT_TYPE a1, // Import
 const XP_DDFLOAT_TYPE a2, // Import
 XP_DDFLOAT_TYPE& b0,      // Export
 XP_DDFLOAT_TYPE& b1)      // Export
{
  XP_DDFLOAT_TYPE x,y;
  x = a1 + a2;
  b0 = a0 + x;
  b1 = a0 - b0;
  b1 += x;
  y = a1 - x;
  XP_DDFLOAT_TYPE tst = b1*(1+XP_DDFLOAT_EPSILON);
  y += a2;

  // Workaround hack so 1 + xi + xi^2 gives 1 + xi + xi^2
  // instead of 1 + xi (where xi = ULP(1)/2).  Also handles
  // case 1 - xi/2 - xi^2/2.  See NOTES VII, 11-Sep-2013, pp 2-4.
  const XP_DDFLOAT_TYPE chk = b0 + tst;
  if(chk != b0) {
    // Either |b1| = ULP(b0)/2, or else |b0|=2^n for some n.
    tst -= b1;
    if (tst == b1*XP_DDFLOAT_EPSILON) {
      // |b1| = 2^m for some m.
      // The OrderedTwoSum is needed to handle case where b1-y==b1,
      // i.e., |y|<<|b1|.
      b0 += 2*b1;
      Xp_DoubleDouble::OrderedTwoSum(b0,y-b1,b0,b1);
      return;
    }
  }
  b1 += y;
}
#if 1
Xp_DoubleDouble operator+
(const Xp_DoubleDouble& x,
 const Xp_DoubleDouble& y)
{ // Adds (x.a0,x.a1) to (y.a0,y.a1)
  // Q: Do x and y need to be normalized? A: Probably yes!
  // |Error| <= 0.5 ulp
  // There is a different version with much larger error in TJ Dekker,
  // Numer Math, 1971, and another version in Hida, Li and Bailey's
  // "QD" library with apparent error of 2.5 ULPs.
  Xp_DoubleDouble result;
  XP_DDFLOAT_TYPE a0,a1,b0,b1,bsum;
  Xp_DoubleDouble::TwoSum(x.a0,y.a0,a0,b0);
  Xp_DoubleDouble::TwoSum(x.a1,y.a1,a1,b1);
  Xp_DoubleDouble::TwoSum(a1,b0,a1,b0);
  Xp_DoubleDouble::OrderedTwoSum(a0,a1);
  XP_DDFLOAT_TYPE tst = a1*(1+XP_DDFLOAT_EPSILON); // Part A of the
                                             /// 1 + xi + xi^2 hack
  bsum = b0 + b1; // Order must be (b0+b1)+a1

#if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) {
    if(!Xp_IsFinite(x.a0 + y.a0)) {
      // Initial TwoSum at start of function overflowed.  At this point
      // result.a0 may be a NaN, so reset to first sum.
      result.a0 = result.a1 = x.a0 + y.a0;
    } else {
      // Overflow happened after first TwoSum; result.a0 should hold
      // correct value.
      result.a1 = result.a0 = a0;
    }
    return result;
  } else if(y.a0 == 0.0) {
    // Signed zero handling.
    return x;
  }
#endif

  // Workaround hack so 1 + xi + xi^2 gives 1 + xi + xi^2
  // instead of 1 + xi (where xi = ULP(1)/2).  Also handles
  // case 1 - xi/2 - xi^2/2.  See NOTES VII, 11-Sep-2013, pp 2-4.
  XP_DDFLOAT_TYPE chk = a0 + tst;
  if(chk != a0) {
    // Either |a1| = ULP(a0)/2, or else |a0|=2^n for some n.
    tst -= a1;
    if (tst == a1*XP_DDFLOAT_EPSILON) {
      // |a1| = 2^m for some m.
      // The OrderedTwoSum is needed to handle case where bsum-a1==a1,
      // i.e., |bsum|<<|a1|.
      a0 += 2*a1;
      Xp_DoubleDouble::OrderedTwoSum(a0,bsum-a1,result.a0,result.a1);
      return result;
    }
  }
  // An OrderedTwoSum is needed to handle cases like
  //
  //   [ 0x10000000000001xb-052,-0x1FFFFFFFFFFFFFxb-106]
  // + [-0x10000000000000xb-052,-0x10000000000000xb-105]
  //
  // and
  //
  //   [ 0x10000000000001xb+054,-0x10000000000001xb+000]
  // + [-0x10000000000000xb+054,-0x10000000000000xb+000]
  //
  // Otherwise result.a0 = a0; result.a1 = a1 + bsum; would suffice.
  Xp_DoubleDouble::OrderedTwoSum(a0,a1+bsum,result.a0,result.a1);
  return result;
}
#endif // 0

Xp_DoubleDouble operator-
(const Xp_DoubleDouble& x,
 const Xp_DoubleDouble& y)
{ // Subtracts: (x.a0,x.a1) - (y.a0,y.a1)
  // |Error| <= 0.5 ulp
  // This is a duplicate of operator+(), but with sign change on y.
  // Alternative implementation is:
  //
  //    inline Xp_DoubleDouble operator-
  //    (const Xp_DoubleDouble& x,const Xp_DoubleDouble& y)
  //    { return x + (-y); }
  //
  // But having a separate implementation is measured in practice
  // to reduce compute time by about 10%
  Xp_DoubleDouble result;
  XP_DDFLOAT_TYPE a0,a1,b0,b1,bsum;
  Xp_DoubleDouble::TwoDiff(x.a0,y.a0,a0,b0);
  Xp_DoubleDouble::TwoDiff(x.a1,y.a1,a1,b1);
  Xp_DoubleDouble::TwoSum(a1,b0,a1,b0);
  Xp_DoubleDouble::OrderedTwoSum(a0,a1);
  XP_DDFLOAT_TYPE tst = a1*(1+XP_DDFLOAT_EPSILON); // Part A of the
                                             /// 1 + xi + xi^2 hack
  bsum = b0 + b1; // Seems it is important to sum these together first.

#if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) {
    if(!Xp_IsFinite(x.a0 - y.a0)) {
      // Initial TwoSum at start of function overflowed.  At this point
      // result.a0 may be a NaN, so reset to first difference.
      result.a0 = result.a1 = x.a0 - y.a0;
    } else {
      // Overflow happened after first TwoSum; result.a0 should hold
      // correct value.
      result.a1 = result.a0 = a0;
    }
    return result;
  } else if(y.a0 == 0.0) {
    // Signed zero handling.
    return x;
  }
#endif

  // Workaround hack so 1 + xi + xi^2 gives 1 + xi + xi^2
  // instead of 1 + xi (where xi = ULP(1)/2).
  XP_DDFLOAT_TYPE chk = a0 + tst;
  if(chk != a0) {
    // Either |a1| = ULP(a0)/2, or else |a0|=2^n for some n.
    tst -= a1;
    if (tst == a1*XP_DDFLOAT_EPSILON) {
      // |a1| = 2^m for some m.
      // The OrderedTwoSum is needed to handle case where bsum-a1==a1,
      // i.e., |bsum|<<|a1|.
      a0 += 2*a1;
      Xp_DoubleDouble::OrderedTwoSum(a0,bsum-a1,result.a0,result.a1);
      return result;
    }
  }
  Xp_DoubleDouble::OrderedTwoSum(a0,a1+bsum,result.a0,result.a1);
  return result;
}

XP_DDFLOAT_TYPE
ValueSafeDivide(const XP_DDFLOAT_TYPE& a,const XP_DDFLOAT_TYPE& b)
{ // Protects signed zeros, Infs and NaNs
  XP_DDFLOAT_TYPE result = a/b;
  return result;
}

// Support code for signed zero handling.  If afactor and bfactor have
// the same signs, then +0.0 is returned.  If afactor and bfactor have
// different signs, then -0.0 is returned.  This routine is used in
// the Xp_DoubleDouble multiplication and division routines.  It is in
// a VALUE_SAFE block so the -0.0 value should retain its sign.
//
XP_DDFLOAT_TYPE Xp_DoubleDouble::GetSignedZero
(XP_DDFLOAT_TYPE afactor,XP_DDFLOAT_TYPE bfactor)
{
  if(Xp_SignBit(afactor) == Xp_SignBit(bfactor)) {
    return 0.0;
  }
  return -0.0;
}

#if 0
// Version of operator+() from TJ Dekker, Numer Math, 1971
// NOTE 1: Probably needs protection from compiler optimization
// NOTE 2: Error appears to be at least 2048 ULPs; example:
//   [-0x1.60C1C86888175p+0005,-0x1.3F59800388900p-0050]
//   + [ 0x1.60E769C9B619Ap+0005,-0x1.E88373063ED75p-0049
// gives
//   [ 0x1.2D0B0970122F0p-0006,-0x1.81984018FA000p-0060]
// but should be
//   [ 0x1.2D0B0970122F0p-0006,-0x1.81984018FA800p-0060]
Xp_DoubleDouble operator+
(const Xp_DoubleDouble& xdd,
 const Xp_DoubleDouble& ydd)
{
  Xp_DoubleDouble zdd;
  const XP_DDFLOAT_TYPE& x  = xdd.a0;
  const XP_DDFLOAT_TYPE& xx = xdd.a1;
  const XP_DDFLOAT_TYPE& y  = ydd.a0;
  const XP_DDFLOAT_TYPE& yy = ydd.a1;
  XP_DDFLOAT_TYPE& z  = zdd.a0;
  XP_DDFLOAT_TYPE& zz = zdd.a1;
  XP_DDFLOAT_TYPE r,s;

  r = x + y;
  if(fabs(x) > fabs(y)) {
    s = x - r;
    s += y;
    s += yy;
    s += xx;
  } else {
    s = y - r;
    s += x;
    s += xx;
    s += yy;
  }
  z = r + s;
  zz = r - z;
  zz += s;

#if XP_RANGE_CHECK
  if(!Xp_IsFinite(z)) {
    if(!Xp_IsFinite(r)) {
      // Initial sum overflowed.  At this point result.a0 may be a NaN,
      // so reset everything to first sum.
      z = zz = r;
    } else {
      if(r>0) z = zz =  XP_INFINITY;
      else    z = zz = -XP_INFINITY;
    }
  }
#endif
  return zdd;
}
#endif
#if 0
////////////////////////////////////////////////////////////////////////
// Version or operator+() based on Hida, Li and Bailey's "QD" library.
// Error is at least 2.5 ULP, for example:
//   [-0x1.16A79801291B7p-0002,-0x1.7C2601C5A2ED8p-0056]
//   + [ 0x1.D9CA132DA208Bp-0004,-0x1.C65C3AF9554CBp-0058]
// gives
//   [-0x1.406A266B81329p-0003,-0x1.DB7A2107F0818p-0057]
// but the correctly rounded result (with 0,5 ULP error) is
//   [-0x1.406a266b81329p-0003,-0x1.db7a2107f0816p-0057]
////////////////////////////////////////////////////////////////////////
Xp_DoubleDouble operator+
(const Xp_DoubleDouble& x,
 const Xp_DoubleDouble& y)
{ // Adds (x.a0,x.a1) to (y.a0,y.a1)
  // Q: Do x and y need to be normalized? A: Probably yes!
  // There is a different version in TJ Dekker, Numer Math, 1971
  // |Error| <= 0.5 ulp
  Xp_DoubleDouble result;
  XP_DDFLOAT_TYPE a0,a1,b0,b1;

  Xp_DoubleDouble::TwoSum(x.a0,y.a0,a0,b0);
  Xp_DoubleDouble::TwoSum(x.a1,y.a1,a1,b1);
  b0 += a1;
  Xp_DoubleDouble::OrderedTwoSum(a0,b0,a0,b0);
  b0 += b1;
  Xp_DoubleDouble::OrderedTwoSum(a0,b0,result.a0,result.a1);

#if XP_RANGE_CHECK
  if(!Xp_IsFinite(result.a0)) {
    if(!Xp_IsFinite(x.a0 + y.a0)) {
      // Initial TwoSum at start of function overflowed.  At this point
      // result.a0 may be a NaN, so reset to first sum.
      result.a0 = result.a1 = x.a0 + y.a0;
    } else {
      // Overflow happened after first TwoSum; result.a0 should hold
      // correct value.
      result.a1 = result.a0;
    }
    return result;
  }
#endif

  return result;
}
#endif

////////////////////////////////////////////////////////////////////////
/// End value safe block
////////////////////////////////////////////////////////////////////////

int Xp_DoubleDouble::IsNormalized() const
{
  Xp_DoubleDouble test;
  if(!Xp_IsFinite(a0)) return 1;
  TwoSum(a0,a1,test.a0,test.a1);
  if(a0 == test.a0 && a1 == test.a1) return 1;
  return 0;
}

// Constructors
Xp_DoubleDouble::Xp_DoubleDouble(float fx)
  : a0(static_cast<XP_DDFLOAT_TYPE>(fx)), a1(0)
{
  XP_DDFLOAT_TYPE x = static_cast<XP_DDFLOAT_TYPE>(fx);
  if(!Xp_IsFinite(x) || Xp_IsZero(x)) a1 = x;
  /// Zero check propagates signed zero
}

Xp_DoubleDouble::Xp_DoubleDouble(float fxhi,float fxlo)
{
  XP_DDFLOAT_TYPE xhi = static_cast<XP_DDFLOAT_TYPE>(fxhi);
  XP_DDFLOAT_TYPE xlo = static_cast<XP_DDFLOAT_TYPE>(fxlo);
  if(!Xp_IsFinite(xhi) || (Xp_IsZero(xhi) && Xp_IsZero(xlo))) {
    // Zero check propagates signed zero
    a0 = a1 = xhi;
    return;
  }
  TwoSum(xhi,xlo,a0,a1);
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) {
    a1 = a0;
    return;
  }
#endif
}

Xp_DoubleDouble::Xp_DoubleDouble(double dx)
  : a0(static_cast<XP_DDFLOAT_TYPE>(dx)), a1(0)
{
  XP_DDFLOAT_TYPE x = static_cast<XP_DDFLOAT_TYPE>(dx);
  if(!Xp_IsFinite(x) || Xp_IsZero(x)) a1 = x;
  /// Zero check propagates signed zero
}

Xp_DoubleDouble::Xp_DoubleDouble(double dxhi,double dxlo)
{
  XP_DDFLOAT_TYPE xhi = static_cast<XP_DDFLOAT_TYPE>(dxhi);
  XP_DDFLOAT_TYPE xlo = static_cast<XP_DDFLOAT_TYPE>(dxlo);
  if(!Xp_IsFinite(xhi) || (Xp_IsZero(xhi) && Xp_IsZero(xlo))) {
    // Zero check propagates signed zero
    a0 = a1 = xhi;
    return;
  }
  TwoSum(xhi,xlo,a0,a1);
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) {
    a1 = a0;
    return;
  }
#endif
}

Xp_DoubleDouble::Xp_DoubleDouble(long double lx)
{
  XP_DDFLOAT_TYPE xhi = static_cast<XP_DDFLOAT_TYPE>(lx);
  XP_DDFLOAT_TYPE xlo
    = static_cast<XP_DDFLOAT_TYPE>(lx - static_cast<long double>(xhi));
# if XP_RANGE_CHECK
  if(!Xp_IsFinite(xhi) || Xp_IsZero(xhi)) {
    a0 = a1 = xhi;
    /// Zero check propagates signed zero
    return;
  }
# endif
  TwoSum(xhi,xlo,a0,a1);
# if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) {
    a1 = a0;
    return;
  }
# endif
}

Xp_DoubleDouble::Xp_DoubleDouble(long double lxhi, long double lxlo)
{
  Xp_DoubleDouble a(lxhi);
  Xp_DoubleDouble b(lxlo);
# if XP_RANGE_CHECK
  if(Xp_IsZero(a.a0) && Xp_IsZero(b.a0)) {
    // Propagate zero sign
    a0 = a1 = a.a0 + b.a0;
    return;
  }
# endif
  *this = a + b;
}

Xp_DoubleDouble::Xp_DoubleDouble(OC_INT4 ix)
  : a0(ix), a1(0)
{
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) a1 = a0;
#endif
}

Xp_DoubleDouble::Xp_DoubleDouble(OC_UINT4 ix)
  : a0(ix), a1(0)
{
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) a1 = a0;
#endif
}

Xp_DoubleDouble::Xp_DoubleDouble(OC_INT8 lx)
{
  XP_DDFLOAT_TYPE xhi = static_cast<XP_DDFLOAT_TYPE>(lx);
  XP_DDFLOAT_TYPE xlo
    = static_cast<XP_DDFLOAT_TYPE>(lx - static_cast<OC_INT8>(xhi));
  TwoSum(xhi,xlo,a0,a1);
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) {
    a1 = a0;
    return;
  }
#endif
}

Xp_DoubleDouble::Xp_DoubleDouble(OC_UINT8 lx)
{
  XP_DDFLOAT_TYPE xhi = static_cast<XP_DDFLOAT_TYPE>(lx);
  XP_DDFLOAT_TYPE xlo
    = static_cast<XP_DDFLOAT_TYPE>(lx - static_cast<OC_UINT8>(xhi));
  TwoSum(xhi,xlo,a0,a1);
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) {
    a1 = a0;
    return;
  }
#endif
}


#if XP_RANGE_CHECK
// Forward declaration; the code immediately proceeds operator*() below.
int XpMultiplicationRescale
(XP_DDFLOAT_TYPE& a0,XP_DDFLOAT_TYPE& a1,
 XP_DDFLOAT_TYPE& b0,XP_DDFLOAT_TYPE& b1,
 XP_DDFLOAT_TYPE& rescale);
#endif // XP_RANGE_CHECK


Xp_DoubleDouble& Xp_DoubleDouble::Square()
{
  XP_DDFLOAT_TYPE u0,u1,u2,t12;
  SquareProd(a0,u0,u1);
  // Note: Unlike the operator* case, if u0 is finite
  // then u1 must also be finite (i.e., not a NaN).
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(u0) || fabs(u0)<XP_DD_TINY) {

    if(Xp_IsZero(a0)) {
      // Import is +0.0 or -0.0.  Either way the result is +0.0.
      a0 = a1 = 0.0;
      return *this;
    }

    // Out-of-range problems.  Do brute-force rescaling.
    XP_DDFLOAT_TYPE zb0 = a0;
    XP_DDFLOAT_TYPE zb1 = a1;
    XP_DDFLOAT_TYPE rescale;
    if(XpMultiplicationRescale(a0,a1,zb0,zb1,rescale)!=0) {
      // Over or underflow detected
      a0 = a1 = rescale;
      return *this;
    }
    // Otherwise, rescaling was successful.  Make recursive call
    // and then unscale result.
    Square();
    Rescale(a0,a1,rescale,a0,a1);
    return *this;
  }
#endif // XP_RANGE_CHECK
  TwoProd(2*a0,a1,t12,u2);
  TwoSum(u1,t12,u1,t12); u2 += t12;
  u2 += a1*a1;
  OrderedTwoSum(u0,u1,a0,a1);
  a1 += u2;
  return *this;
}

void Xp_DoubleDouble::ThreeSum
(XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
 XP_DDFLOAT_TYPE b0,XP_DDFLOAT_TYPE b1,XP_DDFLOAT_TYPE b2,
 XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2)
{ // Adds (a0,a1,a2) to (b0,b1,b2), storing the result in
  // (c0,c1,c2). Error is <= 0.5 ULP.
  TwoSum(a0,b0,a0,b0);
  TwoSum(a1,b1,a1,b1);
  TwoSum(a2,b2,a2,b2);

#if XP_RANGE_CHECK
  const XP_DDFLOAT_TYPE save_sum = a0;
#endif

  TwoSum(a1,b0,a1,b0);
  TwoSum(a2,b1,a2,b1);  b2 += b1;

  TwoSum(a0,a1,a0,a1);

  TwoSum(a2,b0,a2,b0);  b2 += b0;
  TwoSum(a1,a2,a1,a2);  a2 += b2;

  OrderedTwoSum(a0,a1,c0,a1);

#if XP_RANGE_CHECK
  if(!Xp_IsFinite(c0)) {
    // Assume clients are courteous enough that we don't have
    // to worry about imports being NaNs.
    if(save_sum>0) c0 = c1 = c2 =  XP_INFINITY;
    else           c0 = c1 = c2 = -XP_INFINITY;
    return;
  }
#endif // XP_RANGE_CHECK

  OrderedTwoSum(a1,a2,c1,c2);
}

void Xp_DoubleDouble::ThreeIncrement
(XP_DDFLOAT_TYPE& a0,XP_DDFLOAT_TYPE& a1,XP_DDFLOAT_TYPE& a2,
 XP_DDFLOAT_TYPE b0)
{ // a += b0;  Error is <= 0.5 ULP.
#if 1
  TwoSum(a0,b0,a0,b0);
  TwoSum(a1,b0,a1,b0);
  TwoSum(a0,a1,a0,a1);
  TwoSum(a2,b0,a2,b0);
  TwoSum(a1,a2,a1,a2);
  a2 += b0;
#else
  // This variant is about four time faster,
  // but max error appears to be 4 ULP
  TwoSum(a0,b0,a0,b0);
  TwoSum(a1,b0,a1,b0);
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) {
    a2 = a1 = a0;
    return;
  }
#endif // XP_RANGE_CHECK
  a2 += b0;
#endif
}

void Xp_DoubleDouble::SloppyProd
(XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
 XP_DDFLOAT_TYPE b0,XP_DDFLOAT_TYPE b1,XP_DDFLOAT_TYPE b2,
 XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2)
{ // Multiplies (a0,a1,a2) by (b0,b1,b2) and stores the result in
  // (c0,c1,c2).  Code assumes that the ai's are if not completely
  // normalized then at least close, with |a1/a0| and |a2/a1| both
  // smaller than about 2^MANTISSA_PRECISION, and similarly for bi's.
  // Error appears to be smaller than 20 ULPs.
  XP_DDFLOAT_TYPE t0,t1,t2,u1,u2,v1,v2,w2;

  TwoProd(a0,b0,t0,t1);
  TwoProd(a0,b1,u1,u2);
  TwoProd(a1,b0,v1,v2);

#if XP_RANGE_CHECK
  if(!Xp_IsFinite(t0)) {
#if XP_DEKKER_MULTIPLY
    // In Dekker version of TwoProd, return value t0 may be a NaN.
    // Replace this with the value that would be returned by the
    // non-Dekker TwoProd.
    t0 = a0*b0;
#endif
    c0 = c1 = c2 = t0;
    return;
  }
#endif // XP_RANGE_CHECK
  // Note: As in the operator* routine, there is potential here
  // for t1 to be NaN.  We don't check for that here, because
  // SloppyProd is a) "sloppy", and b) intended only for internal
  // use, so we can expect clients to be courteous.

  u2 += v2;
  TwoSum(u1,v1,u1,w2);
  u2 += w2;
  TwoSum(t1,u1,t1,t2);
  t2 += u2;

  OrderedTwoSum(t0,t1,c0,t1);

  // Note: Putting
  //  OrderedTwoSum(t1,t2,t1,t2);
  // at this point can reduce error by several ULPs, but slows
  // by about 10%.

  t2 += a0*b2 + a1*b1 + a2*b0;

  TwoSum(t1,t2,c1,c2);
  // The above sum needs to be a TwoSum and not an OrderedTwoSum.
  // Test cases:
  //  [-0x1.E8EC8A4AEACC4p-004, 0x1.041713F11440Ep-061, 0x1.9F000D5CC9860p-116]
  // *[ 0x1.921fb54442d18p+000, 0x1.1a62633145c07p-054,-0x1.f1976b7ed8fbcp-110]
  //
  //  [-0x1.45F306DC9C883p-002, 0x1.6B01EC5417056p-056, 0x1.A528EF11BF762p-110]
  // *[ 0x1.921fb54442d18p+000, 0x1.1a62633145c07p-054,-0x1.f1976b7ed8fbcp-110]
  //
  // and neighbors.
}

void Xp_DoubleDouble::SloppySquare
(XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
 XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2)
{ // Multiplies (a0,a1,a2) by itself and stores the result in
  // (c0,c1,c2).  Code assumes that the ai's are if not completely
  // normalized then at least close, with |a1/a0| and |a2/a1| both
  // smaller than about 2^MANTISSA_PRECISION.  Error appears to
  // be smaller than 12 ULPs.
  XP_DDFLOAT_TYPE t0,t1,t2,u1,u2;
  SquareProd(a0,t0,t1);
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(t0)) {
    c0 = c1 = c2 = t0;
    return;
  }
#endif // XP_RANGE_CHECK
  TwoProd(2*a0,a1,u1,u2);
  TwoSum(t1,u1,t1,t2);
  t2 += u2 + a1*a1 + 2*a0*a2;
  OrderedTwoSum(t0,t1,c0,t1);
  OrderedTwoSum(t1,t2,c1,c2);
}

void Xp_DoubleDouble::SloppySqrt
(XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
 XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2)
{ // Computes sqrt(a0+a1+a2).  Error appears to be smaller than 15 ULPs.
  if(Xp_IsZero(a0)) {
    c0 = c1 = c2 = a0;
    return;
  }
  // Note: We don't handle the a0==Inf case, and moreover the code
  // below can overflow if a0 is too close to XP_DDFLOAT_MAX.  This
  // is a private function, and clients are expected to play nice.

  XP_DDFLOAT_TYPE x0, rhx0, xsq0, xsq1, d0, d1, d2, e1;
  x0 = sqrt(a0);
  rhx0 = 0.5/x0;
  SquareProd(x0,xsq0,xsq1);
  d0 = a0 - xsq0;
  TwoSum(a1,-xsq1,d1,d2);
  OrderedTwoSum(d0,d1,d0,d1);
  e1 = rhx0 * d0; // One Newton step gives x0 + e1;
  d2 += a2;
  // Do second Newton step
  XP_DDFLOAT_TYPE t0,t1;
  TwoProd(-2*x0,e1,t0,t1);
  t0 += d0;
  d2 -= e1*e1;
  t0 += t1 + d1 + d2;
  OrderedTwoSum(x0,e1,c0,e1);
  t0 *= rhx0;
  TwoSum(e1,t0,c1,c2);
}

void Xp_DoubleDouble::SloppyRecip
(XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
 XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2)
{ // Computes 1.0(a0+a1+a2).
  if(!Xp_IsFinite(a0)) {
    if(Xp_IsPosInf(a0)) {
      c0 = c1 = c2 = 0.0;
    } else if(Xp_IsNegInf(a0)) {
      c0 = c1 = c2 = -0.0;
    } else {
      c0 = c1 = c2 = XP_NAN;
    }
    return;
  }
  if(Xp_IsZero(a0)) {
    if(Xp_SignBit(a0)) {
      c0 = c1 = c2 = -XP_INFINITY;
    } else {
      c0 = c1 = c2 = XP_INFINITY;
    }
    return;
  }

  XP_DDFLOAT_TYPE b0,b1,b2;

  b0 = 1.0/a0; // First estimate

  // First Newton step correction
  Xp_DoubleDouble tmp;
  tmp.a0=a0; tmp.a1=a1;
  tmp *= b0;
  tmp = Xp_DoubleDouble(1.0) - tmp;
  tmp *= b0;
  tmp += Xp_DoubleDouble(b0);
  b0 = tmp.a0;  b1 = tmp.a1;  b2 = 0.0; // Second estimate

  // Second Newton step correction
  XP_DDFLOAT_TYPE t0,t1,t2;
  SloppyProd(a0,a1,a2,b0,b1,b2,t0,t1,t2);
  ThreeIncrement(t0,t1,t2,-1.0);
  SloppyProd(t0,t1,t2,b0,b1,b2,t0,t1,t2);
  ThreeSum(b0,b1,b2,-t0,-t1,-t2,b0,b1,b2);
  Normalize(b0,b1,b2,c0,c1,c2);
}

void Xp_DoubleDouble::Floor
(XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
 XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2)
{ // Compare to floor(const Xp_DoubleDouble&)
  XP_DDFLOAT_TYPE b0 = XP_FLOOR(a0);  XP_DDFLOAT_TYPE b0r = a0 - b0;
  XP_DDFLOAT_TYPE b1 = XP_FLOOR(a1);  XP_DDFLOAT_TYPE b1r = a1 - b1;
  XP_DDFLOAT_TYPE b2 = XP_FLOOR(a2);  XP_DDFLOAT_TYPE b2r = a2 - b2;
  // We need to normalize both pieces.  Salient example:
  // 1.411905e+00 + -6.604641e-17 + -4.687262e-33
  Normalize(b0,b1,b2,b0,b1,b2);
  Normalize(b0r,b1r,b2r,b0r,b1r,b2r);
  ThreeIncrement(b0,b1,b2,XP_FLOOR(b0r));
  c0=b0; c1=b1; c2=b2;
  return;
}

void Xp_DoubleDouble::Normalize
(XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
 XP_DDFLOAT_TYPE& b0,XP_DDFLOAT_TYPE& b1,XP_DDFLOAT_TYPE& b2)
{
  Xp_DoubleDouble::TwoSum(a1,a2,b1,b2);
  Xp_DoubleDouble::TwoSum(a0,b1,b0,b1);
  Xp_DoubleDouble::TwoSum(b1,b2,b1,b2);
}


void Xp_DoubleDouble::LdExp10
(XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
 int m,
 XP_DDFLOAT_TYPE& b0,XP_DDFLOAT_TYPE& b1,XP_DDFLOAT_TYPE& b2)
{ // Computes a*10^m, with more that double-double but somewhat less that
  // triple-double accuracy.  This code is similar in structure and
  // justification to the XpLdExp routine in xpcommon.h (q.v.), but for
  // multiplication by power-of-ten rather than power-of-two.  One notable
  // difference is that while the mantissa width necessary to represent
  // 2^m is one bit regardless of m, 10^m requires a mantissa of
  // 1+floor(log_2(5^m)) bits.  (10=2*5, and 5^m is odd.)
  if(m==0) {
    b0=a0; b1=a1; b2=a2;
    return;
  }
  unsigned int n = (m>0 ? m : -m);

  // Stop values for single and double XP_DDFLOAT operations arising
  // from mantissa limitations.
  static const unsigned int mant1_count = static_cast<unsigned int>
    (floor(log(XP_DDFLOAT_MANTISSA_PRECISION*log(2)/log(5))/log(2)));
  static const unsigned int mant2_count = static_cast<unsigned int>
    (floor(log((2*XP_DDFLOAT_MANTISSA_PRECISION+1)*log(2)/log(5))/log(2)));

  // The nstop value below is set so that xpow, which may be as large as
  // ddbase^2/10, doesn't overflow.
  static const unsigned int max_square_count = static_cast<unsigned int>
    (floor(log(XP_DDFLOAT_HUGE_EXP*log(2)/log(10))/log(2)));

  assert(mant1_count<mant2_count && mant2_count<max_square_count);

  const unsigned int nmant1stop = n >> mant1_count;
  const unsigned int nmant2stop = n >> mant2_count;
  const unsigned int nrangestop = n >> max_square_count;

  Xp_DoubleDouble base,xpow;
  XP_DDFLOAT_TYPE& base0 = base.a0;
  XP_DDFLOAT_TYPE& base1 = base.a1;
  XP_DDFLOAT_TYPE base2;

  XP_DDFLOAT_TYPE& xpow0 = xpow.a0;
  XP_DDFLOAT_TYPE& xpow1 = xpow.a1;
  XP_DDFLOAT_TYPE xpow2;

  base0=10.0; base1=0.0; base2=0.0;
  xpow0 = (n & 1 ? base0 : 1);
  xpow1=0.0;   xpow2=0.0;

  // Base case: exponentiation by squaring
  while( (n >>= 1u) > nmant1stop) {
    // All bits fit in XP_DDFLOAT_TYPE
    base0 *= base0;
    if(n & 1u) {
      xpow0 *= base0;
    }
  }

  while( 0 && n > nmant2stop) {
    // All bits fit in Xp_DoubleDouble
    base.Square();
    if(n & 1u) {
      xpow *= base;
    }
    n >>= 1u;
  }

  while( n > nrangestop) {
    // Use triple-double for maximum precision
    SloppySquare(base0,base1,base2,base0,base1,base2);
    if(n & 1u) {
      SloppyProd(xpow0,xpow1,xpow2,base0,base1,base2,
                 xpow0,xpow1,xpow2);
    }
    n >>= 1u;
  }

  if(m>0) {
    SloppyProd(a0,a1,a2,xpow0,xpow1,xpow2,
               a0,a1,a2);
    if(n>0u) {
      // Leftover bit in case where xpow would overflow
      do {
        SloppyProd(a0,a1,a2,base0,base1,base2,
                   a0,a1,a2);
      } while(n-- > 0u);
    }
  } else { // m<0
    SloppyRecip(xpow0,xpow1,xpow2,
                xpow0,xpow1,xpow2);
    SloppyProd(a0,a1,a2,xpow0,xpow1,xpow2,
               a0,a1,a2);
    if(n>0u) {
      // Leftover bit in case where xpow would overflow
      SloppyRecip(base0,base1,base2,
                  base0,base1,base2);
      do {
        SloppyProd(a0,a1,a2,base0,base1,base2,
                   a0,a1,a2);
      } while(n-- > 0u);
    }
  }

  b0=a0; b1=a1; b2=a2;
  return;
}


#if XP_RANGE_CHECK
int XpMultiplicationRescale
(XP_DDFLOAT_TYPE& a0,XP_DDFLOAT_TYPE& a1,
 XP_DDFLOAT_TYPE& b0,XP_DDFLOAT_TYPE& b1,
 XP_DDFLOAT_TYPE& rescale)
{ // This is a support routine to do rescaling for
  // operator*(Xp_DoubleDouble&,Xp_DoubleDouble&).  This is not a
  // critical path, and moving it out of the function proper improves
  // speed on some platforms.
  //
  // If the return value is 0, then the call was successful:
  // imports/exports a0, a1, b0 and b1 are rescaled and the export
  // rescale is set to the new scaling.  (rescale = 1/(ascale*bscale),
  // where ascale is the scaling on a0+a1, and bscale is the scaling
  // on b0+b1.)
  //
  // If the return value is non-zero, then an over- or underflow
  // condition has been detected.  In this case rescale is set to the
  // appropriate signed infinity or zero value; the other import/export
  // variables may be changed.
  //
  // Method description: Brute-force scaling to solve out-of-range
  // problems in operator*().  Pull out and handle exponents
  // separately.  This protects against various overflow and underflow
  // problems.
  if(!Xp_IsFinite(a0) || !Xp_IsFinite(b0)) {
    // NB: XP_FREXP returns values with magnitude in [0.5,1) IF the
    // import is finite.  If the import is infinite or NaN, then on at
    // least some systems XP_FREXP returns the import value and sets
    // the export to 0.  Therefore we test for non-finite inputs and
    // and use the system double-precision arithmetic to select the
    // proper return value.
    rescale = a0*b0;
    return 1;
  }

  int a0_exp,b0_exp,expsum;
  a0 = 2*XP_FREXP(a0,&a0_exp); --a0_exp; // 1.0 <= a0 < 2.0
  b0 = 2*XP_FREXP(b0,&b0_exp); --b0_exp; // 1.0 <= b0 < 2.0
  a1 = Xp_DoubleDouble::LdExp(a1,-a0_exp);
  b1 = Xp_DoubleDouble::LdExp(b1,-b0_exp);
  expsum = a0_exp + b0_exp;
  // 1.0 <  a0/b0 < 4.0

  if(expsum > XP_DDFLOAT_HUGE_EXP-1) {
    // Overflow
    if(expsum > XP_DDFLOAT_HUGE_EXP) {
      // Definite overflow, regardless of a1 and b1 values
      if(a0*b0 > 0.0) rescale =  XP_INFINITY;
      else            rescale = -XP_INFINITY;
      return 1;
    }
    // Otherwise expsum == XP_DDFLOAT_HUGE_EXP, and whether the product
    // is finite or not depends on a1 and b1.  Shift scaling and let the
    // client code figure out which is which.
    // Test case:
    //  In VCV precision-independent notation: (1pB1d(h)1i1)^2
    //  8-byte doubles: [0x1.fffffffffffffp+1023,0x1.4000000000000p+0918]^2
    expsum -= 2; // Rescale a and b identically so that this routine can be
    a0 *= 2.0;  a1 *= 2.0; // used by Square() as well as operator*()
    b0 *= 2.0;  b1 *= 2.0;
  } else if(expsum < XP_DDFLOAT_VERYTINY_EXP) {
    if(expsum < XP_DDFLOAT_VERYTINY_EXP-3) {
      // Underflow to (signed) zero
      if(a0*b0 > 0.0) rescale =  0.0;
      else            rescale = -0.0;
      return -1;
    }
    // Otherwise the product may be non-zero.  Rescale expsum
    // so 2.0^expsum doesn't underflow.  Here 0.0625 = 2^-4.
    // As before, rescale a and b identically for use by Square()
    expsum += 8;
    a0 *= 0.0625;   a1 *= 0.0625;
    b0 *= 0.0625;   b1 *= 0.0625;
  }
  rescale = Xp_DoubleDouble::LdExp(XP_DDFLOAT_TYPE(1.0),expsum);
  /// NB: The tests above insure pow(2.0,expsum) doesn't over- or underflow.

  return 0;  // Rescaling successful
}
#endif // XP_RANGE_CHECK

Xp_DoubleDouble operator*
(const Xp_DoubleDouble& x,
 const Xp_DoubleDouble& y)
{ // |Error| <= 0.5 ulp
  XP_DDFLOAT_TYPE u0,u1,u2,s1,s2,t1,t2;

  const XP_DDFLOAT_TYPE& a0 = x.a0;
  const XP_DDFLOAT_TYPE& a1 = x.a1;
  const XP_DDFLOAT_TYPE& b0 = y.a0;
  const XP_DDFLOAT_TYPE& b1 = y.a1;

  Xp_DoubleDouble::TwoProd(a0,b0,u0,u1);
  Xp_DoubleDouble::TwoProd(a0,b1,s1,s2);

#if XP_RANGE_CHECK
  if(fabs(u0)<XP_DD_TINY || !Xp_IsFinite(u1)) {
    Xp_DoubleDouble result;
    if(Xp_IsFinite(u1) && Xp_IsZero(u0)) {
      result.a0 = result.a1 = Xp_DoubleDouble::GetSignedZero(x.a0,y.a0);
      return result;
    }

    // Out-of-range problems.  Do brute-force rescaling.
    Xp_DoubleDouble rx, ry;
    rx.a0 = x.a0;  rx.a1 = x.a1;
    ry.a0 = y.a0;  ry.a1 = y.a1;
    XP_DDFLOAT_TYPE rescale;
    if(XpMultiplicationRescale(rx.a0,rx.a1,ry.a0,ry.a1,rescale)!=0) {
      // Over or underflow detected
      if(Xp_IsZero(rescale)) {
        rescale = Xp_DoubleDouble::GetSignedZero(x.a0,y.a0);
      }
      result.a0 = result.a1 = rescale;
      return result;
    }
    // Otherwise, rescaling was successful.  Make recursive call
    // and then unscale result.
    result = rx * ry;
    Xp_DoubleDouble::Rescale(result.a0,result.a1,rescale,result.a0,result.a1);
    if(Xp_IsZero(result.a0)) {
      result.a0 = result.a1 = Xp_DoubleDouble::GetSignedZero(x.a0,y.a0);
    }
    return result;
  }
#else // XP_RANGE_CHECK
  if(!Xp_IsFinite(u0)) {
    Xp_DoubleDouble result;
    result.a0 = result.a1 = u0;
    return result;
  }
#endif // XP_RANGE_CHECK

  Xp_DoubleDouble::TwoSum(u1,s1,u1,s1); s2 += s1;
  Xp_DoubleDouble::TwoProd(a1,b0,t1,t2);
  u2 = a1*b1;  t2 += s2;
  Xp_DoubleDouble::TwoSum(u1,t1,u1,t1); t2 += t1;
  u2 += t2;

  Xp_DoubleDouble result;
  Xp_DoubleDouble::OrderedTwoSum(u0,u1,result.a0,result.a1);
  if(!Xp_IsFinite(result.a0)) {
    result.a1 = result.a0;
    return result;
  }
  result.a1 += u2;
  return result;
}

Xp_DoubleDouble operator*
(XP_DDFLOAT_TYPE x,
 const Xp_DoubleDouble& y)
{ // About 30% faster than (Xp_DoubleDouble&,Xp_DoubleDouble&) version
  // |Error| <= 0.5 ulp
  const XP_DDFLOAT_TYPE& a0 = y.a0;
  const XP_DDFLOAT_TYPE& a1 = y.a1;

  XP_DDFLOAT_TYPE u0,u1,u2,t1,t2;
  Xp_DoubleDouble::TwoProd(a0,x,u0,u1);
  Xp_DoubleDouble::TwoProd(a1,x,t1,u2);

#if XP_RANGE_CHECK
  if(fabs(u0)<XP_DD_TINY || !Xp_IsFinite(u1)) {
    Xp_DoubleDouble result;
    if(Xp_IsFinite(u1) && Xp_IsZero(u0)) {
      result.a0 = result.a1 = Xp_DoubleDouble::GetSignedZero(x,y.a0);
      return result;
    }

    // Out-of-range problems.  Do brute-force rescaling.
    XP_DDFLOAT_TYPE rx = x; // Make a copy to keep sign of original x
    Xp_DoubleDouble ry = y;
    XP_DDFLOAT_TYPE tb1 = 0.0;
    XP_DDFLOAT_TYPE rescale;
    if(XpMultiplicationRescale(ry.a0,ry.a1,rx,tb1,rescale)!=0) {
      // Over or underflow detected
      if(Xp_IsZero(rescale)) {
        rescale = Xp_DoubleDouble::GetSignedZero(x,y.a0);
      }
      result.a0 = result.a1 = rescale;
      return result;
    }

    // Otherwise, rescaling was successful.  Make recursive call
    // and then unscale result.
    result = rx * ry;
    Xp_DoubleDouble::Rescale(result.a0,result.a1,rescale,result.a0,result.a1);
    if(Xp_IsZero(result.a0)) {
      result.a0 = result.a1 = Xp_DoubleDouble::GetSignedZero(x,y.a0);
    }
    return result;
  }
#endif // XP_RANGE_CHECK

  Xp_DoubleDouble::TwoSum(u1,t1,u1,t2);  u2 += t2;
  Xp_DoubleDouble result;
  Xp_DoubleDouble::OrderedTwoSum(u0,u1,result.a0,result.a1);
  if(!Xp_IsFinite(result.a0)) {
    result.a1 = result.a0;
    return result;
  }
  result.a1 += u2;
  return result;
}

#if XP_RANGE_CHECK
int XpDivisionRescale
(XP_DDFLOAT_TYPE& a0,XP_DDFLOAT_TYPE& a1,
 XP_DDFLOAT_TYPE& b0,XP_DDFLOAT_TYPE& b1,
 XP_DDFLOAT_TYPE& rescale)
{ // This is a support routine to do rescaling for
  // operator/(Xp_DoubleDouble&,Xp_DoubleDouble&).  This is not a
  // critical path, and moving it out of the function proper improves
  // speed on some platforms.
  //
  // If the return value is 0, then the call was successful:
  // imports/exports a0, a1, b0 and b1 are rescaled and the export
  // rescale is set to the new scaling.  (rescale = bscale/ascale,
  // where ascale is the scaling on a0+a1, and bscale is the scaling
  // on b0+b1.)
  //
  // If the return value is non-zero, then an over- or underflow
  // condition has been detected.  In this case rescale is set to the
  // appropriate signed infinity or zero value; the other import/export
  // variables may be changed.
  //
  // Method description: Brute-force scaling to solve out-of-range
  // problems in operator/().  Pull out and handle exponents
  // separately.  This protects against various overflow and underflow
  // problems.

  if(!Xp_IsFinite(a0) || !Xp_IsFinite(b0)) {
    // NB: If a0 (resp. b0) is finite on entry, then the return value
    // from XP_FREXP has absolute value in [0.5,1).  HOWEVER, if a0 is
    // infinite, then on at least some platforms XP_FREXP returns an
    // infinity and sets the exponent export to 0.  Similarly, if a0 is
    // NaN, then XP_FREXP returns NaN and sets the exponent export to 0.
    // So, catch these cases and use the system double-precision
    // arithmetic to set the correct return value.
    rescale = a0/b0;
    return 1;
  }

  int a0_exp,b0_exp,expdiff;
  a0 = 2*XP_FREXP(a0,&a0_exp); --a0_exp; // 1.0 <= a0 < 2.0
  b0 = XP_FREXP(b0,&b0_exp);             // 0.5 <= b0 < 1.0
  a1 = Xp_DoubleDouble::LdExp(a1,-a0_exp);
  b1 = Xp_DoubleDouble::LdExp(b1,-b0_exp);
  expdiff = a0_exp - b0_exp;
  // 1.0 <  a0/b0 < 4.0

  if(expdiff > XP_DDFLOAT_HUGE_EXP-1) {
    // Overflow
    if(a0*b0 > 0.0) rescale =  XP_INFINITY;
    else            rescale = -XP_INFINITY;
    return 1;
  } else if(expdiff < XP_DDFLOAT_VERYTINY_EXP) {
    if(expdiff < XP_DDFLOAT_VERYTINY_EXP-3) {
      // Underflow to (signed) zero
      if(a0*b0 > 0.0) rescale =  0.0;
      else            rescale = -0.0;
      return -1;
    }
    // Otherwise the quotient may be non-zero.  Rescale expdiff
    // so 2.0^expdiff doesn't underflow.  Here 0.00390625 = 2^-8.
    a0 *= 0.00390625;   a1 *= 0.00390625;   expdiff += 8;
  }
  rescale = Xp_DoubleDouble::LdExp(XP_DDFLOAT_TYPE(1.0),expdiff);
  /// NB: The tests above insure 2^expdiff doesn't over- or underflow.

  return 0;  // Rescaling successful
}
#endif // XP_RANGE_CHECK

void AuxiliaryDivide
(const Xp_DoubleDouble& x,
 const Xp_DoubleDouble& y,
 Xp_DoubleDouble& q,XP_DDFLOAT_TYPE& q2)
{ // Return x/y.  Implements two Newton steps for full accuracy.
  // See NOTES VI, 19-Dec-2012, p 129.
  // |Error| <= 0.5 ulp
  // WARNING: Imports and export *must* be physically distinct!
  // NOTE: This routine is the guts for
  //       operator/(Xp_DoubleDouble&,Xp_DoubleDouble&), pulled out to
  //       enable proper underflow rounding.  The export (q,q2) is
  //       not a proper triple-double; q2 may overlap q.  q2 is held
  //       out so that q.a0+q.a1+q2 can be rounded properly in underflow
  //       condition where rescaling is needed.
  // NOTE: The return from this routine MAY NOT BE NORMALIZED in
  //       the last component.  However, (q.a0,q.a1+q2) should be a
  //       normalized double-double.
  XP_DDFLOAT_TYPE& q0 = q.a0;
  XP_DDFLOAT_TYPE& q1 = q.a1;

  const XP_DDFLOAT_TYPE& a0 = x.a0;
  const XP_DDFLOAT_TYPE& a1 = x.a1;
  const XP_DDFLOAT_TYPE& b0 = y.a0;
  const XP_DDFLOAT_TYPE& b1 = y.a1;

  XP_DDFLOAT_TYPE recip_b0,x0,d1,u1,u2,t0,t1,t2,s1,s2;
  recip_b0 = 1.0/b0;
  x0 = a0*recip_b0;
  Xp_DoubleDouble::TwoProd(x0,-b0,t0,t1);
  Xp_DoubleDouble::TwoProd(x0,-b1,s1,u2);
  u1 = a0 + t0; // a0 and -t0 should be nearly equal

#if XP_RANGE_CHECK
  if(fabs(a0)<16*XP_DD_TINY ||
     !(16*XP_DDFLOAT_MIN*XP_DDFLOAT_POW_2_MANTISSA*XP_DDFLOAT_POW_2_MANTISSA
       <fabs(x0)) || !Xp_IsFinite(t1)) {
    q2 = 0;
    // Zero checks
    if(Xp_IsZero(x.a0)) {
      if(!Xp_IsZero(y.a0) && Xp_IsFinite(y.a0)) {
        // Return signed zero
        q0 = q1 = Xp_DoubleDouble::GetSignedZero(x.a0,y.a0);
      } else {
        q0 = q1 = x.a0/y.a0; // May be NaN
      }
      return;
    }
    if(Xp_IsZero(y.a0)) {
      q0 = q1 = ValueSafeDivide(x.a0,y.a0); // Signed infinity
      return;
    }

    // Out-of-range problems.  Do brute-force rescaling.
    Xp_DoubleDouble rx = x;
    Xp_DoubleDouble ry = y;
    XP_DDFLOAT_TYPE rescale;
    if(XpDivisionRescale(rx.a0,rx.a1,ry.a0,ry.a1,rescale)!=0) {
      // Over or underflow detected
      if(Xp_IsZero(rescale)) {
        rescale = Xp_DoubleDouble::GetSignedZero(x.a0,y.a0);
      }
      q0 = q1 = rescale;
      return;
    }
    // Otherwise, rescaling was successful. Make recursive
    // call and then unscale result.
    AuxiliaryDivide(rx,ry,q,q2);
    Xp_DoubleDouble::Rescale(q0,q1,q2,rescale,q0,q1);
    q2 = 0.0;
    return; // Result is finite
  }
#endif // XP_RANGE_CHECK

  Xp_DoubleDouble::TwoSum(a1,t1,t1,t2); u2 += t2;
  Xp_DoubleDouble::TwoSum(u1,s1,u1,s2); u2 += s2;
  Xp_DoubleDouble::TwoSum(u1,t1,u1,t2); u2 += t2;
  d1 = u1*recip_b0;
  Xp_DoubleDouble::OrderedTwoSum(x0,d1,q0,q1);
  if(!Xp_IsFinite(q0)) {
    q2 = q1 = q0;
    return;
  }
  u2 -= d1*b1;

  Xp_DoubleDouble::TwoProd(d1,-b0,t1,t2);
  u1 += t1; // u1 and -t1 should be nearly equal
  u2 += t2;
  q2 = recip_b0*(u1+u2);
  Xp_DoubleDouble::Coalesce(q0,q1,q2);
}

Xp_DoubleDouble operator/
(const Xp_DoubleDouble& x,
 XP_DDFLOAT_TYPE y)
{ // Return a/b.  Implements two Newton steps for full accuracy.
  // See NOTES VI, 19-Dec-2012, p 129.
  // |Error| <= 0.5 ulp

  Xp_DoubleDouble q;
  XP_DDFLOAT_TYPE& q0 = q.a0;
  XP_DDFLOAT_TYPE& q1 = q.a1;

  const XP_DDFLOAT_TYPE& a0 = x.a0;
  const XP_DDFLOAT_TYPE& a1 = x.a1;
  const XP_DDFLOAT_TYPE& b0 = y;

  XP_DDFLOAT_TYPE recip_b0,x0,u1,u2,t0,t1,t2;
  recip_b0 = 1.0/b0;
  x0 = a0*recip_b0;
  Xp_DoubleDouble::TwoProd(x0,-b0,t0,t1);

#if XP_RANGE_CHECK
  if(fabs(a0)<16*XP_DD_TINY ||
     !(16*XP_DDFLOAT_MIN*XP_DDFLOAT_POW_2_MANTISSA*XP_DDFLOAT_POW_2_MANTISSA
       <fabs(x0)) || !Xp_IsFinite(t1)) {

    // Zero checks
    if(Xp_IsZero(x.a0)) {
      if(!Xp_IsZero(y) && Xp_IsFinite(y)) {
        // Return signed zero
        q0 = q1 = Xp_DoubleDouble::GetSignedZero(x.a0,y);
      } else {
        q0 = q1 = x.a0/y; // May be NaN
      }
      return q;
    }
    if(Xp_IsZero(y)) {
      q0 = q1 = ValueSafeDivide(x.a0,y); // Signed infinity
      return q;
    }

    // Out-of-range problems.  Do brute-force rescaling.
    Xp_DoubleDouble rx = x;
    XP_DDFLOAT_TYPE ry = y;
    XP_DDFLOAT_TYPE b1=0;
    XP_DDFLOAT_TYPE rescale;
    if(XpDivisionRescale(rx.a0,rx.a1,ry,b1,rescale)!=0) {
      // Over or underflow detected
      if(Xp_IsZero(rescale)) {
        rescale = Xp_DoubleDouble::GetSignedZero(x.a0,y);
      }
      q0 = q1 = rescale;
      return q;
    }
    // Otherwise, rescaling was successful. Make recursive call and then
    // unscale result.  Q: In the operator/(const Xp_DoubleDouble&,const
    // Xp_DoubleDouble&) version of this code, we need a triple-double
    // return from the scaled division in order for the rescaling to
    // perform properly on underflow.  This doesn't seem to be a problem
    // here?
    q = rx / ry;
    Xp_DoubleDouble::Rescale(q0,q1,rescale,q0,q1);
    if(Xp_IsZero(q0)) {
      q0 = q1 = Xp_DoubleDouble::GetSignedZero(x.a0,y);
    }
    return q;
  }
#endif // XP_RANGE_CHECK

  Xp_DoubleDouble::TwoSum(a1,t1,t1,u2);
  u1 = a0 + t0; // a0 and -t0 should be nearly equal
  Xp_DoubleDouble::TwoSum(u1,t1,u1,t2);

  XP_DDFLOAT_TYPE d1,s1,s2;
  d1 = u1*recip_b0;
  u2 += t2;

  Xp_DoubleDouble::TwoProd(d1,-b0,s1,s2);
  Xp_DoubleDouble::OrderedTwoSum(x0,d1,q0,q1);

  u1 += s1; // u1 and -s1 should be nearly equal
  u2 += s2;
  XP_DDFLOAT_TYPE q2 = recip_b0*(u1+u2);
  Xp_DoubleDouble::Coalesce(q0,q1,q2);
  q1 += q2;

  return q;
}

void AuxiliaryRecip
(const Xp_DoubleDouble& x,
 const XP_DDFLOAT_TYPE& y0, // Lead term in result
  Xp_DoubleDouble& result,
  XP_DDFLOAT_TYPE& result_correction)
// WARNING: Import x and export result *must* be physically distinct!
{ // NOTE: This routine is the guts for Recip(const Xp_DoubleDouble&),
  //       pulled out to enable proper underflow rounding.  The export
  //       is not a proper triple-double; result_correction may
  //       overlap result.a1.  result_correction is held out so that
  //       result.a1 + result_correction can be rounded properly in
  //       underflow condition where the rescaling is needed.

  result.a0 = y0;
  XP_DDFLOAT_TYPE s1,s2,u1,u2,t2;
  Xp_DoubleDouble::TwoProd(y0,x.a0,s1,s2); s1 -= 1.0;
  Xp_DoubleDouble::TwoProd(y0,x.a1,u1,u2);
  Xp_DoubleDouble::TwoSum(s1,s2,s1,s2);    u2 += s2;
  Xp_DoubleDouble::TwoSum(s1,u1,s1,u1);
  Xp_DoubleDouble::TwoProd(y0,-s1,result.a1,t2);
  u2 = y0*(u2 + u1 - s1*s1);
  result_correction = t2 - u2;
}


Xp_DoubleDouble recip(const Xp_DoubleDouble& x)
{ // Yet-another-Recip-variant.  NOTES VI, 27-Aug-2013, p199
  // |Error| <= 0.5 ulp

  XP_DDFLOAT_TYPE y0 = 1.0/x.a0;

#if XP_RANGE_CHECK
  XP_DDFLOAT_TYPE xcheck = fabs(x.a0);
  if(!(XP_DDFLOAT_MIN*XP_DDFLOAT_POW_2_MANTISSAHALF<xcheck
       && xcheck<XP_DDFLOAT_MAX
       /(16*XP_DDFLOAT_POW_2_MANTISSA*XP_DDFLOAT_POW_2_MANTISSA))) {
    // Range problems
    Xp_DoubleDouble result;
    if(!Xp_IsFinite(x.a0)) {
      if(Xp_IsPosInf(x.a0)) {
        result.a0 = 0.0;
      } else if(Xp_IsNegInf(x.a0)) {
        result.a0 = -0.0;
      } else {
        result.a0 = XP_NAN;
      }
      result.a1 = result.a0;
      return result;
    }

    if(!Xp_IsFinite(y0)) {
      // x.a0 is (possibly signed) zero
      result.a1 = result.a0 = y0;
      return result;
    }

    // Otherwise, rescale and make auxiliary call
    XP_DDFLOAT_TYPE rescale;
    if(fabs(x.a0) > 1.0) {
      rescale = 1.0/(32*XP_DDFLOAT_POW_2_MANTISSA*XP_DDFLOAT_POW_2_MANTISSA);
    } else {
      rescale = XP_DDFLOAT_POW_2_MANTISSA;
    }
    Xp_DoubleDouble atmp,rtmp;
    XP_DDFLOAT_TYPE rtmp_correction;
    atmp.a0 = x.a0 * rescale;
    atmp.a1 = x.a1 * rescale;
    y0 = 1.0/atmp.a0;
    AuxiliaryRecip(atmp,y0,rtmp,rtmp_correction);
    Xp_DoubleDouble::Rescale(rtmp.a0,rtmp.a1,rtmp_correction,rescale,
                             result.a0,result.a1);
    return result;
  }
#endif // XP_RANGE_CHECK

  Xp_DoubleDouble result;
  XP_DDFLOAT_TYPE result_correction;
  AuxiliaryRecip(x,y0,result,result_correction);
  Xp_DoubleDouble::Coalesce(result.a0,result.a1,result_correction);
  result.a1 += result_correction;

  return result;
}


Xp_DoubleDouble sqrt(const Xp_DoubleDouble& x)
{ // Compute sqrt(x) via two Newton steps: |Error| <= 0.5 ulp
  // See NOTES VI, 19-Dec-2012, pp 128-129.
  Xp_DoubleDouble result;
#if !XP_RANGE_CHECK
  if(Xp_IsZero(x.a0)) {
    result.a0 = result.a1 = x.a0; // Pass sign.
    /// Convention dictates sqrt(-0.0) = -0.0
    return result;
  }
#else // XP_RANGE_CHECK
  if(!(XP_DD_TINY<=x.a0 && x.a0<=0.5*XP_DDFLOAT_MAX)) {
    if(!Xp_IsFinite(x.a0)) {
      // Put Xp_IsFinite (q.v.) check first to catch NaNs (some
      // compiler optimizations make NaNs misbehave).
      if(Xp_IsPosInf(x.a0)) {
        result.a0 = result.a1 = XP_INFINITY;
      } else {
        result.a0 = result.a1 = XP_NAN;
      }
      return result;
    }
    if(Xp_IsZero(x.a0)) {
      result.a0 = result.a1 = x.a0; // Pass sign.
      /// Convention dictates sqrt(-0.0) = -0.0
      return result;
    }
    if(x.a0 < 0.0) {
      result.a0 = result.a1 = XP_NAN;
      return result;
    }
    if(x.a0 < XP_DD_TINY) {
      // If x is too small, underflow can lose digits.  Rescale.
      const XP_DDFLOAT_TYPE scaleup
        = 4*XP_DDFLOAT_POW_2_MANTISSA*XP_DDFLOAT_POW_2_MANTISSA;
      result.a0 = x.a0 * scaleup;   result.a1 = x.a1 * scaleup;
      result = sqrt(result); // Recursive call!
      const XP_DDFLOAT_TYPE scaledown = 0.5/XP_DDFLOAT_POW_2_MANTISSA;
      result.a0 *= scaledown;       result.a1 *= scaledown;
      return result;
    }
    if(x.a0 > 0.5*XP_DDFLOAT_MAX) {
      // If x is too near the top value, the mainline code can
      // overflow.  Rescale.
      result.a0 = x.a0 * 0.25;    result.a1 = x.a1 * 0.25;
      result = sqrt(result); // Recursive call!
      result.a0 *= 2;             result.a1 *= 2;
      return result;
    }
  }
#endif // XP_RANGE_CHECK

  XP_DDFLOAT_TYPE s1,s2,t1,t2,u1,u1sq;
  const XP_DDFLOAT_TYPE y0 = sqrt(x.a0);
  Xp_DoubleDouble::SquareProd(y0,s1,s2);
  const XP_DDFLOAT_TYPE ry0 = 0.5/y0;
  s1 = x.a0 - s1;
  Xp_DoubleDouble::TwoSum(s1,-s2,s1,s2);
  Xp_DoubleDouble::TwoSum(s1,x.a1,s1,t1); s2 += t1;
  u1 = s1*ry0;

  u1sq = u1*u1;
  Xp_DoubleDouble::TwoProd(-2*y0,u1,t1,t2);
  Xp_DoubleDouble::TwoSum(s1,t1,s1,t1);
  s2 += t2 + t1 - u1sq;
  s1 = ry0*(s1 + s2);

  Xp_DoubleDouble::OrderedTwoSum(y0,u1,result.a0,result.a1);
  result.a1 += s1;
  // Xp_DoubleDouble::OrderedTwoSum(result.a0,result.a1,
  //                                result.a0,result.a1); // Not needed?
  return result;
}

Xp_DoubleDouble recipsqrt(const Xp_DoubleDouble& x)
{ // Computes 1.0/sqrt(x) with |Error| <= 0.5 ulp
  // About 20% faster than sqrt() composed with recip()
  // The adjustment to the initial iterate is
  //    0.5*y0*D*(1 + 0.75*D + ...)
  // where x0 is the initial iterate, and D is the small difference
  //    D = 1 - A*y0*y0
  // The code below uses the first two terms in the series above.
  // (See NOTES VI,24-Nov-2012, p117.)
  Xp_DoubleDouble result;
#if !XP_RANGE_CHECK
  if(x.a0 == 0.0) {
    result.a0 = result.a1 = XP_INFINITY;
    return result;
  }
#else // XP_RANGE_CHECK
  if(!(2.0*XP_DD_TINY<=x.a0 && x.a0<=1.0/XP_DD_TINY)) {
    if(!Xp_IsFinite(x.a0)) {
      // Put Xp_IsFinite (q.v.) check first to catch NaNs (some
      // compiler optimizations make NaNs misbehave).
      if(Xp_IsPosInf(x.a0)) {
        result.a0 = result.a1 = 0.0;
      } else {
        result.a0 = result.a1 = XP_NAN;
      }
      return result;
    }
    if(Xp_IsZero(x.a0)) {
      if(Xp_SignBit(x.a0)) {
        result.a0 = result.a1 = -XP_INFINITY;
      } else {
        result.a0 = result.a1 =  XP_INFINITY;
      }
      return result;
    }
    if(x.a0 < 2.0*XP_DD_TINY) {
      if(x.a0 < 0.0) { // Invalid input
        result.a0 = result.a1 = XP_NAN;
        return result;
      }
      // If x is too small, then the code below can overflow.  Rescale.
      const XP_DDFLOAT_TYPE scale
        = 4*XP_DDFLOAT_POW_2_MANTISSA*XP_DDFLOAT_POW_2_MANTISSA;
      result.a0 = x.a0 * scale;   result.a1 = x.a1 * scale;
      result = recipsqrt(result); // Recursive call!
      const XP_DDFLOAT_TYPE unscale = 2*XP_DDFLOAT_POW_2_MANTISSA;
      result.a0 *= unscale;       result.a1 *= unscale;
      return result;
    }
    if(x.a0 > 1.0/XP_DD_TINY) {
      // If x is too big, then we can lose digits to underflow.
      const XP_DDFLOAT_TYPE scale
        = 1.0/(4*XP_DDFLOAT_POW_2_MANTISSA*XP_DDFLOAT_POW_2_MANTISSA);
      result.a0 = x.a0 * scale;   result.a1 = x.a1 * scale;
      result = recipsqrt(result); // Recursive call!
      const XP_DDFLOAT_TYPE unscale = 1.0/(2*XP_DDFLOAT_POW_2_MANTISSA);
      result.a0 *= unscale;       result.a1 *= unscale;
      return result;
    }
  }
#endif // XP_RANGE_CHECK

  const XP_DDFLOAT_TYPE y0 = sqrt(1.0/x.a0);
  XP_DDFLOAT_TYPE s0,s1,u1,u2,t1,t2;
  Xp_DoubleDouble::SquareProd(y0,s0,s1);
  Xp_DoubleDouble::TwoProd(x.a0,s0,t2,u1);    t2 -= 1.0;
  const XP_DDFLOAT_TYPE mhy0 = -0.5 * y0;
  Xp_DoubleDouble::TwoProd(x.a1,s0,t1,u2);
  u2 += x.a1*s1;

  Xp_DoubleDouble::TwoSum(u1,t2,u1,t2); u2 += t2;
  XP_DDFLOAT_TYPE v2;
  Xp_DoubleDouble::TwoSum(u1,t1,u1,v2); u2 += v2;

  Xp_DoubleDouble::TwoProd(x.a0,s1,t1,t2); u2 += t2;
  Xp_DoubleDouble::TwoSum(u1,t1,u1,v2); u2 += v2;
  u2 -= 0.75*u1*u1; // Halley correction

  Xp_DoubleDouble::TwoProd(mhy0,u1,u1,t2);
  u2 *= mhy0;
  u2 += t2;

  Xp_DoubleDouble::CoalescePlus(y0,u1,u2,result.a0,result.a1);
  return result;
}

void Xp_DoubleDouble::ReduceModTwoPiBase
(XP_DDFLOAT_TYPE x,
 XP_DDFLOAT_TYPE& r0,XP_DDFLOAT_TYPE& r1,XP_DDFLOAT_TYPE& r2) {
  // Reduces single XP_DDFLOAT_TYPE, modulo-2.pi, divided by 2.pi,
  // into three XP_DDFLOAT_TYPE words.  Return value is in range
  // [-1,1].  Intended for internal use by ReduceModTwoPi() functions.
  // See NOTES VI, 14-Jan-2013, pp 134-135.

#if XP_DDFLOAT_MANTISSA_PRECISION == 53
  // Split of 1/pi into 64 parts of 27 bits each, with relative
  // error of approximately 1e-520.  Term "j" (starting with j=0)
  // should be multiplied by 2^(-28 - j*27).
  const int block_start = -29; // Offset of first block
  const int block_size = 27; // Bits per block in invpi decomposition
  const int block_count = 64;
  const static double invpi[block_count] = {
      85445659.0,   60002565.0,   39057486.0,   92086099.0,
      40820845.0,   92952164.0,  126382600.0,   33444195.0,
      90109406.0,   22572489.0,   14447748.0,   81604096.0,
      52729717.0,    2573896.0,   60801981.0,   52212009.0,
      87684932.0,    9272651.0,   91654409.0,  110741250.0,
      56242111.0,   17098311.0,   46608490.0,   54129820.0,
      69401693.0,  125717006.0,  104853807.0,  134078553.0,
      67630999.0,   71708008.0,   21865453.0,   87457487.0,
      20863053.0,   97767823.0,  114113727.0,  111335250.0,
      64840693.0,  127387116.0,  127985470.0,  126505618.0,
     122904538.0,  132925411.0,   45748396.0,    3343471.0,
     104707541.0,  130236144.0,   68378246.0,  102607331.0,
      76221175.0,   25608729.0,   53676734.0,   21628548.0,
       4653036.0,   33633740.0,   82190528.0,  102061770.0,
      60638795.0,    3710704.0,   18405007.0,   71408694.0,
      65465972.0,    2402829.0,   54038225.0,   60169382.0
  };
#elif XP_DDFLOAT_MANTISSA_PRECISION == 64
  // The 80-bit extended precision type ("long double" on some x86
  // compilers) allows values up to approximately 1e4932 (there are
  // 15 bits in the exponent field for this type, so the max value
  // is about 2^(2^14)).
  //   Split of 1/pi into 544 parts of 32 bits each, with relative
  // error of approximately 2e-5241.  Term "j" (starting with j=0)
  // should be multiplied by 2^(-33 - j*32).
  //   Note that the 64-bit "double" type has 53 bits of mantissa,
  // so it can hold the 32-bit integers needed here.
  const int block_start = -34; // Offset of first block
  const int block_size = 32; // Bits per block in invpi decomposition
  const int block_count = 544;
  const static double invpi[block_count] = {
    2734261102.0, 1313084713.0, 4230436817.0, 4113882560.0,
    3680671129.0, 1011060801.0, 4266746795.0, 3736847713.0,
    3072618042.0, 1112396512.0,  105459434.0,  164729372.0,
    4263373596.0, 2972297022.0, 3900847605.0,  784024708.0,
    3919343654.0, 3026157121.0,  965858873.0, 2203269620.0,
    2625920907.0, 3187222587.0,  536385535.0, 3724908559.0,
    4012839307.0, 1510632735.0, 1832287951.0,  667617719.0,
    1330003814.0, 2657085997.0, 1965537991.0, 3957715323.0,
    1023883767.0, 2320667370.0, 1811636145.0,  529358088.0,
    1443049542.0, 4235946923.0, 4040145952.0, 2599695901.0,
    2850263393.0, 1592138504.0, 1704559967.0,  346056768.0,
    2382354560.0, 1299392305.0,  101061974.0, 3396577481.0,
    1625455552.0, 2355840964.0,  432236493.0, 3706194218.0,
    2203698294.0, 2341870758.0, 3719251153.0, 1461257534.0,
    2784954117.0, 1065235432.0,  851631695.0, 2553445819.0,
    3275564783.0, 1797152504.0, 2671386421.0, 3404889885.0,
    2280726928.0, 2088510570.0, 4201567607.0,  758137659.0,
     365302965.0, 2635711426.0, 3299688781.0,  744292364.0,
    1182631469.0, 1910741702.0, 2600493619.0, 2094183575.0,
    2813646165.0,  938884823.0,  403743740.0, 1984768669.0,
    1688983408.0, 4168901463.0, 2960844565.0,  391530944.0,
    3654695736.0, 2225589027.0,  611814102.0,  592730809.0,
     520100618.0, 4057976345.0, 4281442154.0,  510026071.0,
    2571631532.0, 3632234167.0, 1696762344.0,  845201382.0,
    3452235529.0,  913101887.0, 1574428182.0, 3728431250.0,
    2615027746.0, 3538454056.0, 1297670706.0, 3401979619.0,
     147553760.0, 1354766247.0,  502488032.0,  406065966.0,
    1645380353.0, 1216568206.0, 4118786221.0, 4075363907.0,
    1246286695.0,  282647978.0, 1113566926.0, 1634378792.0,
     179608019.0, 4070966911.0, 2002551746.0, 2743614561.0,
    2020837978.0, 2360327639.0, 1868801581.0, 3418354927.0,
    2173528001.0,  642107989.0,  920242898.0, 2821229921.0,
    3262630162.0,  338035867.0, 1175635033.0, 3292841416.0,
    2444381683.0,  385920323.0, 3571796265.0,  282459644.0,
    3187723412.0,  518966896.0, 4114486144.0, 4058825703.0,
    3005806791.0, 2483393342.0, 1908519689.0,  787694859.0,
    2618460283.0,  548118453.0,  784503367.0,  791837549.0,
    1426886823.0, 1914693483.0, 2529898826.0,  377086585.0,
    1099554804.0, 2543118468.0, 3873609521.0, 2573987208.0,
     912219918.0, 4256937114.0, 1215079527.0, 1114796338.0,
    1569568789.0, 2668225980.0,  624004409.0, 1962351621.0,
     805374989.0, 1745374040.0, 3995898026.0, 1191372660.0,
     618053030.0, 2113368648.0, 1861162655.0, 2794753782.0,
    2444513619.0, 3522300623.0,  865607806.0, 1274374243.0,
    2992586461.0,   56443007.0, 2307205458.0, 1438671927.0,
     282619186.0, 1211266380.0, 1540649294.0, 1851016641.0,
     151742965.0,  718628372.0, 2634491728.0,   73259835.0,
    3032839914.0,  402229117.0, 1799993895.0,  489253270.0,
    2899101268.0,  346909410.0, 2424953224.0, 1349659838.0,
    2751763463.0, 2003841267.0,  670826664.0, 1911179714.0,
    1715331172.0, 2212337559.0, 1067711892.0, 1133282829.0,
    3728814493.0,  965905520.0, 3722950423.0, 1004472363.0,
     924164224.0, 1553170522.0, 2450591960.0, 3893342080.0,
    1816920027.0,  261109878.0,  408491429.0, 1656474465.0,
    3112814525.0, 1074791666.0, 3525801289.0, 4139183035.0,
     584821268.0,  170862217.0, 1988322355.0,  990452372.0,
     246037073.0, 3265469870.0, 3987673638.0, 1548599917.0,
    2625252759.0, 1455457087.0,   66514953.0, 2353015705.0,
     829228980.0,  957685772.0, 1539561668.0, 2465549229.0,
    3332753998.0, 3442976566.0, 2850460818.0, 2875736797.0,
    3731036655.0, 2356564619.0, 1748491260.0, 2879499825.0,
     366977454.0,   14351116.0, 1716348087.0,   99430501.0,
     700405335.0,  989808569.0, 4184535998.0, 1977586472.0,
     813738998.0, 2355500491.0,   67511034.0,  501537188.0,
    3007155995.0, 1460260150.0, 3913436836.0, 3188962595.0,
     857385712.0, 2825211813.0, 3251769151.0,  198015067.0,
    1996038916.0, 2340123159.0, 2303960774.0, 3798888192.0,
    3958331466.0, 2612517572.0, 3127290575.0, 3480624386.0,
    3509449137.0, 3248065655.0, 2915293768.0, 2258656759.0,
    4102080047.0, 4037843692.0, 3720109119.0, 1843318815.0,
    3348149979.0,  708453795.0, 2595160211.0, 1403847767.0,
    3065261353.0, 2122337191.0,  131731114.0, 1990285691.0,
     705828397.0, 3084713445.0, 4211006345.0, 4257122668.0,
    1994718377.0,  108036158.0,  359564799.0, 2281506622.0,
     674457441.0, 2249730794.0, 3175985127.0, 3010358671.0,
     963089755.0, 3207678167.0, 2216091440.0, 1127073589.0,
    1629867632.0, 3384331056.0, 4251762594.0,   10806380.0,
      94428506.0, 1198465490.0,  308446300.0, 3108594032.0,
    3763759873.0, 1385772885.0, 1354224926.0, 3304141663.0,
    1846797360.0, 1571368581.0, 3283230006.0,  849454263.0,
     148156906.0,  569841380.0, 1771010047.0,  662700812.0,
     759205280.0, 3444545957.0,  550740659.0,  173879106.0,
    4189375450.0,  298892925.0, 3252394941.0,  397115810.0,
    3395054088.0,  391458389.0,    2617364.0, 2139490273.0,
    1678447757.0, 1100406462.0, 2267741658.0, 3055905588.0,
    2306604787.0,   94289849.0, 1332373672.0,  709515972.0,
    1337784365.0, 2556090261.0, 3354692941.0,  228997664.0,
    1599579313.0, 1058313528.0, 2147557580.0, 2262659510.0,
    3737777504.0, 3205588301.0, 1795621292.0, 2962018496.0,
    2991084881.0,  251338435.0, 1922382598.0, 2738176192.0,
    2078017228.0, 1172372009.0, 1321781974.0, 1106503902.0,
    1685903460.0, 2603728601.0, 3281495252.0, 1484244451.0,
    1762908912.0, 1010481734.0,  407265141.0, 1442168274.0,
    3331485277.0,  783084868.0,  239222300.0, 2277794281.0,
    4247385046.0, 3888806946.0,  898723781.0, 3758656983.0,
    4293028462.0, 3338514625.0,  143881309.0, 2092084587.0,
    2641284475.0, 1916693009.0, 3333017591.0, 3748866490.0,
    3384103168.0, 3071128290.0,  616199264.0, 2112195288.0,
    1949046029.0,  202932628.0, 1719539241.0,   24541855.0,
    3204316655.0, 1163277950.0, 3641956844.0, 3116010492.0,
    2546214824.0,  834891505.0,  918918230.0, 2832774568.0,
    3020868815.0,  763957812.0, 1466927446.0,  753127065.0,
    3105937066.0, 1584110634.0, 1053581073.0, 1242299899.0,
    4108414267.0, 2385282786.0, 2228545961.0, 3036467694.0,
    4022940974.0, 1631137604.0,  557369561.0,  453704833.0,
    1783299032.0,  472876212.0, 1401723214.0, 3424802012.0,
    1428870854.0, 3231062283.0, 3094354532.0, 2506711130.0,
     653152831.0,  252804881.0, 3052729803.0, 4230855732.0,
    4005311692.0, 1575510110.0, 3717959271.0, 4013134520.0,
     399088472.0, 1639733217.0, 3330494736.0, 1054361713.0,
    3722255405.0, 2702749510.0,  740415475.0, 1503165145.0,
    3226771194.0, 2253388806.0, 1454275045.0,  908232994.0,
    2906184851.0, 1739253845.0,  942047899.0, 3888817165.0,
    1370567577.0,  249014600.0,   90828978.0, 1705478271.0,
    2538375222.0, 3522802578.0,  558531195.0,  567253212.0,
    2671793479.0, 3694818529.0, 1122723807.0, 2650693588.0,
    1587832699.0, 2058140322.0, 4132774792.0,  727038529.0,
     141449606.0,  706839367.0,  971432841.0, 3567183168.0,
    4215925078.0, 4291432220.0, 2321138987.0, 4204053953.0,
    3553608975.0, 2925190022.0, 3309789763.0, 2235270689.0,
    2490969223.0, 1628470092.0,  706358400.0,  314524560.0,
     646482236.0, 2028258472.0, 2078008770.0,  985983732.0,
     646604791.0, 3214019883.0, 2741350803.0, 1024163005.0,
    3696338019.0, 3710377441.0, 1763284122.0, 2502535208.0,
    3462968557.0,  153132868.0, 3398979171.0, 2188387196.0,
    2117253391.0, 2398463975.0, 1444153585.0,  556441013.0,
    1300131665.0,  430287865.0, 3050758018.0, 1641911810.0,
     907452218.0, 3298927235.0, 1844277882.0, 2369366456.0,
    2187080299.0, 1529300717.0,  872445696.0, 3528848636.0,
    1297678720.0, 1910563135.0, 2310182387.0, 1688793518.0,
    2806724860.0, 1290449595.0, 1193741227.0, 3282515130.0,
    1616043316.0, 4166400747.0, 2122985915.0,  911775159.0,
    1207413140.0, 3282752402.0,  622755646.0, 3633794747.0,
    2840582318.0,   31875508.0,  960335525.0,  598356742.0,
    2383819701.0, 1001490461.0,  983384083.0, 3503037413.0,
    4167231291.0, 4069472693.0, 3205218869.0,  578059938.0
  };
#else
# error Unsupported mantissa precision
#endif

  // Split x up into two pieces, each piece having no more than
  // (MANTISSA_PRECISION+1)/2 non-zero bits of mantissa.  Then decompose
  // each part into mantissa and exponent.
  int x1_exp,x2_exp;
  XP_DDFLOAT_TYPE x1_mant,x2_mant;
  x1_mant = XP_FREXP(x,&x1_exp);
  Split(x1_mant,x1_mant,x2_mant);
  x2_exp = x1_exp - XP_DDFLOAT_MANTISSA_PRECISION;
  x1_exp -= (XP_DDFLOAT_MANTISSA_PRECISION+1)/2;
  x1_mant *= XP_DDFLOAT_POW_2_MANTISSAHALF;
  x2_mant *= XP_DDFLOAT_POW_2_MANTISSA;
  assert(x1_mant == XP_FLOOR(x1_mant) && x2_mant == XP_FLOOR(x2_mant));

  // Find offset to first block in invpi expansion than when
  // multiplied against xi can give a result that is not an integer.
  const int offblk1 = ( x1_exp + block_start < 0 ? 0
                        : (x1_exp + block_start + block_size)/block_size);
  const int offblk2 = ( x2_exp + block_start < 0 ? 0
                        : (x2_exp + block_start + block_size)/block_size);

  if(offblk1+7 >= block_count) {
    r0 = r1 = r2 = 0; // What else?  Should we raise an error?
    return;
  }

  // Multiply x1 and x2 against relevant parts of invpi expansion and
  // add terms.  Note that each product can fit into a single
  // XP_DDFLOAT_TYPE with no error.
  const int base1 = block_start + x1_exp - offblk1*block_size;
  XP_DDFLOAT_TYPE p0 = x1_mant
    * LdExp(XP_DDFLOAT_TYPE(invpi[offblk1]),base1);
  XP_DDFLOAT_TYPE p1 = x1_mant
    * LdExp(XP_DDFLOAT_TYPE(invpi[offblk1+1]),base1-block_size);
  if(x1_mant>=0) { p0 -= XP_FLOOR(p0); p1 -= XP_FLOOR(p1); }
  else           { p0 -= XP_CEIL(p0);  p1 -= XP_CEIL(p1); }
  assert(fabs(p0)<1.0 && fabs(p1)<1.0);

  XP_DDFLOAT_TYPE t0,t1,t2,s1,s2;
  TwoSum(p0,p1,t0,t1); t2=0;
  for(int i=2;i<7;++i) {
    XP_DDFLOAT_TYPE pt= x1_mant
      * LdExp(XP_DDFLOAT_TYPE(invpi[offblk1+i]),base1-i*block_size);
    assert(fabs(pt)<0.5);
    TwoSum(t0,pt,t0,s1); TwoSum(t1,s1,t1,s2); t2 += s2;
  }
  const int base2 = block_start + x2_exp - offblk2*block_size;
  XP_DDFLOAT_TYPE q0 = x2_mant
    * LdExp(XP_DDFLOAT_TYPE(invpi[offblk2]),base2);
  XP_DDFLOAT_TYPE q1 = x2_mant
    * LdExp(XP_DDFLOAT_TYPE(invpi[offblk2+1]),base2-block_size);
  if(x2_mant>=0) { q0 -= XP_FLOOR(q0); q1 -= XP_FLOOR(q1); }
  else           { q0 -= XP_CEIL(q0);  q1 -= XP_CEIL(q1); }
  assert(fabs(q0)<1.0 && fabs(q1)<1.0);
  TwoSum(t0,q0,t0,s1); TwoSum(t1,s1,t1,s2); t2 += s2;
  TwoSum(t0,q1,t0,s1); TwoSum(t1,s1,t1,s2); t2 += s2;

  for(int i=2;i<7;++i) {
    XP_DDFLOAT_TYPE qt = x2_mant
      * LdExp(XP_DDFLOAT_TYPE(invpi[offblk2+i]),base2-i*block_size);
    assert(fabs(qt)<0.5);
    TwoSum(t0,qt,t0,s1); TwoSum(t1,s1,t1,s2); t2 += s2;
  }
  TwoSum(t0,t1,r0,t1);
  TwoSum(t1,t2,r1,r2);
}

void Xp_DoubleDouble::ReduceModTwoPi
(const Xp_DoubleDouble& angle,
 XP_DDFLOAT_TYPE& r0,XP_DDFLOAT_TYPE& r1,XP_DDFLOAT_TYPE& r2) {
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(angle.a0)) {
    // Whether result is +/-Inf or NaN, result is NaN
    r0 = r1 = r2 = XP_NAN;
    return;
  }
  if(fabs(angle.a0)<hires_pi[0]) {
    r0 = angle.a0;  r1 = angle.a1;  r2 = 0.0;
    return;
  }
#endif // XP_RANGE_CHECK
  // CircleReduce does reduction modulo pi/2, but can also be used to
  // reduce modulo 2.pi by prescaling the input by 1/4 and postscaling
  // the output by 4.
  Xp_DoubleDouble tmp;
  tmp.a0 = angle.a0 * 0.25; // Range check protects against underflow.
  tmp.a1 = angle.a1 * 0.25;
  int quad_dummy;
  CircleReduce(tmp,r0,r1,r2,quad_dummy);
  r0 *= 4;  r1 *= 4;  r2 *= 4;
  return;
}

Xp_DoubleDouble& Xp_DoubleDouble::ReduceModTwoPi()
{
#if XP_RANGE_CHECK
  if(!Xp_IsFinite(a0)) {
    // Whether result is +/-Inf or NaN, result is NaN
    a0 = a1 = XP_NAN;
    return *this;
  }
  if(fabs(a0)<hires_pi[0]) { return *this; }
#endif // XP_RANGE_CHECK
  // CircleReduce does reduction modulo pi/2, but can also be used to
  // reduce modulo 2.pi by prescaling the input by 1/4 and postscaling
  // the output by 4.
  a0 *= 0.25;  a1 *= 0.25;  // Range check protects against underflow.
  XP_DDFLOAT_TYPE a2;
  int quad_dummy;
  CircleReduce(*this,a0,a1,a2,quad_dummy);
  a0 *= 4;  a1 *= 4;
  return *this;
}


void Xp_DoubleDouble::CircleReduce
(const Xp_DoubleDouble& angle,
 XP_DDFLOAT_TYPE& r0,
 XP_DDFLOAT_TYPE& r1,
 XP_DDFLOAT_TYPE& r2,
 int& quadrant)
{ // The CircleReduce function reduces import angle mod pi/2, and
  // returns the "centered" quadrant of the import angle, i.e., return
  // value
  //        |r0| <= pi/4,
  //  and
  //       angle = r + quadrant*(pi/2) + m*(2*pi)
  //
  // for some unspecified integer m.  In particular, if angle is say
  // 2*pi - eps, where 0<eps<pi/4, then upon return r = -eps and
  // quadrant = 0 (even though normally one would consider import angle
  // to lie in the third quadrant).
  //
  // Note: If you want reduction modulo some power-of-two multiple
  // of pi/4, then you can either use the quadrant info to add in the
  // appropriate multiple of pi/2, or, more generally, you can pre-
  // and post-scale the input.  For example, to compute angle mod
  // 2.pi, divide angle by 4 before calling this function; let aq
  // be angle/4.  Then this routine returns r s.t. aq = k*(pi/2) + r,
  // with -pi/4 < r <= pi/4.  Then angle = 4*aq = k*(2.pi) + 4*r, with
  // -pi < 4*r <= pi.  To be fully correct, check first that |a| is
  // large enough that a/4 won't underflow.
  //   Similarly, you can pre-multiply and post divide.
  // Say a4 = 4*angle.  Then angle = a4/4 = k*(pi/8) + r/4 with
  // -pi/16 < r/4 <= pi/16 provides angle mod pi/8; Be careful in
  // this setting that 4*angle doesn't overflow.
  //
  // WARNING: This routine is designed for internal use by
  //          Xp_DoubleDouble and friends, and performs no handling for
  //          non-finite values.  End-user wrappers of this function
  //          should include Xp_IsFinite checks as appropriate.

  if(fabs(angle.a0)<hires_pi[0]/4
     || (angle.a0 == -hires_pi[0]/4 && angle.a1 >  -hires_pi[1]/4)
     || (angle.a0 ==  hires_pi[0]/4 &&
         (angle.a1 < hires_pi[1]/4
          || (angle.a1 == hires_pi[1]/4 && 0.0<=hires_pi[2])))) {
    // Do nothing
    r0 = angle.a0;  r1 = angle.a1;  r2 = 0.0;
    quadrant = 0; // "Centered" quadrant rules.
    return;
  }

  if(fabs(angle.a0) <= 1e8) {
    // Reduce via subtraction, which should be quicker for small
    // angles.  It also appears to be more accurate, because the
    // subtraction method does not involve a triple-double
    // multiplication at the end (which can have errors up to 20 ULP).
    // NOTE: The following code assumes the import is less than
    // x/(pi/2) < pi*2^(XP_DDFLOAT_MANTISSA_PRECISION/2 - 1).
    const int block_count = 32;
    // const int block_size  = 27; // Bits per block
    const XP_DDFLOAT_TYPE block_scale = 1.0/134217728.0;  // == 1./2^27
    const static XP_DDFLOAT_TYPE halfpi[block_count] = {
      LdExp(105414357, -26),  LdExp(  8935984, -53),
      LdExp( 74025356, -80),  LdExp(103331853,-107),
      LdExp(101607572,-134),  LdExp( 67713058,-161),
      LdExp( 21821838,-188),  LdExp( 67242942,-215),
      LdExp( 87152796,-242),  LdExp(113808466,-269),
      LdExp( 68219676,-296),  LdExp( 54545886,-323),
      LdExp(130714841,-350),  LdExp(120908044,-377),
      LdExp( 57017697,-404),  LdExp( 40759903,-431),
      LdExp( 10599039,-458),  LdExp(  5069659,-485),
      LdExp( 44270731,-512),  LdExp(105405271,-539),
      LdExp( 53555007,-566),  LdExp( 52154673,-593),
      LdExp(  6108358,-620),  LdExp(132999947,-647),
      LdExp(133883319,-674),  LdExp( 83996155,-701),
      LdExp( 64778455,-728),  LdExp(129345689,-755),
      LdExp(131258191,-782),  LdExp( 76563953,-809),
      LdExp( 23329993,-836),  LdExp( 19424849,-863)
    };

    XP_DDFLOAT_TYPE p0=angle.a0,p1=angle.a1,p2=0.0;
    XP_DDFLOAT_TYPE m = XP_FLOOR(0.5 + 2*p0/hires_pi[0]);
    assert(fabs(m)
           <XP_DDFLOAT_POW_2_MANTISSAHALF/(1+XP_DDFLOAT_MANTISSA_PRECISION%2));
    // Note: If 2*p0/hires_pi[0] is very close to a half integer, then
    // without taking p1 into account we can't be sure that m is the
    // absolutely closest integer.  We check for this below; an
    // alternative would be to add in the correction term
    //    2*(p1 - hires_pi[1]*p0/hires_pi[0])/hires_pi[0]

    // Extract quadrant
    XP_DDFLOAT_TYPE dummy;
    quadrant = int(4*modf(0.25*m,&dummy));
    // If you don't like "modf", an alternative is
    //    quadrant += int(m - 4*floor(0.25*m));
    // In this case the RHS should always be >= 0.0, but be aware that
    // there is a bug in at least some versions of MSVC++ with the
    // compiler switches "/Ox /arch:SSE2" that causes some failues.
    // Example test case: m = -65059461063766472, for which the RHS
    // gives "-8".
    if(quadrant<0) quadrant += 4;

    // Subtract m*(pi/2) from import angle
    m *= -1;

    XP_DDFLOAT_TYPE t0;
    t0 = m*halfpi[0];  // m and halfpi terms are half-width, so
                      /// product is exact
    p0 += t0; // First sum should be exact, because t0 is close to p0.
    OrderedTwoSum(p0,p1,p0,p1); // Adjust for cancellation

    t0 = m*halfpi[1]; TwoSum(p0,t0,p0,t0); TwoSum(p1,t0,p1,p2);
    OrderedTwoSum(p0,p1,p0,p1); OrderedTwoSum(p1,p2,p1,p2);

    XP_DDFLOAT_TYPE checkval =
      (2*XP_DDFLOAT_POW_2_MANTISSA*block_scale)
      *(2*XP_DDFLOAT_POW_2_MANTISSA*block_scale)
      *(2*XP_DDFLOAT_POW_2_MANTISSA*block_scale);
    checkval *= fabs(m);
    for(int i=2;i<block_count;++i){
      t0 = m*halfpi[i];
      TwoSum(p0,t0,p0,t0); TwoSum(p1,t0,p1,t0); p2 += t0;
      OrderedTwoSum(p0,p1,p0,p1); OrderedTwoSum(p1,p2,p1,p2);
      if(fabs(p0)>checkval) break;
      checkval *= block_scale;
    }

    if(p0 > hires_pi[0]/4
       || (p0 == hires_pi[0]/4 && p1 > hires_pi[1]/4)
       || (p0 == hires_pi[0]/4 && p1 == hires_pi[1]/4 && p2 > hires_pi[2]/4)) {
      // p is too big
      p0 -= hires_pi[0]/2;  // Should be exact
      XP_DDFLOAT_TYPE ptmp;
      TwoSum(p1,-hires_pi[1]/2,p1,ptmp);
      p2 += ptmp - hires_pi[2]/2;
      if(++quadrant > 3) quadrant -= 4;
    } else if(p0 < -hires_pi[0]/4
       || (p0 == -hires_pi[0]/4 && p1 < -hires_pi[1]/4)
       || (p0 == -hires_pi[0]/4
           && p1 == -hires_pi[1]/4 && p2 <= -hires_pi[2]/4)) {
      // -p is too big
      p0 += hires_pi[0]/2;  // Should be exact
      XP_DDFLOAT_TYPE ptmp;
      TwoSum(p1,hires_pi[1]/2,p1,ptmp);
      p2 += ptmp + hires_pi[2]/2;
      if(--quadrant < 0) quadrant += 4;
    }

    r0 = p0;  r1 = p1;  r2 = p2;
    return;
  }

  // Otherwise, multiply import angle by 1/(2*pi), keeping (actually,
  // calculating) only the portion of the product that is < 1 in
  // absolute value (i.e., the fractional part).  The product is
  // extended as necessary until triple-double precision is achieved.
  // See NOTES VI, 14-Jan-2013, pp 134-137.

#if XP_DDFLOAT_MANTISSA_PRECISION == 53
  // Split of 1/(2.pi) into 64 parts of 27 bits each, with relative
  // error of approximately 1e-520.  Term "j" (starting with j=0)
  // should be multiplied by 2^(-28 - j*27).
  //   Expansion for 1/pi is same, except block_start would be -28
  // instead of -29.  Similarly for multiplication by other powers of 2.
  const int block_start = -29; // Offset of first block
  const int block_size = 27; // Bits per block in invpi decomposition
  const XP_DDFLOAT_TYPE block_scale = 1.0/XP_DDFLOAT_POW_2_MANTISSAHALF;
  /// == 1/pow(2,block_size)
  const XP_DDFLOAT_TYPE start_scale = block_scale/4.;
  /// == pow(2,block_start)
  const int block_count = 64;
  const static double invtwopi[block_count] = {
      85445659.0,   60002565.0,   39057486.0,   92086099.0,
      40820845.0,   92952164.0,  126382600.0,   33444195.0,
      90109406.0,   22572489.0,   14447748.0,   81604096.0,
      52729717.0,    2573896.0,   60801981.0,   52212009.0,
      87684932.0,    9272651.0,   91654409.0,  110741250.0,
      56242111.0,   17098311.0,   46608490.0,   54129820.0,
      69401693.0,  125717006.0,  104853807.0,  134078553.0,
      67630999.0,   71708008.0,   21865453.0,   87457487.0,
      20863053.0,   97767823.0,  114113727.0,  111335250.0,
      64840693.0,  127387116.0,  127985470.0,  126505618.0,
     122904538.0,  132925411.0,   45748396.0,    3343471.0,
     104707541.0,  130236144.0,   68378246.0,  102607331.0,
      76221175.0,   25608729.0,   53676734.0,   21628548.0,
       4653036.0,   33633740.0,   82190528.0,  102061770.0,
      60638795.0,    3710704.0,   18405007.0,   71408694.0,
      65465972.0,    2402829.0,   54038225.0,   60169382.0
  };
#elif XP_DDFLOAT_MANTISSA_PRECISION == 64
  // The 80-bit extended precision type ("long double" on some x86
  // compilers) allows values up to approximately 1e4932 (there are
  // 15 bits in the exponent field for this type, so the max value
  // is about 2^(2^14)).
  //   Split of 1/(2.pi) into 544 parts of 32 bits each, with relative
  // error of approximately 2e-5241.  Term "j" (starting with j=0)
  // should be multiplied by 2^(-33 - j*32).
  //   Expansion for 1/pi is same, except block_start would be -33
  // instead of -34.  Similarly for multiplication by other powers of 2.
  //   Note that the 64-bit "double" type has 53 bits of mantissa,
  // so it can hold the 32-bit integers needed here.
  const int block_start = -34; // Offset of first block
  const int block_size = 32; // Bits per block in invpi decomposition
  const XP_DDFLOAT_TYPE block_scale = 1.0/XP_DDFLOAT_POW_2_MANTISSAHALF;
  /// == 1/pow(2,block_size)
  const XP_DDFLOAT_TYPE start_scale = block_scale/4.;
  /// == pow(2,block_start)
  const int block_count = 544;
  const static double invtwopi[block_count] = {
    2734261102.0, 1313084713.0, 4230436817.0, 4113882560.0,
    3680671129.0, 1011060801.0, 4266746795.0, 3736847713.0,
    3072618042.0, 1112396512.0,  105459434.0,  164729372.0,
    4263373596.0, 2972297022.0, 3900847605.0,  784024708.0,
    3919343654.0, 3026157121.0,  965858873.0, 2203269620.0,
    2625920907.0, 3187222587.0,  536385535.0, 3724908559.0,
    4012839307.0, 1510632735.0, 1832287951.0,  667617719.0,
    1330003814.0, 2657085997.0, 1965537991.0, 3957715323.0,
    1023883767.0, 2320667370.0, 1811636145.0,  529358088.0,
    1443049542.0, 4235946923.0, 4040145952.0, 2599695901.0,
    2850263393.0, 1592138504.0, 1704559967.0,  346056768.0,
    2382354560.0, 1299392305.0,  101061974.0, 3396577481.0,
    1625455552.0, 2355840964.0,  432236493.0, 3706194218.0,
    2203698294.0, 2341870758.0, 3719251153.0, 1461257534.0,
    2784954117.0, 1065235432.0,  851631695.0, 2553445819.0,
    3275564783.0, 1797152504.0, 2671386421.0, 3404889885.0,
    2280726928.0, 2088510570.0, 4201567607.0,  758137659.0,
     365302965.0, 2635711426.0, 3299688781.0,  744292364.0,
    1182631469.0, 1910741702.0, 2600493619.0, 2094183575.0,
    2813646165.0,  938884823.0,  403743740.0, 1984768669.0,
    1688983408.0, 4168901463.0, 2960844565.0,  391530944.0,
    3654695736.0, 2225589027.0,  611814102.0,  592730809.0,
     520100618.0, 4057976345.0, 4281442154.0,  510026071.0,
    2571631532.0, 3632234167.0, 1696762344.0,  845201382.0,
    3452235529.0,  913101887.0, 1574428182.0, 3728431250.0,
    2615027746.0, 3538454056.0, 1297670706.0, 3401979619.0,
     147553760.0, 1354766247.0,  502488032.0,  406065966.0,
    1645380353.0, 1216568206.0, 4118786221.0, 4075363907.0,
    1246286695.0,  282647978.0, 1113566926.0, 1634378792.0,
     179608019.0, 4070966911.0, 2002551746.0, 2743614561.0,
    2020837978.0, 2360327639.0, 1868801581.0, 3418354927.0,
    2173528001.0,  642107989.0,  920242898.0, 2821229921.0,
    3262630162.0,  338035867.0, 1175635033.0, 3292841416.0,
    2444381683.0,  385920323.0, 3571796265.0,  282459644.0,
    3187723412.0,  518966896.0, 4114486144.0, 4058825703.0,
    3005806791.0, 2483393342.0, 1908519689.0,  787694859.0,
    2618460283.0,  548118453.0,  784503367.0,  791837549.0,
    1426886823.0, 1914693483.0, 2529898826.0,  377086585.0,
    1099554804.0, 2543118468.0, 3873609521.0, 2573987208.0,
     912219918.0, 4256937114.0, 1215079527.0, 1114796338.0,
    1569568789.0, 2668225980.0,  624004409.0, 1962351621.0,
     805374989.0, 1745374040.0, 3995898026.0, 1191372660.0,
     618053030.0, 2113368648.0, 1861162655.0, 2794753782.0,
    2444513619.0, 3522300623.0,  865607806.0, 1274374243.0,
    2992586461.0,   56443007.0, 2307205458.0, 1438671927.0,
     282619186.0, 1211266380.0, 1540649294.0, 1851016641.0,
     151742965.0,  718628372.0, 2634491728.0,   73259835.0,
    3032839914.0,  402229117.0, 1799993895.0,  489253270.0,
    2899101268.0,  346909410.0, 2424953224.0, 1349659838.0,
    2751763463.0, 2003841267.0,  670826664.0, 1911179714.0,
    1715331172.0, 2212337559.0, 1067711892.0, 1133282829.0,
    3728814493.0,  965905520.0, 3722950423.0, 1004472363.0,
     924164224.0, 1553170522.0, 2450591960.0, 3893342080.0,
    1816920027.0,  261109878.0,  408491429.0, 1656474465.0,
    3112814525.0, 1074791666.0, 3525801289.0, 4139183035.0,
     584821268.0,  170862217.0, 1988322355.0,  990452372.0,
     246037073.0, 3265469870.0, 3987673638.0, 1548599917.0,
    2625252759.0, 1455457087.0,   66514953.0, 2353015705.0,
     829228980.0,  957685772.0, 1539561668.0, 2465549229.0,
    3332753998.0, 3442976566.0, 2850460818.0, 2875736797.0,
    3731036655.0, 2356564619.0, 1748491260.0, 2879499825.0,
     366977454.0,   14351116.0, 1716348087.0,   99430501.0,
     700405335.0,  989808569.0, 4184535998.0, 1977586472.0,
     813738998.0, 2355500491.0,   67511034.0,  501537188.0,
    3007155995.0, 1460260150.0, 3913436836.0, 3188962595.0,
     857385712.0, 2825211813.0, 3251769151.0,  198015067.0,
    1996038916.0, 2340123159.0, 2303960774.0, 3798888192.0,
    3958331466.0, 2612517572.0, 3127290575.0, 3480624386.0,
    3509449137.0, 3248065655.0, 2915293768.0, 2258656759.0,
    4102080047.0, 4037843692.0, 3720109119.0, 1843318815.0,
    3348149979.0,  708453795.0, 2595160211.0, 1403847767.0,
    3065261353.0, 2122337191.0,  131731114.0, 1990285691.0,
     705828397.0, 3084713445.0, 4211006345.0, 4257122668.0,
    1994718377.0,  108036158.0,  359564799.0, 2281506622.0,
     674457441.0, 2249730794.0, 3175985127.0, 3010358671.0,
     963089755.0, 3207678167.0, 2216091440.0, 1127073589.0,
    1629867632.0, 3384331056.0, 4251762594.0,   10806380.0,
      94428506.0, 1198465490.0,  308446300.0, 3108594032.0,
    3763759873.0, 1385772885.0, 1354224926.0, 3304141663.0,
    1846797360.0, 1571368581.0, 3283230006.0,  849454263.0,
     148156906.0,  569841380.0, 1771010047.0,  662700812.0,
     759205280.0, 3444545957.0,  550740659.0,  173879106.0,
    4189375450.0,  298892925.0, 3252394941.0,  397115810.0,
    3395054088.0,  391458389.0,    2617364.0, 2139490273.0,
    1678447757.0, 1100406462.0, 2267741658.0, 3055905588.0,
    2306604787.0,   94289849.0, 1332373672.0,  709515972.0,
    1337784365.0, 2556090261.0, 3354692941.0,  228997664.0,
    1599579313.0, 1058313528.0, 2147557580.0, 2262659510.0,
    3737777504.0, 3205588301.0, 1795621292.0, 2962018496.0,
    2991084881.0,  251338435.0, 1922382598.0, 2738176192.0,
    2078017228.0, 1172372009.0, 1321781974.0, 1106503902.0,
    1685903460.0, 2603728601.0, 3281495252.0, 1484244451.0,
    1762908912.0, 1010481734.0,  407265141.0, 1442168274.0,
    3331485277.0,  783084868.0,  239222300.0, 2277794281.0,
    4247385046.0, 3888806946.0,  898723781.0, 3758656983.0,
    4293028462.0, 3338514625.0,  143881309.0, 2092084587.0,
    2641284475.0, 1916693009.0, 3333017591.0, 3748866490.0,
    3384103168.0, 3071128290.0,  616199264.0, 2112195288.0,
    1949046029.0,  202932628.0, 1719539241.0,   24541855.0,
    3204316655.0, 1163277950.0, 3641956844.0, 3116010492.0,
    2546214824.0,  834891505.0,  918918230.0, 2832774568.0,
    3020868815.0,  763957812.0, 1466927446.0,  753127065.0,
    3105937066.0, 1584110634.0, 1053581073.0, 1242299899.0,
    4108414267.0, 2385282786.0, 2228545961.0, 3036467694.0,
    4022940974.0, 1631137604.0,  557369561.0,  453704833.0,
    1783299032.0,  472876212.0, 1401723214.0, 3424802012.0,
    1428870854.0, 3231062283.0, 3094354532.0, 2506711130.0,
     653152831.0,  252804881.0, 3052729803.0, 4230855732.0,
    4005311692.0, 1575510110.0, 3717959271.0, 4013134520.0,
     399088472.0, 1639733217.0, 3330494736.0, 1054361713.0,
    3722255405.0, 2702749510.0,  740415475.0, 1503165145.0,
    3226771194.0, 2253388806.0, 1454275045.0,  908232994.0,
    2906184851.0, 1739253845.0,  942047899.0, 3888817165.0,
    1370567577.0,  249014600.0,   90828978.0, 1705478271.0,
    2538375222.0, 3522802578.0,  558531195.0,  567253212.0,
    2671793479.0, 3694818529.0, 1122723807.0, 2650693588.0,
    1587832699.0, 2058140322.0, 4132774792.0,  727038529.0,
     141449606.0,  706839367.0,  971432841.0, 3567183168.0,
    4215925078.0, 4291432220.0, 2321138987.0, 4204053953.0,
    3553608975.0, 2925190022.0, 3309789763.0, 2235270689.0,
    2490969223.0, 1628470092.0,  706358400.0,  314524560.0,
     646482236.0, 2028258472.0, 2078008770.0,  985983732.0,
     646604791.0, 3214019883.0, 2741350803.0, 1024163005.0,
    3696338019.0, 3710377441.0, 1763284122.0, 2502535208.0,
    3462968557.0,  153132868.0, 3398979171.0, 2188387196.0,
    2117253391.0, 2398463975.0, 1444153585.0,  556441013.0,
    1300131665.0,  430287865.0, 3050758018.0, 1641911810.0,
     907452218.0, 3298927235.0, 1844277882.0, 2369366456.0,
    2187080299.0, 1529300717.0,  872445696.0, 3528848636.0,
    1297678720.0, 1910563135.0, 2310182387.0, 1688793518.0,
    2806724860.0, 1290449595.0, 1193741227.0, 3282515130.0,
    1616043316.0, 4166400747.0, 2122985915.0,  911775159.0,
    1207413140.0, 3282752402.0,  622755646.0, 3633794747.0,
    2840582318.0,   31875508.0,  960335525.0,  598356742.0,
    2383819701.0, 1001490461.0,  983384083.0, 3503037413.0,
    4167231291.0, 4069472693.0, 3205218869.0,  578059938.0
  };
#else
# error Unsupported mantissa precision
#endif

  // Split import into four pieces, each piece having no more than
  // (MANTISSA_PRECISION+1)/2 non-zero bits of mantissa.  The x#_exp
  // values a selected so that x# * 2^(x#_exp) is an integer.
  XP_DDFLOAT_TYPE a0_mant, a1_mant;
  int x0_exp, x1_exp, x2_exp, x3_exp;
  XP_DDFLOAT_TYPE x0,x1,x2,x3;
  a0_mant = XP_FREXP(angle.a0,&x0_exp);
  a1_mant = XP_FREXP(angle.a1,&x2_exp);
  Split(a0_mant,x0,x1);
  Split(a1_mant,x2,x3);

  // Find offset to first block in invtwopi expansion than when
  // multiplied against xi can give a result that is not an integer.
  // From check at top of routine, we know that |angle.a0|>=pi/4,
  // so x0_exp >= -(XP_DDFLOAT_MANTISSA_PRECISION+1)/2 = -block_size,
  // thus offblk0 >= block_start/block_size = -1 Similarly,
  // x1_exp >= -XP_DDFLOAT_MANTISSA_PRECISION, so offblk1 >= -2
  x1_exp = x0_exp -  XP_DDFLOAT_MANTISSA_PRECISION;
  x0_exp = x0_exp - (XP_DDFLOAT_MANTISSA_PRECISION+1)/2;
  const int offblk1 = (x1_exp + block_start + block_size)/block_size;
  const int offblk0 = (x0_exp + block_start + block_size)/block_size;
  assert(offblk1 >= -2);
  assert(offblk0 >= -1);
  if(offblk0+8 > block_count) {
    throw("Access attempt beyond end of invtwopi array.");
  }

  x3_exp = x2_exp -  XP_DDFLOAT_MANTISSA_PRECISION;
  x2_exp = x2_exp - (XP_DDFLOAT_MANTISSA_PRECISION+1)/2;
  const int offblk3 = (x3_exp + block_start + block_size)/block_size;
  const int offblk2 = (x2_exp + block_start + block_size)/block_size;

  // offblk0 > offblk1 > offblk2 > offblk3

  // Multiply x1 and x2 against relevant parts of invtwopi expansion and
  // add terms.  Note that each product can fit into a single
  // XP_DDFLOAT_TYPE with no error.
  XP_DDFLOAT_TYPE p0=0.0,p1=0.0,p2=0.0,t0,t1;

  x0 = LdExp(x0,x0_exp-offblk0*block_size+(XP_DDFLOAT_MANTISSA_PRECISION+1)/2);
  x1 = LdExp(x1,x1_exp-offblk1*block_size+XP_DDFLOAT_MANTISSA_PRECISION);

  if(offblk0>=0) p0 = x0 * start_scale * XP_DDFLOAT_TYPE(invtwopi[offblk0]);
  if(offblk0+1>=0) {
    p1 = x0 * start_scale  * block_scale * XP_DDFLOAT_TYPE(invtwopi[offblk0+1]);
    if(x0>=0) { p0 -= XP_FLOOR(p0); p1 -= XP_FLOOR(p1); }
    else      { p0 -= XP_CEIL(p0);  p1 -= XP_CEIL(p1); }
    assert(fabs(p0)<1.0 && fabs(p1)<1.0);
    TwoSum(p0,p1,p0,p1);
  }

  t0 = t1 = 0.0;
  if(offblk1>=0) t0 = x1 * start_scale * XP_DDFLOAT_TYPE(invtwopi[offblk1]);
  if(offblk1+1>=0) {
    t1 = x1 * start_scale * block_scale * XP_DDFLOAT_TYPE(invtwopi[offblk1+1]);
    if(x1>=0) { t0 -= XP_FLOOR(t0); t1 -= XP_FLOOR(t1); }
    else      { t0 -= XP_CEIL(t0);  t1 -= XP_CEIL(t1); }
    assert(fabs(t0)<1.0 && fabs(t1)<1.0);
    TwoSum(p0,t0,p0,t0); TwoSum(p1,t0,p1,t0); p2 += t0;
    TwoSum(p0,t1,p0,t1); TwoSum(p1,t1,p1,t1); p2 += t1;
  }

  x2 = LdExp(x2,x2_exp-offblk2*block_size+(XP_DDFLOAT_MANTISSA_PRECISION+1)/2);
  x3 = LdExp(x3,x3_exp-offblk3*block_size+XP_DDFLOAT_MANTISSA_PRECISION);

  t0 = t1 = 0.0;
  if(offblk2>=0) t0 = x2 * start_scale * XP_DDFLOAT_TYPE(invtwopi[offblk2]);
  if(offblk2+1>=0) {
    t1 = x2 * start_scale * block_scale * XP_DDFLOAT_TYPE(invtwopi[offblk2+1]);
    if(x2>=0) { t0 -= XP_FLOOR(t0); t1 -= XP_FLOOR(t1); }
    else      { t0 -= XP_CEIL(t0);  t1 -= XP_CEIL(t1); }
    assert(fabs(t0)<1.0 && fabs(t1)<1.0);
    TwoSum(p0,t0,p0,t0); TwoSum(p1,t0,p1,t0); p2 += t0;
    TwoSum(p0,t1,p0,t1); TwoSum(p1,t1,p1,t1); p2 += t1;
  }

  t0 = t1 = 0.0;
  if(offblk3>=0) t0 = x3 * start_scale * XP_DDFLOAT_TYPE(invtwopi[offblk3]);
  if(offblk3+1>=0) {
    t1 = x3 * start_scale * block_scale * XP_DDFLOAT_TYPE(invtwopi[offblk3+1]);
    if(x3>=0) { t0 -= XP_FLOOR(t0); t1 -= XP_FLOOR(t1); }
    else      { t0 -= XP_CEIL(t0);  t1 -= XP_CEIL(t1); }
    assert(fabs(t0)<1.0 && fabs(t1)<1.0);
    TwoSum(p0,t0,p0,t0); TwoSum(p1,t0,p1,t0); p2 += t0;
    TwoSum(p0,t1,p0,t1); TwoSum(p1,t1,p1,t1); p2 += t1;
  }

  if(fabs(p0)>0.5) {
    p0 -= XP_FLOOR(p0);
    if(p0 > 0.5) p0 -= 1.0;
  }

  // Multiplication above is by 1/(2.pi).  Shift this now to 1/(pi/2)
  // and extract quadrant info.  (This way the quadrant offset doesn't
  // interfere with the precision in the low order bits.)
  p0 *= 4;  p1 *= 4;  p2 *= 4;
  quadrant = 0;
  if(fabs(p0)>0.5) {
    XP_DDFLOAT_TYPE fq = XP_FLOOR(p0);
    quadrant = static_cast<int>(fq);
    p0 -= fq;
    if(p0 > 0.5) {
      p0 -= 1.0;
      ++quadrant;
    }
  }
  OrderedTwoSum(p0,p1,p0,p1);
  OrderedTwoSum(p1,p2,p1,p2);

  const XP_DDFLOAT_TYPE checkval =
    XP_DDFLOAT_POW_2_MANTISSAHALF * XP_DDFLOAT_POW_2_MANTISSAHALF *
    XP_DDFLOAT_POW_2_MANTISSAHALF * XP_DDFLOAT_POW_2_MANTISSAHALF *
    XP_DDFLOAT_POW_2_MANTISSAHALF * XP_DDFLOAT_POW_2_MANTISSAHALF *
    XP_DDFLOAT_POW_2_MANTISSAHALF * XP_DDFLOAT_POW_2_MANTISSAHALF
    /(4*start_scale);

  XP_DDFLOAT_TYPE scale = 4*block_scale*block_scale*start_scale;

  int i=2;
  while(fabs(p0)<checkval*scale && offblk0+i<block_count) {
    // The offblk0+i >= block_count could be handled more gracefully,
    // but hopefully it doesn't occur in practice.

    // As noted earlier, offblk0 and offblk1 are >= -2, so we know
    // offblk0+i and offblk1+i are >=0.

    // NOTE: In each of the following LdExp(...) expressions,
    // the exponent is base# + 2 - i*blocksize.  The "+2" is because
    // now we want to multiply by 1/(pi/2) rather than 1/(2.pi)
    XP_DDFLOAT_TYPE ta,tb;
    ta = x0 * XP_DDFLOAT_TYPE(invtwopi[offblk0+i]) * scale;
    assert(fabs(ta)<2.0);
    TwoSum(p0,ta,p0,ta); TwoSum(p1,ta,p1,ta); p2 += ta;

    tb = x1 * XP_DDFLOAT_TYPE(invtwopi[offblk1+i]) * scale;
    assert(fabs(tb)<2.0);
    TwoSum(p0,tb,p0,tb); TwoSum(p1,tb,p1,tb); p2 += tb;

    if(offblk2+i>=0) {
      XP_DDFLOAT_TYPE tc;
      tc = x2 * XP_DDFLOAT_TYPE(invtwopi[offblk2+i]) * scale;
      assert(fabs(tc)<2.0);
      TwoSum(p0,tc,p0,tc); TwoSum(p1,tc,p1,tc); p2 += tc;
    }

    if(offblk3+i>=0) {
      XP_DDFLOAT_TYPE td;
      td = x3 * XP_DDFLOAT_TYPE(invtwopi[offblk3+i]) * scale;
      assert(fabs(td)<2.0);
      TwoSum(p0,td,p0,td); TwoSum(p1,td,p1,td); p2 += td;
    }

    if(fabs(p0)>0.5) {
      XP_DDFLOAT_TYPE fq = XP_FLOOR(p0);
      quadrant += static_cast<int>(fq);
      p0 -= fq;  // This is exact
      if(p0 > 0.5) {
        p0 -= 1.0; // This is exact
        ++quadrant;
      }
    }

    OrderedTwoSum(p0,p1,p0,p1);
    OrderedTwoSum(p1,p2,p1,p2);

    scale *= block_scale;
    ++i;
  }

  // Handle boundary cases
  if(p0 == 0.5 && p1 > 0.0) {
    p0 -= 1.0;
    ++quadrant;
  } else if(p0 == -0.5 && p1 <= 0.0) {
    p0 += 1.0;
    --quadrant;
  }

  // At this point, (p0,p1,p2) is the fractional part of angle/(pi/2).
  // To finish, adjust quadrant and multiply by pi/2.
  quadrant %= 4;
  if(quadrant<0) quadrant += 4;

  // The worse-case error produced by the following SloppyProd call
  // creates more error than all of the above computations combined.
  SloppyProd(p0,p1,p2,
             0.5*hires_pi[0],0.5*hires_pi[1],0.5*hires_pi[2],
             r0,r1,r2);
  assert(fabs(r0)<=hires_pi[0]/4.);
}

// Triple-double SinCos, for class internal use (note CamelCase).
void Xp_DoubleDouble::SinCos
(const Xp_DoubleDouble& angle,
 Xp_DoubleDouble& sin_angle,XP_DDFLOAT_TYPE& sin_angle_a2,
 Xp_DoubleDouble& cos_angle,XP_DDFLOAT_TYPE& cos_angle_a2)
{ // Computes sin and cos of import angle.  See NOTES VI, 23-Dec-2012,
  // pp 130-131.  Error appears to be less than just a tad over
  // 0.5 ULP.  Sample test case, for 53-bit mantissa:
  //    2544286953068562390800870175923811 * 2^-118
  //    = 0x1F5C5AA2166540 * 2^-60 + 0x1412CA49C7DE63 * 2^-118

#if XP_RANGE_CHECK
  if(!Xp_IsFinite(angle.a0)) {
    sin_angle.a0 = sin_angle.a1 = sin_angle_a2 = XP_NAN;
    cos_angle.a0 = cos_angle.a1 = cos_angle_a2 = XP_NAN;
    return;
  }
#endif

  Xp_DoubleDouble r;    // Working angle
  XP_DDFLOAT_TYPE r_a2; // Working "third" double
  int quadrant;

  // First, reduce import angle modulo pi/2, keeping quadrant info.
  if(fabs(angle.a0)<hires_pi[0]/4) {
    // This check is built into CircleReduce, but in timing
    // tests it seems faster to test for this common case
    // here and avoid the function call overhead.
    r.a0 = angle.a0;   r.a1 = angle.a1;   r_a2 = 0.0;
    quadrant = 0;
  } else {
    CircleReduce(angle,r.a0,r.a1,r_a2,quadrant);
  }

  Xp_DoubleDouble cosb; XP_DDFLOAT_TYPE cosb_a2;
  Xp_DoubleDouble sinb; XP_DDFLOAT_TYPE sinb_a2;

  if(fabs(r.a0) < XP_DDFLOAT_CUBEROOT_VERYTINY) {
    // If r.a0^2 underflows, then the series in the mainline is rubbish.
    // Instead, compute sin and cos directly from the series
    //   sin(x) = x ( - x^3/3 + ... )
    //   cos(x) = 1 - x^2/2 ( + x^4/4! - ...)
    // where the parenthesized expressions evaluate to floating-point zero.
    sinb.a0 = r.a0;  sinb.a1 = r.a1;         sinb_a2 = r_a2;
    cosb.a0 = 1.0;
    XP_DDFLOAT_TYPE dummy;
    SloppySquare(r.a0,r.a1,r_a2,cosb.a1,cosb_a2,dummy); // overkill
    cosb.a1 *= -0.5; cosb_a2 *= -0.5;
  } else {
    // Mainline: reduce until |r|^14/14! < 0.5*|r|^2*doubledouble_precision
#if XP_DDFLOAT_MANTISSA_PRECISION == 53
    const XP_DDFLOAT_TYPE checkval = 8e-3;
#elif XP_DDFLOAT_MANTISSA_PRECISION == 64
    const XP_DDFLOAT_TYPE checkval = 2e-3;
#else
#  error Unsupported mantissa precision
#endif

    int kreduction = 0;
    while(fabs(double(r.a0)) > 8*checkval) {
      r.a0 *= 0.0625;   r.a1 *= 0.0625;
      r_a2 *= 0.0625;
      kreduction += 4;
    }
    while(fabs(double(r.a0)) > checkval) {
      r.a0 *= 0.5;   r.a1 *= 0.5;
      r_a2 *= 0.5;
      ++kreduction;
    }

    // Compute 1-cos(r) using standard series.  Assume it suffices to
    // include terms up through r^12.  This gives a few extra digits
    // of accuracy that help push down rounding error. (Error bound
    // is at best 0.5 ULP + series truncation error.)
    Xp_DoubleDouble rsq;
    XP_DDFLOAT_TYPE rsq_a2;
    SloppySquare(r.a0,r.a1,r_a2,rsq.a0,rsq.a1,rsq_a2);
    // First few terms can be computed at single-double precision
    // without affecting final accuracy.
    XP_DDFLOAT_TYPE ssum = rsq.a0;  ssum /= (-12.*11.);
    ssum += 1.0;    ssum *= rsq.a0; ssum /= (10.*9.);
    Xp_DoubleDouble sum(ssum); // From this point use double-double
    sum -= 1.0;    sum *= rsq;  sum /= (8.*7.);
    sum += 1.0;    sum *= rsq;  sum /= (6.*5.);
    sum -= 1.0;    sum *= rsq;  sum /= (4.*3.);
    // At the end use triple-double to wring out best accuracy.
    XP_DDFLOAT_TYPE sum_a2;
    SloppyProd(sum.a0,sum.a1,0.0,
               rsq.a0,rsq.a1,rsq_a2,
               sum.a0,sum.a1,sum_a2);
    ThreeSum(sum.a0,sum.a1,sum_a2,
             rsq.a0,rsq.a1,rsq_a2,
             sum.a0,sum.a1,sum_a2);
    sum.a0 *= 0.5; sum.a1 *= 0.5; sum_a2 *= 0.5;

    // Unscale, using formula
    // cos(2x)-1 = 2*(cos(x)-1)^2 + 4*(cos(x)-1)
    for(int k=0;k<kreduction;++k) {
      XP_DDFLOAT_TYPE t0,t1,t2;
      SloppySquare(sum.a0,sum.a1,sum_a2,t0,t1,t2);
      ThreeSum(4*sum.a0,4*sum.a1,4*sum_a2,
               -2*t0,-2*t1,-2*t2,
               sum.a0,sum.a1,sum_a2);
    }

    ThreeSum(1.,0.,0.,-sum.a0,-sum.a1,-sum_a2,
             cosb.a0,cosb.a1,cosb_a2);
    sinb.a0 = 2*sum.a0; sinb.a1 = 2*sum.a1; sinb_a2 = 2*sum_a2;
    SloppySquare(sum.a0,sum.a1,sum_a2,
                 sum.a0,sum.a1,sum_a2);
    ThreeSum(sinb.a0,sinb.a1,sinb_a2,
             -sum.a0,-sum.a1,-sum_a2,
             sinb.a0,sinb.a1,sinb_a2);
    if(sinb.a0 <= 0.0) {
      sinb.a0 = sinb.a1 = sinb_a2 = 0.0;
    } else {
      SloppySqrt(sinb.a0,sinb.a1,sinb_a2,
                 sinb.a0,sinb.a1,sinb_a2);
      if(r.a0<0.0) {
        sinb.a0 *= -1;  sinb.a1 *= -1; sinb_a2 *= -1;
      }
    }
  }

  // Quadrant fix-up.  Uses formulae
  //     sin(a+b) = sin(a)*cos(b) + sin(b)*cos(a)
  //     cos(a+b) = cos(a)*cos(b) - sin(a)*sin(b)
  // here a = m*(pi/2), and 1 - sum = cos(b)
  switch(quadrant) {
  case 0:  // m = 0 => sin(a) =  0, cos(a) =  1
    sin_angle = sinb;  sin_angle_a2 = sinb_a2;
    cos_angle = cosb;  cos_angle_a2 = cosb_a2;
    break;

  case 1:  // m = 1 => sin(a) =  1, cos(a) =  0
    sin_angle = cosb;  sin_angle_a2 = cosb_a2;
    cos_angle.a0 = -1*sinb.a0;
    cos_angle.a1 = -1*sinb.a1;
    cos_angle_a2 = -1*sinb_a2;
    break;

  case 2:  // m = 2 => sin(a) =  0, cos(a) = -1
    sin_angle.a0 = -1*sinb.a0;
    sin_angle.a1 = -1*sinb.a1;
    sin_angle_a2 = -1*sinb_a2;
    cos_angle.a0 = -1*cosb.a0;
    cos_angle.a1 = -1*cosb.a1;
    cos_angle_a2 = -1*cosb_a2;
    break;

  default: // m = 3 => sin(a) = -1, cos(a) =  0
    sin_angle.a0 = -1*cosb.a0;
    sin_angle.a1 = -1*cosb.a1;
    sin_angle_a2 = -1*cosb_a2;
    cos_angle = sinb; cos_angle_a2 = sinb_a2;
    break;
  }

  return;
}

void Xp_DoubleDouble::ExpBase
(const Xp_DoubleDouble& inval,
 Xp_DoubleDouble& outval, XP_DDFLOAT_TYPE& outval_a2,
 int& outm) {
  // Base computation for expm1 and exp, via power series.  On return,
  // the export m specifies the scaling (as 2^m) needed to undo the
  // scaling done inside this function to bring the argument within
  // good convergence range for the exp power series.  This scaling is
  // left for the caller function, because for best precision it needs
  // to be handled differently in expm1 than in exp.

  // On exit, outval + outval_a2 = e^inval * 2^(-m) - 1.

  // Equivalently, if the input generates finite output, then inval is
  // reduced to inval = m*log(2) + r, and outval is set to exp(r)-1.
  // Here |r|<=0.5*log(2) so
  //
  //     0.707 < sqrt(0.5) <= e^r <= sqrt(2) < 1.42
  //            => -0.293 < outval < 0.42.
  //
  // With respect to range limits on m, for 8-byte floats
  //
  //     -745.13321910194134 < inval < 709.78271289338409
  //                 => -1075 <= m <= 1024
  //
  // Likewise, for 10-byte (64-bit mantissa) mantissa, we find
  //
  //                   -16446 <= m <= 16384
  //
  // (see discussion on input range limits below).

  // Error appears to be close to 0.5 ULP.

  // Sample test case, for 53-bit mantissa:
  //    -5184960692966200 * 2^-51

  // See NOTES VI, 26-Nov-2012, p118-120.

  // Largest input magnitudes that return less than infinity:

  // For 8-byte (53-bit mantissa) form, the max representable value is
  // just under 2^1024, and log(2^1024) = 1024 * log(2) < 710;
  // The min representable value > 0 is just under 2^-1074, so the
  // smallest non-trivial input value is -1075 * log(2) > -746.
  // (The asymmetry between the upper and lower limits is due to
  // denormals on the latter.)

  // For 10-byte (64-bit mantissa) form, the max representable value
  // is just under 2^16384, and log(2^16384) = 16384 * log(2) < 11357;
  // The min representable value > 0 is just under 2^-16445, so the
  // smallest non-trivial input value is -16446 * log(2) > -11400.

  // The following range restriction code block assumes the radix for
  // the exponent representation is 2.  If XP_DDFLOAT_HUGE_EXP is 1024
  // (presumably 8-byte form with 53-bit mantissa) then the input
  // range for exp(x) with finite non-zero output is roughly
  // (-746,710); for XP_DDFLOAT_MAX_HUGE==16384 (presumably 10-byte
  // form with 64-bit mantissa) the input range is roughly
  // (-11400,11357).  We assume without checking that
  // -1*XP_DDFLOAT_TINY_EXP is close to XP_DDFLOAT_HUGE_EXP.  All of
  // this is easily modified if necessary.
#if XP_FLT_RADIX != 2
# error Code assumes FLT_RADIX == 2, but it is not
#endif
#if XP_DDFLOAT_HUGE_EXP <= 1024
  const XP_DDFLOAT_TYPE max_inval =  709.78271289338409;
  const XP_DDFLOAT_TYPE min_inval = -745.13321910194134;
#elif XP_DDFLOAT_HUGE_EXP <= 16384
# if !defined(__BORLANDC__) && !defined(__DMC__)
  const XP_DDFLOAT_TYPE max_inval =  11356.5234062941439506L;
  const XP_DDFLOAT_TYPE min_inval = -11399.4985314888605594L;
# else // __BORLANDC__  : Workaround for old Borland C++ long double I/O bug
  const XP_DDFLOAT_TYPE max_inval
    = ( 12786308640000000000.L + 5202655661.L)/1125899906842624.L;
  const XP_DDFLOAT_TYPE min_inval
    = (-12834694330000000000.L - 4655937194.L)/1125899906842624.L;
# endif // __BORLANDC__
#else // XP_DDFLOAT_HUGE_EXP
#  error Unsupported floating point exponent size
#endif // XP_DDFLOAT_HUGE_EXP


  if(inval.a0>=max_inval) {
    // Note: There are some values with inval.a0 just under max_inval
    // which also evaluate to inf.  The code below will compute the
    // correct value but such that outval * 2^m overflows. It is up
    // to the caller to handle that border case.
    outval.a0 = outval.a1 = outval_a2 = XP_INFINITY;
    outm = 0;
    return;
  }
  if(inval.a0<=min_inval) {
    // Similar to the note regarding inval.a0 close to max_inval, there
    // are some values with inval.a0 just over min_inval which also
    // evaluate to 0.  The code below will compute the correct non-zero
    // value but then outval * 2^m will evaluate to 0. It is up to the
    // caller to handle that border case.
    outval.a0 = -1.0;
    outval.a1 = outval_a2 = 0.0;
    outm = 0;
    return;
  }
  if(Xp_IsZero(inval.a0)) {
    outval.a0 = outval.a1 = outval_a2 = 0.0;
    outm = 0;
    return;
  }
  // Break x = m*log2 + r.  This reduction is similar to the reduction
  // mod 2.pi needed in the SinCos routine, except here the input
  // range is restricted to (-745,710) or (-11406,11357); either way,
  // it suffices to extend the log(2) representation to three values.
  XP_DDFLOAT_TYPE m = XP_FLOOR(0.5 + inval.a0/Xp_DoubleDouble::hires_log2[0]);
  Xp_DoubleDouble r = inval;
  XP_DDFLOAT_TYPE r_a2;
  XP_DDFLOAT_TYPE tr0a,tr0b,tr1a,tr1b,tr2;
  Xp_DoubleDouble::TwoProd(m,Xp_DoubleDouble::hires_log2[0],tr0a,tr0b);
  Xp_DoubleDouble::TwoProd(m,Xp_DoubleDouble::hires_log2[1],tr1a,tr1b);
  tr2 = m*Xp_DoubleDouble::hires_log2[2];
  r.a0 -= tr0a; // 1/2 <= |r.a0/tr0| <= 2, so this difference is exact
  Xp_DoubleDouble::TwoSum(tr0b,tr1a,tr0b,tr1a);
  tr2 += tr1a + tr1b;
  Xp_DoubleDouble::TwoSum(r.a1,-tr0b,r.a1,r_a2);
  Xp_DoubleDouble::TwoSum(r.a0,r.a1,r.a0,r.a1);
  r_a2 -= tr2;
  // OrderedTwoSums are needed to shift zeros in r, which occur if
  // inval is close to an integral multiple of log(2).
  Xp_DoubleDouble::OrderedTwoSum(r.a1,r_a2,r.a1,r_a2);
  Xp_DoubleDouble::OrderedTwoSum(r.a0,r.a1,r.a0,r.a1);
  assert(fabs(r.a0)>=fabs(r.a1) && fabs(r.a1)>=fabs(r_a2));
  assert(fabs(double(r.a0)) < 0.347 && m == int(m));
  // At this point |r| should be <= 0.5 log(2) ~= 0.35.  If not,
  // import x is too big and what you have is just rounding error.  If
  // m != int(m), then m is too big to fit in an int, and the LdExp
  // won't do want you want.  Both of these concerns should be handled
  // by the range check above.

  // Scale to range for fast series convergence.
  int kreduction = 0;
#if XP_DDFLOAT_MANTISSA_PRECISION<=53
  while(fabs(double(r.a0)) > 16*0.0034) {
    r.a0 /= 16;   r.a1 /= 16; r_a2 /= 16;
    kreduction += 4;
  }
  while(fabs(double(r.a0)) > 0.0034) {
    r.a0 /= 2;   r.a1 /= 2; r_a2 /= 2;
    ++kreduction;
  }
#elif XP_DDFLOAT_MANTISSA_PRECISION<=64
  while(fabs(double(r.a0)) > 16*0.00075) {
    r.a0 /= 16;   r.a1 /= 16; r_a2 /= 16;
    kreduction += 4;
  }
  while(fabs(double(r.a0)) > 0.00075) {
    r.a0 /= 2;   r.a1 /= 2; r_a2 /= 2;
    ++kreduction;
  }
#else
# error Xp_DoubleDouble::Exp() not designed for precisions over 129 bits.
#endif

  Xp_DoubleDouble sum;
  XP_DDFLOAT_TYPE sum_a2;
  if(fabs(r.a0)
     < XP_DDFLOAT_EPSILON*XP_DDFLOAT_EPSILON*XP_DDFLOAT_EPSILON/8.0) {
    // If |r| is very small, then only the first term of the exp(r)-1
    // series has any impact:
    sum = r;  sum_a2 = r_a2;
  } else {
    // Otherwise, use the sinh series (see NOTES VI, 26-Nov-2012, p118).
    // Assume it suffices to include terms up through r^12.  This gives
    // a few extra digits of accuracy that help push down rounding
    // error. (Error bound is at best 0.5 ULP + series truncation
    // error.)
    Xp_DoubleDouble rsq;
    XP_DDFLOAT_TYPE rsq_a2;
    Xp_DoubleDouble::SloppySquare(r.a0,r.a1,r_a2,rsq.a0,rsq.a1,rsq_a2);
    // First few terms can be computed at single-double precision
    // without affecting final accuracy.
    sum.a0 = rsq.a0/(10.*11.);  sum.a1 = 0.0;
    sum.a0 += 1.0;    sum.a0 *= rsq.a0;  sum.a0 /= (8.*9.);
    sum += 1.0;    sum *= rsq;  sum /= (6.*7.);
    sum += 1.0;    sum *= rsq;  sum /= (4.*5.);
    sum += 1.0;    sum *= rsq;  sum /= (3.*2.);
    // At the end use triple-double to wring out best accuracy.
    Xp_DoubleDouble::SloppyProd(sum.a0,sum.a1,0.0,
                                r.a0,r.a1,r_a2,
                                sum.a0,sum.a1,sum_a2);
    Xp_DoubleDouble::ThreeSum(sum.a0,sum.a1,sum_a2,
                              r.a0,r.a1,r_a2,
                              sum.a0,sum.a1,sum_a2);

    // exp(r) - 1 = sinh(r) + sinh^2(r)/(1+sqrt(1+sinh^2(r)))
    Xp_DoubleDouble t1;
    XP_DDFLOAT_TYPE t1_a2;
    Xp_DoubleDouble::SloppySquare(sum.a0,sum.a1,sum_a2,
                                  t1.a0,t1.a1,t1_a2);
    Xp_DoubleDouble t2 = t1 + 1.0;   t2 = sqrt(t2);  t2 += 1.0;
    Xp_DoubleDouble Q = t1/t2;   // First approximate to quotient
    XP_DDFLOAT_TYPE Q_a2 = t1_a2/t2.a0;
    // Do one Newton step to improve quotient estimate
    Xp_DoubleDouble T; XP_DDFLOAT_TYPE T_a2;
    Xp_DoubleDouble::SloppySquare(Q.a0,Q.a1,Q_a2,
                                  T.a0,T.a1,T_a2);
    Xp_DoubleDouble::ThreeSum(T.a0,T.a1,T_a2,
                              2*Q.a0,2*Q.a1,2*Q_a2,
                              T.a0,T.a1,T_a2);
    assert(t1.a0 == T.a0);
    t1.a1 -= T.a1; // t1.a0 - T.a0 should be zero
    t1_a2 -= T_a2;
    Q_a2 += (t1.a1+t1_a2)/(2*(1+Q.a0));

    Xp_DoubleDouble::ThreeSum(sum.a0,sum.a1,sum_a2,
                              Q.a0,Q.a1,Q_a2,
                              sum.a0,sum.a1,sum_a2);
  }

  // Unscale
  for(int k=0;k<kreduction;++k) {
    XP_DDFLOAT_TYPE z0,z1,z2;
    Xp_DoubleDouble::SloppySquare(sum.a0,sum.a1,sum_a2,
                                  z0,z1,z2);
    Xp_DoubleDouble::ThreeSum(2*sum.a0,2*sum.a1,2*sum_a2,
                              z0,z1,z2,
                              sum.a0,sum.a1,sum_a2);
  }

  // Return for further processing
  outval = sum;
  outval_a2 = sum_a2;

  outm = int(m);
  return;
}

Xp_DoubleDouble expm1
(const Xp_DoubleDouble& x)
{ // Computes exp(x)-1 via power series.
  // See NOTES VI, 26-Nov-2012, p118-120.

  // BTW, with double-double arithmetic exp1m() is not as necessary
  // as on contiguous arithmetic systems, because 1 + small_bit only
  // loses one half of precision on small_bit --- the lower half of
  // the double-double doesn't have to lie adjacent to 1.0. In other
  // words [exp(tiny) - 1.0] is not fully useless in a double-double
  // system.
  Xp_DoubleDouble y;
  XP_DDFLOAT_TYPE y_a2;
  int m;

#if XP_RANGE_CHECK
  if(!Xp_IsFinite(x.a0)) { // ExpBase doesn't handle NaNs
    Xp_DoubleDouble result;
    if(Xp_IsNegInf(x.a0)) {
      result.a0 = -1.0; result.a1 = 0.0;
    } else {
      result.a0 = result.a1 = x.a0;
    }
    return result;
  }
#endif // XP_RANGE_CHECK

  Xp_DoubleDouble::ExpBase(x,y,y_a2,m);
  if(m==0) { return y; }

  if(m > 2*XP_DDFLOAT_MANTISSA_PRECISION + 20) {

    // Return from ExpBase is (y+1)*2^m - 1.  In this case we know
    // that the final -1 can be added to the bottom word of the result
    // without effecting the upper word, so we don't need an extended
    // sum for that part.  (If the bottom word is adjacent to the
    // upper, then the -1 will have no effect, but if they are spaced
    // then the -1 can produce a very minor adjustment; whether to do
    // this or not is largely a matter of taste.)  The "+20" governs
    // how close to 0.5 ULP we want the result to be --- for
    // transcendental functions we can't guarantee error <= 0.5 ULP.
    // The best we can do in general is 0.5 + 2^(-n) ULP, where the
    // "n" is bought by performing the computations with roughly n
    // extra bits.
    //   Second point: m>1 so we can subtract one from m to protect
    // against overflow (in case m = XP_DDFLOAT_HUGE_EXP) and not
    // worry about underflow.
    Xp_DoubleDouble::ThreeIncrement(y.a0,y.a1,y_a2,1.0);
    const XP_DDFLOAT_TYPE shavedscale
      = Xp_DoubleDouble::LdExp(XP_DDFLOAT_TYPE(1.0),m-1);
#if 1
    y.a0 *= 2;
    y.a1 *= shavedscale;
    y.a0 *= shavedscale;
    y.a1 = 2*y.a1  - 1.0;
#else
    Xp_DoubleDouble::Rescale(2*y.a0,2*y.a1,shavedscale,y.a0,y.a1);
    y.a1 -= 1.0;
#endif
#if XP_RANGE_CHECK
    if(!Xp_IsFinite(y.a0)) { y.a1 = y.a0; }
#endif
    return y;
  }

  const XP_DDFLOAT_TYPE scale
    = Xp_DoubleDouble::LdExp(XP_DDFLOAT_TYPE(1.0),m);

  if(m < -XP_DDFLOAT_MANTISSA_PRECISION-1) {
    // Similar to above stanza, but in this case the -1 dominates,
    // which in particular means we only need the hi word of y+1
    y += 1;  // double-double add
    y.a1 = y.a0 * scale;
    y.a0 = -1.0;
    return y;
  }

#if XP_DDFLOAT_MANTISSA_PRECISION + 1 >= XP_DDFLOAT_HUGE_EXP
# error Unsupported floating-point format
#endif

  // Otherwise |m| is moderate.  Compute ((1-2^-m) + y) * 2^m, which
  // requires only a single triple-sum.
  Xp_DoubleDouble adj(XP_DDFLOAT_TYPE(1.0),
                      -1.0/scale); // Overflow not possible
  Xp_DoubleDouble::ThreeSum(y.a0,y.a1,y_a2,adj.a0,adj.a1,0.0,
                            y.a0,y.a1,y_a2);
  // y_a2 not needed beyond this point.
  y.a0 *= scale;  // Overflow not possible
  y.a1 *= scale;

  return y;
}

Xp_DoubleDouble exp
(const Xp_DoubleDouble& x)
{ // Computes exp(*this) via power series.
  // See NOTES VI, 26-Nov-2012, p118-120.
  Xp_DoubleDouble y;
  XP_DDFLOAT_TYPE y_a2;
  int m;

#if XP_RANGE_CHECK
  if(!Xp_IsFinite(x.a0)) { // ExpBase doesn't handle NaNs
    Xp_DoubleDouble result;
    if(Xp_IsNegInf(x.a0)) {
      result.a0 = result.a1 = 0.0;
    } else {
      result.a0 = result.a1 = x.a0;
    }
    return result;
  }
#endif // XP_RANGE_CHECK

  Xp_DoubleDouble::ExpBase(x,y,y_a2,m);
  // If result is finite, then -0.293 < y < 0.42
  // and XP_DDFLOAT_TINY_EXP - XP_DDFLOAT_MANTISSA_PRECISION
  //     <=  m  <= XP_DDFLOAT_HUGE_EXP

#if XP_RANGE_CHECK
  if(m == XP_DDFLOAT_VERYTINY_EXP - 1 && y.a0>0.0) {
    // Result is less than XP_DDFLOAT_VERYTINY, but should properly
    // round up to XP_DDFLOAT_VERYTINY.  (See discussion on the range
    // of return values in ExpBase().)
    y.a0 = XP_DDFLOAT_VERYTINY; y.a1 = 0.0;
    return y;
  }
#endif
  if(Xp_IsFinite(y.a0)) { // Protect exact rounding code from infinities
    Xp_DoubleDouble::ThreeIncrement(y.a0,y.a1,y_a2,1.0);
    // y_a2 not needed beyond this point.
#if XP_RANGE_CHECK
    if(m<XP_DDFLOAT_HUGE_EXP) {
      if(m >= XP_DDFLOAT_TINY_EXP+2+XP_DDFLOAT_MANTISSA_PRECISION+14) {
        // The "14" allows for non-adjacency of y.a0 and y.a1.  See
        // discussion in the "else" branch.
        XP_DDFLOAT_TYPE scale = Xp_DoubleDouble::LdExp(1.0,m);
        y.a0 *= scale;   y.a1 *= scale;
      } else {
        // Underflow rounding error can occur if the underflow occurs
        // between components y.a0 + y.a1 + y_a2.  Protect against
        // this by extracting the underflow rounding error (if any)
        // and putting it back in as a single word.
        //   One complication is that division by "scale" will NaN if
        // 2^m evaluates to 0.  Protect against that by prescaling
        // by 2^MANTISSA_PRECISION.
        //  Another complication is that if y.a0 and y.a1 are not
        // adjacent but have a string of zero between them, then
        // underflow rounding error can occur at magnitudes for y.a0
        // above what one might initially expect.  In this case the
        // error will be less than 0.5 ULP (divide by two for each
        // "extra" zero between y.a0 and y.a1), but if we want to
        // insure the total error for this transendental is less than
        // 0.5001 then we need to allow for up to 14 extra zeros (see
        // m check above).
        XP_DDFLOAT_TYPE scale
          = Xp_DoubleDouble::LdExp(1.0,m+XP_DDFLOAT_MANTISSA_PRECISION);
        Xp_DoubleDouble::Rescale(y.a0*1.0/XP_DDFLOAT_POW_2_MANTISSA,
                y.a1*1.0/XP_DDFLOAT_POW_2_MANTISSA,
                y_a2*1.0/XP_DDFLOAT_POW_2_MANTISSA,
                scale,y.a0,y.a1);
      }
    } else {
      y.a0 = Xp_DoubleDouble::LdExp(y.a0,m);
      if(Xp_IsFinite(y.a0)) {
        y.a1 = Xp_DoubleDouble::LdExp(y.a1,m);
      } else {
        y.a1 = y.a0;
      }
    }
#else // XP_RANGE_CHECK
    XP_DDFLOAT_TYPE scale = Xp_DoubleDouble::LdExp(1.0,m);
    y.a0 *= scale;   y.a1 *= scale;
#endif // XP_RANGE_CHECK
  }
  return y;
}

Xp_DoubleDouble atan(const Xp_DoubleDouble& inval)
{ // Compute arctan using SinCos() + one step of Newton's method
  // Handle extremes
#if XP_RANGE_CHECK
  if(Xp_IsNaN(inval.a0)) {
    return XP_NAN;
  }
#endif // XP_RANGE_CHECK
#if XP_DDFLOAT_MANTISSA_PRECISION <= 64
  if(!(-1e40 <= inval.a0 && inval.a0 <= 1e40)) {
    // Crazy test construction above captures non-finite cases (see
    // Xp_IsFinite)
    Xp_DoubleDouble result;
    result.a0 = 0.5 * Xp_DoubleDouble::hires_pi[0];
    result.a1 = 0.5 * Xp_DoubleDouble::hires_pi[1];
    if(!Xp_IsFinite(inval.a0)
       && !Xp_IsPosInf(inval.a0) && !Xp_IsNegInf(inval.a0)) {
      result.a1 = result.a0 = XP_NAN;
      return result;
    }
    if(inval.a0 < -1e40) {
      result.a0 *= -1.0;
      result.a1 *= -1.0;
    }
    return result;
  }
#else
#  error Unsupported mantissa precision
#endif

  // SMALL_CHECK is pow(0.5,(p+15)/6), where p is the double-double
  // precision, i.e., 2*XP_DDFLOAT_MANTISSA_PRECISION+1.
#if XP_DDFLOAT_MANTISSA_PRECISION <= 53
  const XP_DDFLOAT_TYPE SMALL_CHECK = 9.53674316e-7;
#elif XP_DDFLOAT_MANTISSA_PRECISION <= 64
  const XP_DDFLOAT_TYPE SMALL_CHECK = 5.96046448e-8;
#else
#  error Unsupported mantissa precision
#endif
  if(fabs(inval.a0) < SMALL_CHECK) {
    // For very small inputs, use the first couple of terms in the
    // atan(x) series, x - x^3/3 + x^5/5.  This is faster and more
    // accurate than the normal case below.
    Xp_DoubleDouble xsq, sum;
    XP_DDFLOAT_TYPE sum_a2 = 0.0;
    xsq = inval;  xsq.Square();
    sum = 3.*xsq - 5.;
    sum *= xsq;
    sum /= 15.;
    Xp_DoubleDouble::ThreeIncrement(sum.a0,sum.a1,sum_a2,1.0);
    Xp_DoubleDouble::SloppyProd(sum.a0,sum.a1,sum_a2,
                                inval.a0,inval.a1,0.0,
                                sum.a0,sum.a1,sum_a2);
    return sum;
  }

  // Normal case, using modified Halley.
  // For details, see NOTES VI, 17-Feb-2013, pp 138-140.
  bool flip=0;
  Xp_DoubleDouble offset(0.0);
  Xp_DoubleDouble sinx0, cosx0;
  XP_DDFLOAT_TYPE x0,offset_a2(0.0), sinx0_a2, cosx0_a2;
  if(fabs(inval.a0)<=1.0) {
    // The "1.0" limit could be bumped up a good bit and still retain
    // full double-double accuracy.
    x0 = atan(inval.a0);
    Xp_DoubleDouble::SinCos(Xp_DoubleDouble(x0),
                            sinx0,sinx0_a2,cosx0,cosx0_a2);
  } else {
    // Compute complementary value; see NOTES VI, Errata, 18-Feb-2013,
    // p 140.
    flip = 1;
    x0 = atan(1.0/inval.a0);
    if(inval.a0>0.0) {
      offset.a0 = 0.5*Xp_DoubleDouble::hires_pi[0];
      offset.a1 = 0.5*Xp_DoubleDouble::hires_pi[1];
      offset_a2 = 0.5*Xp_DoubleDouble::hires_pi[2];
    } else {
      offset.a0 = -0.5*Xp_DoubleDouble::hires_pi[0];
      offset.a1 = -0.5*Xp_DoubleDouble::hires_pi[1];
      offset_a2 = -0.5*Xp_DoubleDouble::hires_pi[2];
    }
    Xp_DoubleDouble::SinCos(Xp_DoubleDouble(x0),
                            cosx0,cosx0_a2,sinx0,sinx0_a2);
    x0 *= -1;
  }

  // In the above, we compute Sin and Cos with somewhat better than
  // double-double accuracy, so we can compute "delta" below to full
  // double-double accuracy.

  // Newton step.  To get full accuracy, the computations inside "delta"
  // need to be done with slightly better than double-double accuracy.
  // After that "adjA" only needs to carry double-double accuracy.
  Xp_DoubleDouble delta;
  XP_DDFLOAT_TYPE delta_a2;
  Xp_DoubleDouble::SloppyProd(inval.a0,inval.a1,0,
                              cosx0.a0,cosx0.a1,cosx0_a2,
                              delta.a0,delta.a1,delta_a2);
  Xp_DoubleDouble::ThreeSum(delta.a0,delta.a1,delta_a2,
                            -sinx0.a0,-sinx0.a1,-sinx0_a2,
                            delta.a0,delta.a1,delta_a2);
  /// There is significant cancellation in the above ThreeSum.

  Xp_DoubleDouble adjA;
  adjA = delta*cosx0;  // Double-double multiplication

  // Halley adjustment.  This only needs single-double accuracy.
  XP_DDFLOAT_TYPE adjB;
  adjB = -adjA.a0*delta.a0*sinx0.a0;

  // Add everything up
  adjA += adjB;
  Xp_DoubleDouble result;
  if(!flip) {
    Xp_DoubleDouble::OrderedTwoSum(x0,adjA.a0,result.a0,result.a1);
    result.a1 += adjA.a1;
  } else {
    // A little extra care is needed in the |A|>1 case to carry full
    // accuracy through the sum with +/- pi/2.
    XP_DDFLOAT_TYPE result_a2;
    Xp_DoubleDouble::OrderedTwoSum(x0,adjA.a0,result.a0,result.a1);
    Xp_DoubleDouble::OrderedTwoSum(result.a1,adjA.a1,
                                   result.a1,result_a2);
    Xp_DoubleDouble::ThreeSum(offset.a0,offset.a1,offset_a2,
                              result.a0,result.a1,result_a2,
                              result.a0,result.a1,result_a2);
  }

  return result;
}

Xp_DoubleDouble atan2(const Xp_DoubleDouble& y,const Xp_DoubleDouble& x)
{ // NB: Less than full accuracy; to get full accuracy would involve
  // breaking into the one argument atan function and work with
  // triple-double values.

#if XP_RANGE_CHECK
  if(Xp_IsNaN(y.a0) || Xp_IsNaN(x.a0)) {
    return XP_NAN;
  }
#endif // XP_RANGE_CHECK

  // Handle corner cases first
  if(y.a0 == 0.0 && x.a0 == 0.0) { return Xp_DoubleDouble(0.0); }
  if(Xp_IsZero(y.a0)) {
    if(x.a0>0.0) return y;  // Returns signed zero
    return Xp_DoubleDouble::DD_PI;
  }
  if(x.a0 == 0.0) {
    if(y.a0>0.0) return Xp_DoubleDouble::DD_HALFPI;
    return -1*Xp_DoubleDouble::DD_HALFPI;
  }

  if(fabs(y.a0)<=fabs(x.a0)) { // Division won't be cranky
    Xp_DoubleDouble result = atan(y/x);
    if(x.a0>0.0) return result;  // atan answers in right quadrant
    if(y.a0>0.0) return result + Xp_DoubleDouble::DD_PI; // x<0, y>0
    return              result - Xp_DoubleDouble::DD_PI; // x<0, y<0
  }

  // Otherwise do inverse division and subtract from (+/-)pi/2
  Xp_DoubleDouble base = Xp_DoubleDouble::DD_HALFPI;
  if(y.a0<0.0) {
    base.a0 *= -1;   base.a1 *= -1;
  }
  return base -  atan(x/y);
}

Xp_DoubleDouble log1p(const Xp_DoubleDouble& inval)
{ // Use Newton/Halley with double-double exp to compute log(1+x)
  // See NOTES VI, 20/21-Feb-2013, p141-142

#if XP_DDFLOAT_MANTISSA_PRECISION >= 53
  const int HEADROOM_FACTOR = 256;
  // This headroom_factor, when used as below, is big enough
  // to guarantee that the difference between log1p(inval) and log(inval)
  // is smaller than 0.0001 ULP if the XP_DDFLOAT_TYPE mantissa width
  // is at least 53 bits.  If additionally the computational+rounding
  // error in log is not more than 0.5 ULP, then replacing log1p(inval)
  // with log(inval) yields and error smaller than 0.50001 ULP.
#else
#  error Unsupported floating point mantissa precision
#endif

  if(!(-1+XP_DDFLOAT_EPSILON<=inval.a0
       && inval.a0<=HEADROOM_FACTOR
                   *XP_DDFLOAT_POW_2_MANTISSA*XP_DDFLOAT_POW_2_MANTISSA)) {
    // The !(... && ...) construct protects against broken NaN handling.
    // Assume Xp_DoubleDouble precision is 2*XP_DDFLOAT_MANTISSA_PRECISION+1
    if(inval.a0 > HEADROOM_FACTOR
                 *XP_DDFLOAT_POW_2_MANTISSA*XP_DDFLOAT_POW_2_MANTISSA) {
      // In this case log(1+inval) is indistguishable from log(inval)
      return log(inval);
    }
    Xp_DoubleDouble scratch;
#if XP_RANGE_CHECK
    if(inval.a0 < -1.0 || (inval.a0 == -1.0 && inval.a1 < 0.0)) {
      scratch.a0 = scratch.a1 = XP_NAN;
      return scratch;
    } else if(!Xp_IsFinite(inval.a0)) { // Presumably +Inf or NaN
      return inval;
    } else if(inval.a0 == -1.0 && inval.a1 == 0.0) {
      scratch.a0 = scratch.a1 = -XP_INFINITY;
      return scratch;
    }
#endif
    if(inval.a0 == -1) {
      // In this case, the code that calculates initial estimate x0
      // yields -Inf, which is incorrect if inval.a1>0.  Rather than
      // trying to rework the log1p() code, pass the problem over to
      // log().
      scratch.a0 = inval.a1;
      scratch.a1 = 0.0;
      return log(scratch);
    }
  }

  // Initial guess
  XP_DDFLOAT_TYPE x0 = 1.0 + inval.a0; // Standard trick to get accurate
  XP_DDFLOAT_TYPE y0 = x0 - 1.0;       // value for log(1+a)
  if(y0 != 0.0) { x0 = log(x0)/y0; x0 *= inval.a0; }
  else          { x0 = inval.a0; }

  // Newton correction
  Xp_DoubleDouble tmp,h1;
  XP_DDFLOAT_TYPE tmp_a2,h1_a2;
  int m;

  Xp_DoubleDouble::ExpBase(Xp_DoubleDouble(-x0),tmp,tmp_a2,m);

  if(m==0) {
    Xp_DoubleDouble::SloppyProd(inval.a0,inval.a1,0.0,
                                tmp.a0,tmp.a1,tmp_a2,
                                h1.a0,h1.a1,h1_a2);
    Xp_DoubleDouble::ThreeSum(h1.a0,h1.a1,h1_a2,
                              tmp.a0,tmp.a1,tmp_a2,
                              h1.a0,h1.a1,h1_a2);
    Xp_DoubleDouble::ThreeSum(h1.a0,h1.a1,h1_a2,
                              inval.a0,inval.a1,0.0,
                              h1.a0,h1.a1,h1_a2);
  } else {

#if 2*XP_DDFLOAT_MANTISSA_PRECISION+1 > XP_DDFLOAT_HUGE_EXP
# error Unsupported floating point type
#endif

    // The XP_RANGE_CHECK block insures that |m| < XP_DDFLOAT_HUGE_EXP
    XP_DDFLOAT_TYPE scale = Xp_DoubleDouble::LdExp(1.0,m);
    h1.a0 = inval.a0*scale; h1.a1 = inval.a1*scale; h1_a2 = 0.0;
    Xp_DoubleDouble::ThreeIncrement(tmp.a0,tmp.a1,tmp_a2,1.0);
    Xp_DoubleDouble::ThreeIncrement(h1.a0,h1.a1,h1_a2,scale);

    Xp_DoubleDouble::SloppyProd(h1.a0,h1.a1,h1_a2,
                                tmp.a0,tmp.a1,tmp_a2,
                                h1.a0,h1.a1,h1_a2);
    Xp_DoubleDouble::ThreeIncrement(h1.a0,h1.a1,h1_a2,-1.0);
  }

  // Halley correction
  XP_DDFLOAT_TYPE h2 = h1.a0*h1.a0;
  Xp_DoubleDouble::TwoSum(h1.a0,-0.5*h2,h1.a0,h2);
  h1.a1 += h2;

  // Add corrections to initial guess
  Xp_DoubleDouble::TwoSum(h1.a0,x0,h1.a0,x0);
  h1.a1 += x0;
  return h1;
}

Xp_DoubleDouble log(const Xp_DoubleDouble& inval)
{ // Use Newton/Halley with double-double exp to compute log(x)
  // See NOTES VI, 20/21-Feb-2013, p141-142

#if XP_RANGE_CHECK
  if(!(XP_DDFLOAT_MIN<=inval.a0 && inval.a0<=XP_DDFLOAT_MAX)) {
    if(inval.a0 < XP_DDFLOAT_TYPE(0.0)) {
      Xp_DoubleDouble result;
      result.a0 = result.a1 = XP_NAN;
      return result;
    } else if(!Xp_IsFinite(inval.a0)) { // Presumably +Inf or NaN
      return inval;
    } else if(inval.a0 == XP_DDFLOAT_TYPE(0.0)) {
      Xp_DoubleDouble result;
      result.a0 = result.a1 = -XP_INFINITY;
      return result;
    }

    Xp_DoubleDouble h1;
    XP_DDFLOAT_TYPE h1_a2, x0, ia0;
    int m;

    // Otherwise, inval.a0 is so small that we will overflow when we
    // compute the Newton correction.  OTOH, we also know that
    // inval.a1 must be zero.
    ia0 = inval.a0 * XP_DDFLOAT_POW_2_MANTISSA;
    x0 = log(ia0);
    Xp_DoubleDouble::ExpBase(Xp_DoubleDouble(-x0),h1,h1_a2,m);
    ia0 = Xp_DoubleDouble::LdExp(ia0,m);

    // Note: The following is (h1+1)*ia0 - 1.0.  It is possible
    // to change that to h1*ia0 + (-1., ia0) (where -1 and ia0
    // are guaranteed to not overlap), which has one less ThreeSum,
    // but in testing the current code seems to be a little faster.
    // Similar contractions are available in the Halley correction
    // section that follows, but in testing those contractions don't
    // speed up this code block and (for mysterious reasons) do slow
    // down the outer "in-range" code.  Go figure.
    Xp_DoubleDouble::ThreeIncrement(h1.a0,h1.a1,h1_a2,1.0);

    Xp_DoubleDouble::SloppyProd(h1.a0,h1.a1,h1_a2,
                                ia0,0.0,0.0,
                                h1.a0,h1.a1,h1_a2);

    Xp_DoubleDouble::ThreeIncrement(h1.a0,h1.a1,h1_a2,-1.0);

    // Halley correction
    XP_DDFLOAT_TYPE h2 = h1.a0*h1.a0;
    Xp_DoubleDouble::ThreeSum(h1.a0,h1.a1,h1_a2,
                              -Xp_DoubleDouble::hires_log2_mant[0],
                              -Xp_DoubleDouble::hires_log2_mant[1],
                              -Xp_DoubleDouble::hires_log2_mant[2],
                              h1.a0,h1.a1,h1_a2);

    Xp_DoubleDouble::ThreeIncrement(h1.a0,h1.a1,h1_a2,-0.5*h2);

    // Add corrections to initial guess
    Xp_DoubleDouble::ThreeIncrement(h1.a0,h1.a1,h1_a2,x0);

    return h1;
  }
#endif // XP_RANGE_CHECK

  Xp_DoubleDouble h1;
  XP_DDFLOAT_TYPE h1_a2;
  int m;

  // Initial guess
  XP_DDFLOAT_TYPE x0 = log(inval.a0);

  // Newton correction
  Xp_DoubleDouble::ExpBase(Xp_DoubleDouble(-x0),h1,h1_a2,m);
  // Range checks above mean that -HUGE_EXP <= m <= -TINY_EXP,
  // which means that pow(2,m) is representable --- but
  // make sure the computation of pow(2,m) doesn't flush to
  // zero for m<TINY_EXP
  XP_DDFLOAT_TYPE scale = Xp_DoubleDouble::LdExp(1.0,m);
  XP_DDFLOAT_TYPE ia0 = inval.a0*scale;
  XP_DDFLOAT_TYPE ia1 = inval.a1*scale;

  Xp_DoubleDouble::ThreeIncrement(h1.a0,h1.a1,h1_a2,1.0);
  Xp_DoubleDouble::SloppyProd(h1.a0,h1.a1,h1_a2,
                              ia0,ia1,0.0,
                              h1.a0,h1.a1,h1_a2);

  XP_DDFLOAT_TYPE scratch;
  Xp_DoubleDouble::TwoSum(h1.a0,-1.0,h1.a0,scratch);
  h1.a1 += scratch;
  Xp_DoubleDouble::OrderedTwoSum(h1.a0,h1.a1,h1.a0,h1.a1);
  // If some small error shows up, try replacing h1.a1 += scratch
  // above with something like TwoSum(h1.a1,scratch,h1.a1,scratch)
  // and then h1_a2 += scratch.
  h1.a1 += h1_a2;

  // Halley correction
  XP_DDFLOAT_TYPE h2 = h1.a0*h1.a0;
  Xp_DoubleDouble::TwoSum(h1.a0,-0.5*h2,h1.a0,h2);
  h1.a1 += h2;

  // Add corrections to initial guess
  Xp_DoubleDouble::TwoSum(h1.a0,x0,h1.a0,x0);
  h1.a1 += x0;

  return h1;
}


//////////////////////////////////////////////////////////////////////////
// Xp version of Nb_FindRatApprox (q.v.) for finding rational
// approximations.  Fills exports p and q with best result, and returns
// 1 or 0 according to whether or not p/q meets the specified relative
// error.
int Xp_FindRatApprox
(const Xp_DoubleDouble& xin,const Xp_DoubleDouble& yin,
 XP_DDFLOAT_TYPE relerr, XP_DDFLOAT_TYPE maxq,
 Xp_DoubleDouble& p,Xp_DoubleDouble& q)
{ // Uses Euclid's algorithm to find rational approximation p/q to x/y
  Xp_DoubleDouble x = xin;
  Xp_DoubleDouble y = yin;

  int sign = 1;
  if(x<0) { x *= -1;  sign *= -1; }
  if(y<0) { y *= -1;  sign *= -1; }

  int swap = 0;
  if(x<y) { Xp_DoubleDouble t = x; x = y; y = t; swap = 1; }

  const Xp_DoubleDouble x0 = x;
  const Xp_DoubleDouble y0 = y;

  Xp_DoubleDouble p0 = 0;  Xp_DoubleDouble p1 = 1;
  Xp_DoubleDouble q0 = 1;  Xp_DoubleDouble q1 = 0;

  XP_DDFLOAT_TYPE m;
  (x/y).DownConvert(m);  m = XP_FLOOR(m);
  Xp_DoubleDouble r = x - m*y;
  Xp_DoubleDouble p2 = m*p1 + p0;
  Xp_DoubleDouble q2 = m*q1 + q0;
  while(q2.Hi()<maxq && fabs(x0*q2 - p2*y0) > relerr*x0*q2) {
    x = y;
    y = r;
    (x/y).DownConvert(m);  m = XP_FLOOR(m);
    r = x - m*y;
    p0 = p1; p1 = p2; p2 = m*p1 + p0;
    q0 = q1; q1 = q2; q2 = m*q1 + q0;
  }

  if(!swap) {
    p = p2;  q = q2;
  } else {
    p = q2;  q = p2;
  }
  return (fabs(x0*q2 - p2*y0) <= relerr*x0*q2);
}

// SSE implementation //////////////////////////////////////////////////
#else // XP_DD_USE_SSE
// Awaiting construction...
# error SSE-version of Xp_DoubleDouble not implemented
#endif // XP_DD_USE_SSE

////////////////////////////////////////////////////////////////////////
// Extra code (just for fun)
// This code is not compiled in, but preserved here in case it finds
// some use in the future.
//

#if 0 // EXTRA_CODE
// The method used to compute log(Xp_DoubleDouble) above is based on
// Newton's method + Taylor series for exp() (or, actually, sinh()).  An
// alternative approach, shown below, computes log directly via the
// Arithmetic-Geometric Mean (AGM) method.  See Richard P. Brent and
// Paul Zimmerman, "Modern Computer Arithmetic," 2003-2009, and papers
// by Sasaki and Kanada.  This approach is interesting especially for
// high precision with large inputs.
Xp_DoubleDouble Log_AGM(const Xp_DoubleDouble& inval) {
  // log(x) \approx pi/[2.M(1,2^(2-m)/x)] - m.log(2)
  // want x.2^m > 2^(p/2), with precision 1/2^p.

  // See NOTES VI, 26-Nov-2012, p122-124.

  Xp_DoubleDouble outval;
  XP_DDFLOAT_TYPE& a0 = outval.a0;  a0 = inval.a0;
  XP_DDFLOAT_TYPE& a1 = outval.a1;  a1 = inval.a1;

  // Special case check
  if(a0<=0.0) {
    cerr << "ERROR: a0=" << a0 << ", a1=" << a1<< "\n";
    throw("Domain error in Xp_DoubleDouble::log()");
  }
  if(a0 == 1.0 && Xp_IsZero(a1)) {
    a0 = a1; // Propagates signed zero
    return outval;
  }

  // Sign adjustment; code below wants xt>1
  XP_DDFLOAT_TYPE sign_adjustment = 1;
  Xp_DoubleDouble xt = outval;
  if(a0<1.0) {
    xt = Recip(xt);
    sign_adjustment = -1;
  }

  // Square a few times, until xt > 2^(p/36).  Limit squaring to
  // max_square_count times.  Otherwise precision lost in squaring
  // operation can dominate error.
  const int max_square_count = 12;
  const XP_DDFLOAT_TYPE checkpower = (2*XP_DDFLOAT_MANTISSA_PRECISION+2)/49.;
  const XP_DDFLOAT_TYPE magcheck = Xp_DoubleDouble::LdExp(XP_DDFLOAT_TYPE(1.0),
                                                     int(XP_FLOOR(checkpower)))
    *(1. + checkpower - XP_FLOOR(checkpower));
  // magcheck is cheap upper estimate to pow(2,checkpower)
  // NOTE: For production use, replace the above expressions with
  // literal values, to insure that the values aren't computed each
  // time through.

  int square_count = 0;
  while(xt.a0 < magcheck && square_count<max_square_count) {
    xt.Square();    // Note: Each squaring doubles the error. May
    ++square_count; // want to shift to triple-double accuracy.
  }

  // Compute AGM start points, [theta_2(q^4)]^2 and [theta_3)q^4)]^2;
  Xp_DoubleDouble q = Recip(xt);
  Xp_DoubleDouble q4 = q; q4.Square();  q4.Square();
  Xp_DoubleDouble q8 = q4; q8.Square();

  Xp_DoubleDouble theta2 = q8;
  theta2.Square(); theta2 += 1.; theta2 *= q8;
  theta2 += 1.; theta2 *= q;
  theta2.a0 *= 2;  theta2.a1 *= 2;

#if 0
  Xp_DoubleDouble theta3 = q8;
  theta3 *= q4; theta3 += 1.;
  theta3 *= q4;
  theta3.a0 *= 2;  theta3.a1 *= 2;
  theta3 += 1.0;
#else
  Xp_DoubleDouble q12 = q8; q12 *= q4;
  Xp_DoubleDouble theta3 = q12;
  theta3 *= q8; theta3 += 1.;
  theta3 *= q12; theta3 += 1.;
  theta3 *= q4;
  theta3.a0 *= 2;  theta3.a1 *= 2;
  theta3 += 1.0;
#endif

  // There are a few shortcuts on the first iteration.
  Xp_DoubleDouble xa = theta2; xa += theta3; xa.Square();
  Xp_DoubleDouble ya = theta2; ya *= theta3; ya.a0 *= 2; ya.a1 *= 2;
  xa -= ya;
  Xp_DoubleDouble xb, yb;

  int count = 0;
  while(1) {
    count += 2; // Do two iterations per loop

    xb = xa; xb += ya;  xb *= 0.5;
    yb = xa; yb *= ya;  yb = sqrt(yb);

    xa = xb; xa += yb;  xa *= 0.5;
    ya = xb; ya *= yb;  ya = sqrt(ya);

    xb -= xa; // Note that this renormalizes xb, so
    // xb.a0 ==0 implies xb.a1 is zero too.
    if(xb.a0 == 0.0) break;
    if(count > 200) {
      cerr << "Bad juju; count=" << count
           << "\n x=" << xa << "\n y=" << ya << "\n";
      break;
    }
  }
  a0 = 0.5*Xp_DoubleDouble::hires_pi[0];  // Additional factor of 1/2 is
  a1 = 0.5*Xp_DoubleDouble::hires_pi[1]; /// subsumed into start point xa,ya
  outval /= xa;

  // Adjust scaling and sign
  a0 = sign_adjustment * Xp_DoubleDouble::LdExp(a0,-1*square_count);
  a1 = sign_adjustment * Xp_DoubleDouble::LdExp(a1,-1*square_count);

  return outval;
}
#endif // EXTRA_CODE
