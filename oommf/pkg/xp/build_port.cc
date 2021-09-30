/* FILE: build_port.cc
 *
 * This program examines the compiler environment and writes
 * out a platform specific header for the XP extension, called
 * xpport.h.  One builds and runs this program first, and then
 * compiles the XP C++ source programs, which #include xpport.h to set
 * variable types and other compile-time constants appropriately.
 *
 * It is important that XP C++ source program code be compiled with
 * exactly the same options as build_port.cc.  The compile line should
 * be passed to the build_port executable so it can be recorded inside
 * xpport.h for future reference.  In particular, the compiler options
 * can influence the variable type used for XP_DDFLOAT_TYPE (double or
 * long double).  For example, the g++ option -mfpmath=sse selects SSE
 * for evaluation of double arithmetic.  This arithmetic carries no
 * extra precision and so is compatible with the TwoSum routines, and so
 * XP_FLOAT_TYPE will be set to double.  But if doubledouble.cc were
 * later built with -mfpmath=387, then 387 math would be used inside the
 * TwoSum routines, causing failures because of the extra precision.
 * (If -mfpmath=387 is selected, then XP_FLOAT_TYPE should be long
 * double.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 */

#ifndef XP_USE_MPFR
# define XP_USE_MPFR 0
#endif

#if XP_USE_MPFR && !defined(XP_MPFR_MANTDIG)
// If not specified, use mantissa digits setting that provides
// 128 bits of precision.  This approximates the precision provided
// by two 80-bit long double variables, and runs slightly faster
// than XP_MPFR_MANTDIG 32 (which yields 108 bit mantissa).
# define XP_MPFR_MANTDIG 32
#endif

// Enable long double printf support in MinGW.  The
// __USE_MINGW_ANSI_STDIO macro needs to be set before
// the system header files are #include'd.
#if defined(__MINGW32__) && !defined(__USE_MINGW_ANSI_STDIO)
# define __USE_MINGW_ANSI_STDIO 1
#endif

#include <cassert>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <limits>  // For std::numeric_limits<T>::infinity(), radix

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

/* End includes */

#define xp_stringify(s) xp_tstringify(s)
#define xp_tstringify(s) #s

#if XP_USE_MPFR
// Multiprecision library using namespace boost::multiprecision
# include <boost/version.hpp>
# include <boost/math/special_functions/sign.hpp>
# include <boost/multiprecision/mpfr.hpp>
using namespace boost::multiprecision;
typedef number<mpfr_float_backend<XP_MPFR_MANTDIG> > XP_MPFR;
# define XP_MPFR_STR "number<mpfr_float_backend<" \
  xp_stringify(XP_MPFR_MANTDIG) "> >"
#endif // XP_USE_MPFR

enum XpBaseType {
  XP_INVALID_TYPE=0,
  XP_FLOAT_TYPE=1,
  XP_DOUBLE_TYPE=2, XP_LONGDOUBLE_TYPE=3,
  XP_MPFR_TYPE=4
};
const char* XpBaseTypeString[] =
  { "XP_INVALID_TYPE", "XP_FLOAT_TYPE",
    "XP_DOUBLE_TYPE", "XP_LONGDOUBLE_TYPE",
    "XP_MPFR_TYPE" };
/// NB: Keep this in sync.  For example, XpBaseTypeString[XP_DOUBLE_TYPE]
/// should point to the string "XP_DOUBLE_TYPE".


////////////////////////////////////////////////////////////////////////
// Support code

int PrintDecimalDigits(int precision)
{ // Returns the number of decimal digits needed to exactly specify a
  // binary floating point value of precision bits.  For details, see
  //
  //    "How to print floating point numbers accurately," Guy L. Steele
  //    and Jon L. White, ACM SIGPLAN NOTICES, Volume 39, Issue 4,
  //    372-389 (Apr 2004).  DOI: 10.1145/989393.989431
  //
  // (Also available in Proceedings of the ACM SIGPLAN'SO Conference on
  // Programming Language Design and Implementation, White Plains, New
  // York, June 20-22, 1990, pp 112-126.)
  //
  // The base formula is
  //
  //    B^(N-1) > b^n
  //
  // where n is the number of digits in base b of the source value, B is
  // the target base, and N is the number of digits needed in the target
  // base.  In our case b=2, n=precision, and B=10.  So we get
  //
  //    N = 2 + floor(precision*log10(2))
  //
  if(precision == 53) { // Common case: 8-byte IEEE float
    return 17;
  }
  if(precision == 64) { // Common case: 10-byte IEEE float
    return 21;
  }
  if(precision == 24) { // Common case: 4-byte IEEE float
    return 9;
  }
  return 2 + int(floor(precision*log(2.)/log(10.)));
}


// Template to create a string representation of a value, in scientific
// notation, to the number of requested base-ten digits.
template <class floatType>
const char* NumberToString
(floatType val,int digits, // imports
 std::string& sbuf         // export
) {
  // Use C++ i/o to auto handle floating point variable type
  std::stringbuf strbuf;
  std::ostream ostrbuf(&strbuf);
  ostrbuf << std::scientific << std::setprecision(digits-1) << val;
  sbuf = strbuf.str();
  return sbuf.c_str();
}

// Overload of preceding for case floatType == long double.  This
// version appends an "L" suffix.
const char* NumberToString
(long double val,int digits, // imports
 std::string& sbuf            // export
) {
  // Use C++ i/o to auto handle floating point variable type
  std::stringbuf strbuf;
  std::ostream ostrbuf(&strbuf);
  ostrbuf << std::scientific << std::setprecision(digits-1) << val << "L";
  sbuf = strbuf.str();
  return sbuf.c_str();
}


#if XP_USE_MPFR
// Overload for case floatType == XP_MPFR.
const char* NumberToString
(XP_MPFR val,int digits, // imports
 std::string& sbuf       // export
) {
  // Use C++ i/o to auto handle floating point variable type
  std::stringbuf strbuf;
  std::ostream ostrbuf(&strbuf);
  ostrbuf << "XP_DDFLOAT_TYPE(\""
          << std::scientific << std::setprecision(digits-1) << val
          << "\")";
  sbuf = strbuf.str();
  return sbuf.c_str();
}
#endif // XP_USE_MPFR


unsigned int HasExtraDoublePrecisionTest()
{ // This routine attempts to test for extra precision in intermediate
  // computations.  If this routine returns a non-zero value, then
  // incorrect rounding was detected.  This presumably indicates
  // double-rounding and hence extra intermediate precision.  If the
  // return value is zero, then no incorrect rounding was found.  It
  // is hard to know for certain what optimization compilers may make,
  // so a zero return value is not certain evidence of no intermediate
  // rounding, but it is suggestive of such.
  const int n = DBL_MANT_DIG-1; // Mantissa bit count - 1
  volatile double t = pow(2.0,double(n))-1.0;
  double y = t*pow(2.0,double(-n-1));
  double x = t + pow(2.0,double(52));
  double za =  x + y;  // Various tests here check against different
  double zb = -x - y;  // rounding modes.
  double zc =  x - y;
  unsigned int error = 0;
  volatile double check = x;
  if(za !=  check) error |= 1;
  if(zb != -check) error |= 2;
  if(zc !=  check) error |= 4;
  return error;
}

unsigned int HasExtraDoublePrecision()
{ // Floating point evaluation method:
  //
  // If a compiler is C99 compliant, then it will define the
  // FLT_EVAL_METHOD macro to either 0, 1, or 2, with meanings:
  //
  //   ----------------+-------------+-------------+-------------
  //   FLT_EVAL_METHOD |   float     |   double    | long double
  //   ----------------+-------------+-------------+-------------
  //      < -1         |   implementation-defined behavior
  //        -1         |  unknown    |  unknown    |  unknown
  //         0         |   float     |   double    | long double
  //         1         |   double    |   double    | long double
  //         2         | long double | long double | long double
  //   ----------------+-------------+-------------+-------------
  //
  // The three column headers on the right indicate the variable type,
  // the setting under them indicate the precision use to compute
  // intermediaries.  For Xp_DoubleDouble it is essential that the
  // type specified for XP_DDFLOAT_TYPE be evaluated without any extra
  // precision.  So if FLT_EVAL_METHOD is defined and is equal to 0 or
  // 1, then we can use double for XP_DDFLOAT_TYPE.  Otherwise we need
  // to use long double.  An exception is if SSE is available, in
  // which case SSE evaluation for floats and doubles conforms to
  // FLT_EVAL_METHOD 0, so se can set XP_DDFLOAT_TYPE to double in
  // this setting provided critical operations are performed using SSE
  // arithmetic.
  //
  // Note that under Microsoft Visual C++, long double is the same as
  // double, and all floating point operations are performed at double
  // precision (53-bit mantissa), so XP_DDFLOAT_TYPE can be safely set
  // to either double or long double with that compiler.
  //
  // The HasExtraDoublePrecisionTest() routine attempts to detect
  // intermediate extra precision in double computations.  A non-zero
  // return from this function should be conclusive evidence of extra
  // intermediate precision.  A zero return is suggestive, if not
  // conclusive, of no extra intermediate precision.

  unsigned int extra_precision = HasExtraDoublePrecisionTest();

#ifdef FLT_EVAL_METHOD
  if(FLT_EVAL_METHOD==0 || FLT_EVAL_METHOD==1) {
    // FLT_EVAL_METHOD claims intermediate double computations
    // performed at double precision (one rounding only).
# if !defined(__DMC__) || __DMC__ >= 0x852
    // Digital Mars 8.42n/8.51 has bad FLT_EVAL_METHOD setting
    if(extra_precision) {
      fprintf(stderr,
              "\nWARNING: Macro FLT_EVAL_METHOD claims no extra"
              " intermediate precision,\n       but extra precision"
              " detected by HasExtraDoublePrecision().\n"
              "       Float type set to long double.\n");
    }
# endif // __DMC__
  } else if(FLT_EVAL_METHOD>=2) {
    // Intermediate double computations at long double precision
    if(!extra_precision && sizeof(double)!=sizeof(long double)) {
      fprintf(stderr,
      "\nWARNING: Macro FLT_EVAL_METHOD warns extra precision, although\n"
      "         HasExtraDoublePrecision() didn't find it.  Float type set\n"
      "         to long double.\n");
      extra_precision = 1;
    }
  }
#endif // FLT_EVAL_METHOD
  return extra_precision;
}

// Check to see if (std::)fma(a,b,c) does one rounding or two.
// std::fma() is part of the C++11 standard. Returns 0 if one
// rounding, 1 if more than one rounding, 2 if can't tell, or
// -1 if no fma support.
template <typename floatType>
int CheckFmaRounding(floatType eps)
{
  // Code requires radix == 2.
  if(!std::numeric_limits<floatType>::is_specialized ||
     std::numeric_limits<floatType>::radix != 2) {
    // Either not radix 2 or can't tell.
    return 2;
  }
  assert(eps<1./64.);
  floatType a = static_cast<floatType>(1 + 8*eps);
  floatType b = static_cast<floatType>(1 + (1./64.));
  floatType c = static_cast<floatType>(7.*eps/16.);
  floatType d = std::fma(a,b,c);
  floatType correct = static_cast<floatType>(1. + (1./64.) + 8*eps + eps);
  if(d != correct) return 1;  // More than one rounding
  return 0;  // One rounding
}

// Routines to compute mantissa widths, in bits.
template<typename floatType>
int MantissaWidth()
{
  volatile floatType test = 2.0;
  test /= 3.0; test -= 0.5;
  test *= 3;   test -= 0.5;
  if(test<0.0) test *= -1;
  return static_cast<int>(0.5-log(test)/log(2.)); // floor
}

template <typename floatType>
void FloatExtremes(int& verytiny, int& tiny, int& giant)
{ // Returns power-of-two exponent for
  //   verytiny = smallest non-zero value
  //       tiny = smallest normal value
  //      giant = smallest non-representable power-of-two
  // Here "smallest" means smallest in magnitude.  Use "giant" instead
  // of "huge" lest some compiler mistake "huge" for a keyword.
  //
  // The giant and tiny values are related to the standard C macros
  // DBL_MIN_EXP and DBL_MAX_EXP, except that the usual definitions one
  // finds for the C macros are ambiguous.  In practice, one finds that
  // DBL_MAX_EXP is what is called here "giant", and "DBL_MIN_EXP - 1"
  // equals "tiny".  If gradual underflow is supported, then the value
  // "verytiny" is "DBL_MIN_EXP - DBL_MANT_DIG"; otherwise, underflows
  // round to zero in which case verytiny is the same as tiny.  However,
  // rather than relying on what the compiler says, in the code below we
  // compute these values directly.
  int i;
  volatile floatType testa;
  volatile floatType testb;
  volatile floatType check;
  i = 0;  testa = 1.0;  testb = 2.0;
  while(testa < testb) {
    testa = testb;  testb *= 2.0;  ++i;
  }
  giant = i;

  i = 0;  testa = 1.0;  testb = 2.0;
  while(0.0 < testa && testa < testb) {
    // Second test above (testa < testb) catches case where rounding
    // mode sends 0.5*(2^i) -> 2^i at i == verytiny.
    testb = testa;  testa *= 0.5;  --i;
  }
  verytiny = i + 1;  // testb = 2**verytiny

  // Test for gradual underflow
  testa = 4 * testb;
  testa += testb;
  testa /= 2;
  if(testa != 2*testb) {
    // No gradual underflow
    tiny = verytiny;
  } else {
    testa = 1.0;
    for(i=0;i>verytiny;--i) {
      check = testa + testb;
      if(check != testa) break;
      testa *= 0.5;
    }
    tiny = i;
  }
}

template<typename floatType>
void FloatMinMaxEpsilon
(int verytiny_exp,int tiny_exp,int giant_exp,int mantissa_width,
 floatType& min, floatType& max, floatType& epsilon,
 floatType& pow_2_mantissahalf, floatType& pow_2_mantissa,
 floatType& cuberoot_verytiny, floatType& verytiny
 )
{ // Given mantissa_width from MantissaWidth, and tiny and giant from
  // FloatExtremes, returns the smallest positive normal value and the
  // largest finite number for floatType.  If floatType is double then
  // min is DBL_MIN and max is DBL_MAX.  Also returns epsilon and
  // auxilary values pow_2_mantissahalf, pow_2_mantissa,
  // cuberoot_verytiny, and verytiny.
  int i;
  volatile floatType mintest=1;
  for (i=0;i>tiny_exp;--i) mintest *= 0.5;
  min = mintest;

  volatile floatType maxtest=1;
  for(i=0;i<mantissa_width;++i) maxtest *= 0.5;
  maxtest = static_cast<floatType>(1.0) - maxtest;
  for(i=0;i<giant_exp;++i) maxtest *= 2;
  max = maxtest;

  volatile floatType pot = 1;
  for(i=0, pot=1.0;i<(mantissa_width+1)/2;++i) pot *= 2.0;
  pow_2_mantissahalf = pot;
  for(;i<mantissa_width;++i) pot *= 2.0;
  pow_2_mantissa = pot;
  epsilon = static_cast<floatType>(2.0)/pot;

  for(i=0, pot=1.0;i>verytiny_exp/3;--i) pot *= 0.5;
  if((-verytiny_exp)%3) {
    pot *= static_cast<floatType>(pow(0.5L,((-verytiny_exp)%3)/3.0L));
  }
  cuberoot_verytiny = pot;

  for(i=0, pot=1.0;i>verytiny_exp;--i) pot *= 0.5;
  verytiny = pot;
}

template<typename floatType>
void DoubleDoubleSpecialValues
(int verytiny_exp, int giant_exp, int mantissa_width,
 floatType& dd_tiny, floatType& dd_splitmax,
 floatType& dd_epsilon, floatType& dd_sqrt_epsilon,
 floatType& dd_4rt_epsilon
 )
{ // Special values for a double-double constructed from two floatType
  // variables.
  int i;
  volatile floatType pot; // Power-of-two.

  for(i=0, pot=1.0;i>verytiny_exp + 2*mantissa_width;--i) pot *= 0.5;
  dd_tiny = pot;

  for(i=0, pot=1.0;i<giant_exp - 1 - (mantissa_width+1)/2;++i) pot *= 2.0;
  dd_splitmax = pot;

  for(i=0,pot=1.0;i<mantissa_width;++i) pot *= 0.5;
  dd_epsilon = pot*pot;
  dd_sqrt_epsilon = pot;
  dd_4rt_epsilon = sqrt(pot);
}

#if XP_USE_MPFR
// The functions CheckFmaRounding, MantissaWidth, FloatExtremes, and
// FloatMinMaxEpsilon require special handling for the MPFR type
// because that type does not accept the volatile qualifier.  For the
// MantissaWidth and FloatExtremes functions we do this via function
// template specialization.  This is somewhat brittle because function
// template specializations don't participate in function overload
// resolution.  For this reason, the general advice for function
// templates is to directly overload the base template in preference
// to specialization.  We could do this for FloatMinMaxEpsilon, but we
// can't for the other two because MantissaWidth has no parameters and
// FloatExtremes has only int parameters.  The other option would be
// to run these functions through a class, and to specialize on the
// class.
//   We specialize FloatMinMaxEpsilon so we can call it like a template
// via FloatMinMaxEpsilon<XP_MPFR>(...), which is in keeping with the
// other parallel calls.

template <>
int CheckFmaRounding<XP_MPFR>(XP_MPFR)
{ // No mpfr fma support at this time.
  return -1;
}


template<>
int MantissaWidth<XP_MPFR>()
{
  return std::numeric_limits<XP_MPFR>::digits;
}
template <>
void FloatExtremes<XP_MPFR>(int& verytiny, int& tiny, int& giant)
{
  giant = std::numeric_limits<XP_MPFR>::max_exponent;
  tiny  = std::numeric_limits<XP_MPFR>::min_exponent-1;
  // Test to see if gradual underflow is supported.
  XP_MPFR test = pow(static_cast<XP_MPFR>(2.0),tiny);
  if(test == 0.0) {
    fprintf(stderr,
            "ERROR: Broken pow(XP_MPFR,int); pow(2,%d) rounds to zero\n",
            tiny);
    exit(14);
  } else {
    test *= 0.5;
    if(test == 0.0) {
      // Gradual underflow not supported.
      verytiny = tiny;
    } else {
      verytiny  = std::numeric_limits<XP_MPFR>::min_exponent
        - std::numeric_limits<XP_MPFR>::digits;
    }
  }
}
template <>
void FloatMinMaxEpsilon<XP_MPFR>
(int verytiny_exp, int /* tiny_exp */, int /* giant_exp */,
 int mantissa_width,
 XP_MPFR& min, XP_MPFR& max, XP_MPFR& epsilon,
 XP_MPFR& pow_2_mantissahalf, XP_MPFR& pow_2_mantissa,
 XP_MPFR& cuberoot_verytiny, XP_MPFR& verytiny
 )
{
  min = std::numeric_limits<XP_MPFR>::min();
  max = std::numeric_limits<XP_MPFR>::max();
  epsilon = std::numeric_limits<XP_MPFR>::epsilon();

  int i;
  XP_MPFR pot = 1;
  for(i=0, pot=1.0;i<(mantissa_width+1)/2;++i) pot *= 2.0;
  pow_2_mantissahalf = pot;
  for(;i<mantissa_width;++i) pot *= 2.0;
  pow_2_mantissa = pot;
  assert(epsilon == 2.0/pot); // Little test that compiler optimization
                              // didn't cause problems.

  // The exponent range for mpfr is so large that it isn't practical to
  // compute verytiny via brute-force multiplication.  We could use
  // successive squaring like in XpLdExp, but instead assume pow is
  // properly implemented.  (However, improve accuracy by helping out
  // with range reduction.)
  const int t1 = verytiny_exp/3;
  const int t2 = verytiny_exp - 3*t1;
  cuberoot_verytiny = pow(static_cast<XP_MPFR>(2.0),
                          static_cast<XP_MPFR>(t2)/static_cast<XP_MPFR>(3.0));
  cuberoot_verytiny *= pow(static_cast<XP_MPFR>(2.0),t1);

  verytiny = pow(static_cast<XP_MPFR>(2),verytiny_exp);
}

template<>
void DoubleDoubleSpecialValues<XP_MPFR>
(int verytiny_exp, int giant_exp, int mantissa_width,
 XP_MPFR& dd_tiny, XP_MPFR& dd_splitmax,
 XP_MPFR& dd_epsilon, XP_MPFR& dd_sqrt_epsilon,
 XP_MPFR& dd_4rt_epsilon
 )
{ // Special values for a double-double constructed from two floatType
  // variables.

  // Special handling for XP_MPFR type because the exponent range is so
  // large.  See notes in FloatMinMaxEpsilon<XP_MPFR> routine concerning
  // verytiny and cuberoot_verytiny values.

  dd_tiny = pow(static_cast<XP_MPFR>(2),verytiny_exp+2*mantissa_width);
  dd_splitmax = pow(static_cast<XP_MPFR>(2),
                    giant_exp - 1 - (mantissa_width+1)/2);
  dd_sqrt_epsilon = pow(static_cast<XP_MPFR>(0.5),mantissa_width-1);
  dd_epsilon = dd_sqrt_epsilon * dd_sqrt_epsilon;
  dd_4rt_epsilon = sqrt(dd_sqrt_epsilon);
}

#endif // XP_USE_MPFR

class BaseTypeDetails {
public:
  XpBaseType basetype;
  std::string basetype_info;
  std::string typestr;
  int mantissa_precision;
  int decimal_digits;
  int huge_exp;
  int tiny_exp;
  int verytiny_exp;
  int fma_rounding_check; // 0 if fma is single rounded, 1 if more
  /// than one rounding, 2 if can't tell, -1 if no fma support.

  std::string max;
  std::string min;
  std::string cuberoot_verytiny;
  std::string verytiny;

  std::string pow_2_mantissahalf;
  std::string pow_2_mantissa;
  std::string epsilon;

  // Data for a double-double variable built from basetype
  std::string dd_tiny;
  std::string dd_splitmax;
  std::string dd_epsilon;
  std::string dd_sqrt_epsilon;
  std::string dd_4rt_epsilon;
};

class BaseTypeWrappers {
public:
  std::string frexp;
  std::string floor;
  std::string ceil;
};

class BaseTypeInfNan {
public:
  std::string infinity;
  std::string nan;
};

template<typename floatType>
void GetTypeDetails
(XpBaseType  import_basetype,      // Import
 const char* import_basetype_info, // Import
 const char* import_typestr,       // Import
 BaseTypeDetails& btd         // Export
 )
{
  btd.basetype      = import_basetype;
  btd.basetype_info = import_basetype_info;
  btd.typestr       = import_typestr;

  int mantissa_width = MantissaWidth<floatType>();
  int decimal_digits = PrintDecimalDigits(mantissa_width);
  btd.mantissa_precision = mantissa_width;
  btd.decimal_digits = decimal_digits;

  int verytiny_exp, tiny_exp, giant_exp;
  FloatExtremes<floatType>(verytiny_exp,tiny_exp,giant_exp);
  btd.huge_exp = giant_exp;
  btd.tiny_exp = tiny_exp;
  btd.verytiny_exp = verytiny_exp;

  floatType min,max,epsilon;
  floatType pow_2_mantissahalf,pow_2_mantissa;
  floatType cuberoot_verytiny,verytiny;
  FloatMinMaxEpsilon<floatType>(verytiny_exp,tiny_exp,giant_exp,
                                mantissa_width,
                                min,max,epsilon,
                                pow_2_mantissahalf,pow_2_mantissa,
                                cuberoot_verytiny,verytiny);
  btd.fma_rounding_check = CheckFmaRounding(epsilon);

  NumberToString(max,decimal_digits,btd.max);
  NumberToString(min,decimal_digits,btd.min);
  NumberToString(epsilon,decimal_digits,btd.epsilon);
  NumberToString(pow_2_mantissahalf,decimal_digits,btd.pow_2_mantissahalf);
  NumberToString(pow_2_mantissa,decimal_digits,btd.pow_2_mantissa);
  NumberToString(cuberoot_verytiny,decimal_digits,btd.cuberoot_verytiny);
  NumberToString(verytiny,decimal_digits,btd.verytiny);

  // Data for a double-double variable built from basetype
  floatType dd_tiny,dd_splitmax,dd_epsilon,dd_sqrt_epsilon,dd_4rt_epsilon;
  DoubleDoubleSpecialValues<floatType>
    (verytiny_exp,giant_exp,mantissa_width,
     dd_tiny, dd_splitmax,
     dd_epsilon, dd_sqrt_epsilon, dd_4rt_epsilon);
  NumberToString(dd_tiny,decimal_digits,btd.dd_tiny);
  NumberToString(dd_splitmax,decimal_digits,btd.dd_splitmax);
  NumberToString(dd_epsilon,decimal_digits,btd.dd_epsilon);
  NumberToString(dd_sqrt_epsilon,decimal_digits,btd.dd_sqrt_epsilon);
  NumberToString(dd_4rt_epsilon,decimal_digits,btd.dd_4rt_epsilon);
}

template<typename floatType>
void GetWrappers
(const BaseTypeDetails& /* btd */, // Import
 BaseTypeWrappers& wrappers        // Export
 )
{
  wrappers.frexp =
    "inline XP_DDFLOAT_TYPE XP_FREXP(XP_DDFLOAT_TYPE x,int* exp)\n"
    " { return std::frexp(x,exp); }";
  wrappers.floor =
    "inline XP_DDFLOAT_TYPE XP_FLOOR(XP_DDFLOAT_TYPE x)\n"
    " { return std::floor(x); }";
  wrappers.ceil =
    "inline XP_DDFLOAT_TYPE XP_CEIL(XP_DDFLOAT_TYPE x)\n"
    " { return std::ceil(x); }";
}

template<>
void GetWrappers<long double>
(const BaseTypeDetails& btd, // Import
 BaseTypeWrappers& wrappers   // Export
 )
{ // frexp, floor and ceil are frequently mangled for long
  // double types.  Test each and adjust wrappers as necessary.
  // Note: Applicants for ldexp should use Xp_DoubleDouble::LdExp.
  // Also, see notes above about function template specializations.
  wrappers.frexp = "";
  wrappers.floor = "";
  wrappers.ceil = "";

  int k;
  long double prex = 1.0L;
  for(k=0;k<btd.mantissa_precision-1;++k) prex *= 2;
  long double x = prex - 1;
  x += prex; // == 2^btd.mantissa_precision - 1
  int exp;
  long double mant = std::frexp(x,&exp); // frexp check
  for(k=0;k<exp;++k) mant *= 2;
  if(mant == x) {
    wrappers.frexp =
      "inline XP_DDFLOAT_TYPE XP_FREXP(XP_DDFLOAT_TYPE x,int* exp)\n"
      " { return std::frexp(x,exp); }";
  } else { // Bad frexp
    wrappers.frexp =
      "inline XP_DDFLOAT_TYPE XP_FREXP(XP_DDFLOAT_TYPE x,int* exp)\n"
      " { return frexpl(x,exp); } // Broken std::frexp";
  }
  long double z = x * 0.5L;
  long double zdown = std::floor(z);       // floor check
  long double zup   = std::ceil(z-1.0L);   // ceil check
  long double zcheck = z - 0.5L;
  if(zdown == zcheck) {
    wrappers.floor =
      "inline XP_DDFLOAT_TYPE XP_FLOOR(XP_DDFLOAT_TYPE x)\n"
      " { return std::floor(x); }";
  } else { // Bad floor
    wrappers.floor =
      "inline XP_DDFLOAT_TYPE XP_FLOOR(XP_DDFLOAT_TYPE x)\n"
      " { return floorl(x); } // Broken std::floor";
  }
  if(zup == zcheck) {
    wrappers.ceil =
      "inline XP_DDFLOAT_TYPE XP_CEIL(XP_DDFLOAT_TYPE x)\n"
      " { return std::ceil(x); }";
  } else { // Bad ceil
    wrappers.ceil =
      "inline XP_DDFLOAT_TYPE XP_CEIL(XP_DDFLOAT_TYPE x)\n"
      " { return ceill(x); } // Broken std::ceil";
  }
}

#if XP_USE_MPFR
template<>
void GetWrappers<XP_MPFR>
(const BaseTypeDetails& /* btd */, // Import
 BaseTypeWrappers& wrappers        // Export
 )
{
  wrappers.frexp =
    "inline XP_DDFLOAT_TYPE XP_FREXP(XP_DDFLOAT_TYPE x,int* exp)\n"
    " { return boost::multiprecision::frexp(x,exp); }";
  wrappers.floor =
    "inline XP_DDFLOAT_TYPE XP_FLOOR(XP_DDFLOAT_TYPE x)\n"
    " { return boost::multiprecision::floor(x); }";
  wrappers.ceil =
    "inline XP_DDFLOAT_TYPE XP_CEIL(XP_DDFLOAT_TYPE x)\n"
    " { return boost::multiprecision::ceil(x); }";
}
#endif // XP_USE_MPFR

template<typename floatType>
void GetInfNan
(const BaseTypeDetails& btd, // Import
 BaseTypeInfNan& infnan      // Export
 )
{
  if(std::numeric_limits<floatType>::has_infinity) {
    infnan.infinity
      = std::string("std::numeric_limits<")
      + btd.typestr
      + std::string(">::infinity()");
  } else {
    infnan.infinity
      = std::string("std::numeric_limits<")
      + btd.typestr
      + std::string(">::max()")
      + std::string(" /* No infinity for this type */");
  }
  if(std::numeric_limits<floatType>::has_quiet_NaN) {
    infnan.nan
      = std::string("std::numeric_limits<")
      + btd.typestr
      + std::string(">::quiet_NaN()");
  } else if(std::numeric_limits<floatType>::has_signaling_NaN) {
    infnan.nan
      = std::string("std::numeric_limits<")
      + btd.typestr
      + std::string(">::signaling_NaN()");
  } else {
    infnan.nan
      = std::string("(-1*std::numeric_limits<")
      + btd.typestr
      + std::string(">::max())")
      + std::string(" /* No NaN for this type */");
  }
}

////////////////////////////////////////////////////////////////////////
// Mainline usage:
//   build_port <use_alt_interface> <base_type> <compiler command with args>
// Argument use_alt_interface is 0 to use standard double-double interface
// or 1 to use alternative interface.  The base type should be one of
//   auto float double "long double"
// If the macro XP_USE_MPFR is 1, then the type
//   mpfr
// is additionally supported.

int main(int argc,char** argv)
{
#ifdef _MSC_VER
  // Disable annoying "This application has requested the Runtime to
  // terminate it in an unusual way." pop-up dialog when asserts are
  // triggered:
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif // _MSC_VER
#ifdef __BORLANDC__
  // Borland C++ enables floating point exceptions by default.  Turn
  // this off to get rolling Infs and NaNs.
  std::_control87(MCW_EM, MCW_EM); // Defined in float.h
#endif

  if(argc<4) {
    fprintf(stderr,"Usage: build_port use_alt base_type compile_cmd\n");
    exit(99);
  }

  int use_alt_interface = atoi(argv[1]);
  int range_check = atoi(argv[2]); // Use -1 to get default behavior (NDEBUG)
  std::string base_type_str = argv[3];
  std::transform(base_type_str.begin(),base_type_str.end(),
                 base_type_str.begin(), ::tolower);
  /// Note: Visual C++ complains here about possible precision loss
  /// because ::tolower returns an int rather than a char.

  char** compile_argv = argv+4;
  const int compile_argc = argc-4;

  // Determine base type
  XpBaseType base_type = XP_INVALID_TYPE;
  if(base_type_str.compare("auto")==0) {
    if(use_alt_interface || HasExtraDoublePrecision()) {
      base_type = XP_LONGDOUBLE_TYPE;
    } else {
      base_type = XP_DOUBLE_TYPE;
    }
  } else if(base_type_str.compare("float")==0) {
    base_type = XP_FLOAT_TYPE;
  } else if(base_type_str.compare("double")==0) {
    base_type = XP_DOUBLE_TYPE;
  } else if(base_type_str.compare("long double")==0) {
    base_type = XP_LONGDOUBLE_TYPE;
#if XP_USE_MPFR
  } else if(base_type_str.compare("mpfr")==0) {
    base_type = XP_MPFR_TYPE;
#endif // XP_USE_MPFR
  } else {
    std::cerr << "Unrecognized base type request: "
              << base_type_str << std::endl;
    exit(10);
  }

  // Determine base type details
  BaseTypeDetails btd;
  BaseTypeWrappers wrappers;
  BaseTypeInfNan infnan;
  double base_type_epsilon;
  switch(base_type) {
  case XP_FLOAT_TYPE:
    GetTypeDetails<float>(XP_FLOAT_TYPE,"float","float",btd);
    GetWrappers<float>(btd,wrappers);
    GetInfNan<float>(btd,infnan);
    base_type_epsilon
      = static_cast<double>(std::numeric_limits<float>::epsilon());
    break;
  case XP_DOUBLE_TYPE:
    GetTypeDetails<double>(XP_DOUBLE_TYPE,"double","double",btd);
    GetWrappers<double>(btd,wrappers);
    GetInfNan<double>(btd,infnan);
    base_type_epsilon = std::numeric_limits<double>::epsilon();
    break;
  case XP_LONGDOUBLE_TYPE:
    GetTypeDetails<long double>(XP_LONGDOUBLE_TYPE,
                                "long double","long double",btd);
    GetWrappers<long double>(btd,wrappers);
    GetInfNan<long double>(btd,infnan);
    base_type_epsilon
      = static_cast<double>(std::numeric_limits<long double>::epsilon());
    break;
#if XP_USE_MPFR
  case XP_MPFR_TYPE:
    GetTypeDetails<XP_MPFR>(XP_MPFR_TYPE,
                            "MPFR Multiprecision",XP_MPFR_STR,btd);
    GetWrappers<XP_MPFR>(btd,wrappers);
    GetInfNan<XP_MPFR>(btd,infnan);
    base_type_epsilon
      = static_cast<double>(std::numeric_limits<XP_MPFR>::epsilon());
    break;
#endif // XP_USE_MPFR
  default:
    std::cerr << "Invalid base type request: " << base_type << std::endl;
    exit(10);
    break;
  }

  // Dump output
  FILE* fptr = stdout;

  // Compile option info
  fputs("/* FILE: xpport.h             -*-Mode: c++-*-\n"
        " *\n"
        " * Platform specific #define's and typedef's for Xp extension,\n"
        " * generated by build_port.\n"
        " *\n"
        " * This is a machine-generated file.  DO NOT EDIT!\n"
        " *\n"
        " * This file based on compiler command:\n *  ",fptr);
  for(int i=0;i<compile_argc;++i) {
    fputs(" ",fptr); fputs(compile_argv[i],fptr);
  }
  fputs("\n */\n",fptr);

  // Open compile guard
  fputs("\n#ifndef _XP_PORT_H\n#define _XP_PORT_H\n\n",fptr);

  // Headers
  fputs("#include <cassert>\n#include <cfloat>\n#include <cmath>\n"
        "#include <cstdio>\n#include <cstdlib>\n#include <string>\n"
        "#include <limits>\n",
        fptr);

#if XP_USE_MPFR
  if(use_alt_interface) {
    fputs("#include <boost/version.hpp>\n"
          "#include <boost/multiprecision/mpfr.hpp>"
          "  // Multiprecision library\n"
          "using namespace boost::multiprecision;\n",fptr);
  }
#endif // XP_USE_MPFR

  // Specify use of standard Xp_DoubleDouble or alternative
  fprintf(fptr,"\n#define XP_USE_ALT_DOUBLEDOUBLE %d\n",use_alt_interface);

  // Range check?
  if(!use_alt_interface && range_check >= 0) {
    fprintf(fptr,"\n#define XP_RANGE_CHECK %d\n",range_check);
  }

  // Check floating point radix.  Some of the range limit code assumes
  // the radix of the exponent is 2.  This could probably be modified if
  // necessary, but the set of machines for which FLT_RADIX != 2 is very
  // small.  The FLT_RADIX setting affects the meaning of the
  // (L)DBL_MIN/MAX_EXP macros.
#if !defined(FLT_RADIX)
# error FLT_RADIX not defined
#endif
#if FLT_RADIX != 2
# error FLT_RADIX != 2
#endif
  fprintf(fptr,"#define XP_FLT_RADIX %d\n",FLT_RADIX);

#if XP_USE_MPFR
  fprintf(fptr,"#define XP_HAVE_COPYSIGN 0\n");
#else
  fprintf(fptr,"#define XP_HAVE_COPYSIGN 1\n");
#endif

#if !defined(XP_USE_FMA)
  // Default is to use fma if fma is single rounding
  if(btd.fma_rounding_check == 0) {
    fprintf(fptr,"#define XP_USE_FMA 1\n\n");
  } else {
    fprintf(fptr,"#define XP_USE_FMA 0\n\n");
  }
#else
  // Otherwise follower user request
  fprintf(fptr,"#define XP_USE_FMA %d\n\n",XP_USE_FMA);
#endif

  fprintf(fptr,
   "// Floating point type information:\n"
   "//  XP_DDFLOAT_MANTISSA_PRECISION is XP_DDFLOAT mantissa width in bits\n"
  "//  XP_DDFLOAT_DECIMAL_DIGITS is C macro DECIMAL_DIG value for XP_DDFLOAT\n"
   "//  XP_DDFLOAT_HUGE_EXP is smallest n such that 2^n is not finite\n"
   "//  XP_DDFLOAT_TINY_EXP is smallest n such that 2^n is normal\n"
   "//  XP_DDFLOAT_VERYTINY_EXP is smallest n such that 2^n is non-zero\n"
   "//   Note: If gradual underflow is not supported then tiny == verytiny\n"
   "//  XP_DDFLOAT_MAX is largest finite value\n"
   "//  XP_DDFLOAT_MIN is smallest positive normal value\n"
   "//  XP_DDFLOAT_CUBEROOT_VERYTINY is XP_DDFLOAT_VERYTINY^(1/3)\n"
   "//  XP_DDFLOAT_VERYTINY is smallest positive value (denormal)\n"
   "//  XP_DDFLOAT_POW_2_MANTISSA is pow(2,mantissa precision)\n"
   "//  XP_DDFLOAT_POW_2_MANTISSAHALF is pow(2,(mantissa precision+1)/2)\n"
   "//  XP_DDFLOAT_EPSILON is ULP(1), i.e., 2^(1-mantissa_precision)\n"
   "#define XP_FLOAT_TYPE %d\n"
   "#define XP_DOUBLE_TYPE %d\n"
   "#define XP_LONGDOUBLE_TYPE %d\n",
   XP_FLOAT_TYPE,XP_DOUBLE_TYPE,XP_LONGDOUBLE_TYPE);
  fprintf(fptr,"#define XP_MPFR_TYPE %d\n",XP_MPFR_TYPE);
  fprintf(fptr,"#define XP_USE_FLOAT_TYPE %s\n",
          XpBaseTypeString[btd.basetype]);
  fprintf(fptr,"#define XP_USE_FLOAT_TYPE_INFO \"%s\"\n",
          btd.basetype_info.c_str());
  fprintf(fptr,"typedef %s XP_DDFLOAT_TYPE;\n\n",btd.typestr.c_str());

  fprintf(fptr,"#define XP_DDFLOAT_MANTISSA_PRECISION %d\n",
          btd.mantissa_precision);
  fprintf(fptr,"#define XP_DDFLOAT_DECIMAL_DIGITS %d\n",btd.decimal_digits);
  fprintf(fptr,"#define XP_DDFLOAT_HUGE_EXP %d\n",btd.huge_exp);
  fprintf(fptr,"#define XP_DDFLOAT_TINY_EXP %d\n",btd.tiny_exp);
  fprintf(fptr,"#define XP_DDFLOAT_VERYTINY_EXP %d\n",btd.verytiny_exp);

  fprintf(fptr,"#define XP_DDFLOAT_MAX %s\n",btd.max.c_str());
  fprintf(fptr,"#define XP_DDFLOAT_MIN %s\n",btd.min.c_str());
  fprintf(fptr,"#define XP_DDFLOAT_CUBEROOT_VERYTINY %s\n",
          btd.cuberoot_verytiny.c_str());
  fprintf(fptr,"#define XP_DDFLOAT_VERYTINY %s\n",btd.verytiny.c_str());

  fprintf(fptr,"#define XP_DDFLOAT_POW_2_MANTISSA %s\n",
          btd.pow_2_mantissa.c_str());
  fprintf(fptr,"#define XP_DDFLOAT_POW_2_MANTISSAHALF %s\n",
          btd.pow_2_mantissahalf.c_str());
  fprintf(fptr,"#define XP_DDFLOAT_EPSILON %s\n",btd.epsilon.c_str());

  if(!use_alt_interface) {
    fputs("\n// Special constants for Xp_DoubleDouble class\n",fptr);
    fputs("// XP_DD_TINY is smallest non-denormal double-double.\n",fptr);
    fputs("// XP_DD_SPLITMAX is a lower bound on the largest import to\n"
          "//   ::Split that won't overflow.\n",fptr);
    fprintf(fptr,"#define XP_DD_TINY %s\n",btd.dd_tiny.c_str());
    fprintf(fptr,"#define XP_DD_SPLITMAX %s\n",btd.dd_splitmax.c_str());
    fprintf(fptr,"#define XP_DD_EPS %s\n",btd.dd_epsilon.c_str());
    fprintf(fptr,"#define XP_DD_SQRT_EPS %s\n",btd.dd_sqrt_epsilon.c_str());
    fprintf(fptr,"#define XP_DD_4RT_EPS %s\n",btd.dd_4rt_epsilon.c_str());
  } else {
    // In the standard interface, the XP_DD_* macros correspond to
    // values of the "base_type"-"base_type" class.  In the alt
    // interface the base_type is used singly, so the correspondence in
    // this branch to the base_type directly.
    fputs("\n// Constants parallel to standard Xp_DoubleDouble interface\n",
          fptr);
    fprintf(fptr,"#define XP_DD_EPS %.17e\n",base_type_epsilon);
  }

  fputs("\n// Wrappers for frexp, floor and ceil\n",fptr);
  fprintf(fptr,"%s\n",wrappers.frexp.c_str());
  fprintf(fptr,"%s\n",wrappers.floor.c_str());
  fprintf(fptr,"%s\n",wrappers.ceil.c_str());

  // Infinity, NaN support
  fputs("\n// Infinity, NaN support\n",fptr);
  fprintf(fptr,"const XP_DDFLOAT_TYPE XP_INFINITY =\n %s;\n",
          infnan.infinity.c_str());
  fprintf(fptr,"const XP_DDFLOAT_TYPE XP_NAN =\n %s;\n",
          infnan.nan.c_str());

  // Close compile guard
  fputs("\n#endif // _XP_PORT_H\n",fptr);

  return 0;
}
