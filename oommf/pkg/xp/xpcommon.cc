/* FILE: common.cc
 *
 * Common code for doubledouble.cc and alt_doubledouble.cc.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 */

#include <cassert>
#include "xpcommon.h"

#if XP_USE_FLOAT_TYPE == XP_MPFR_TYPE
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/math/special_functions/sign.hpp>
using boost::math::isfinite;
using boost::math::signbit;
#endif

/* End includes */

////////////////////////////////////////////////////////////////////////
/// Start value safe block (associative floating point optimization
/// disallowed).
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// Extended value support (infinities, NaNs):
//
// The are a host of various isnan, isinf, isfinite, which are
// non-standard and broken in various ingenious ways on different
// compilers with different compiler options.  But mostly what is
// needed in this source file is a check on whether or not a given
// value is finite.  The following test appears pretty robust:
#if XP_USE_FLOAT_TYPE == XP_MPFR_TYPE
// MPFR type doesn't support volatile qualifier, so
// use standard C++11 library routines
OC_BOOL Xp_IsFinite(XP_DDFLOAT_TYPE tx)
{ return isfinite(tx); }
OC_BOOL Xp_IsPosInf(XP_DDFLOAT_TYPE tx)
{ return (tx == XP_INFINITY); }
OC_BOOL Xp_IsNegInf(XP_DDFLOAT_TYPE tx)
{ return (tx == -XP_INFINITY); }

#else // !XP_USE_FLOAT_TYPE == XP_MPFR_TYPE

OC_BOOL Xp_IsFinite(XP_DDFLOAT_TYPE tx)
{
  volatile XP_DDFLOAT_TYPE x = tx;
  return (-XP_DDFLOAT_MAX<=x && x<=XP_DDFLOAT_MAX);
}
OC_BOOL Xp_IsPosInf(XP_DDFLOAT_TYPE tx)
{ // Depending on compiler flags, differentiating Infs from NaNs is
  // difficult.  The following is a best try:
  volatile XP_DDFLOAT_TYPE x = tx;
  if(x == XP_INFINITY && !(x == -XP_INFINITY)) return 1;
  return 0;
  /// A more robust though probably slower alternative would be
  /// return (memcmp(&x,&XP_INFINITY,sizeof(x))==0);
}
OC_BOOL Xp_IsNegInf(XP_DDFLOAT_TYPE tx)
{ // Depending on compiler flags, differentiating Infs from NaNs is
  // difficult.  The following is a best try:
  volatile XP_DDFLOAT_TYPE x = tx;
  if(x == -XP_INFINITY && !(x == XP_INFINITY)) return 1;
  return 0;
  /// A more robust though probably slower alternative would be
  ///    XP_DDFLOAT_TYPE chk = -XP_INFINITY;
  ///    return (memcmp(&x,&chk,sizeof(x))==0);
}
#endif // XP_USE_FLOAT_TYPE == XP_MPFR_TYPE

OC_BOOL Xp_IsNaN(XP_DDFLOAT_TYPE x)
{
  if(Xp_IsFinite(x) || Xp_IsPosInf(x) || Xp_IsNegInf(x)) {
    return 0;
  }
  return 1;
}

// Some compilers (e.g. g++ 5.3.1) will take code like this
//
//    if(x==0.0) {
//       signbit = Xp_SignBit(x)
//    }
//
// and replace it with
//
//    if(x==0.0) {
//       signbit = Xp_SignBit(0.0)
//    }
//
// which is a broken optimization if one wants to distinguish between x
// being +0.0 and -0.0.  A workaround is to use a non-inlined subroutine
// call to make the magnitude check, e.g.
//
//    if(Xp_IsZero(x)) {
//       signbit = Xp_SignBit(0.0)
//    }
//
// (Here if(fabs(x)==0.0) might also work, depending on how much the
// compiler knows about fabs().)  The Xp_AreEqual routine handles
// similar issues for comparisons between two values.
//
// These routines also take care of NaN handling problems.
OC_BOOL Xp_IsZero(XP_DDFLOAT_TYPE x)
{
  return (Xp_IsFinite(x) && 0.0 == x);
}
OC_BOOL Xp_AreEqual(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y)
{
  if(Xp_IsNaN(x) || Xp_IsNaN(y)) return 0;
  return (x == y);
}

// The Xp_SignBit routines return 1 if x is negative, 0 if positive
// (same as std::signbit spec).
#if XP_USE_FLOAT_TYPE == XP_MPFR_TYPE
OC_BOOL Xp_SignBit(XP_DDFLOAT_TYPE x)
{ return boost::math::signbit(x); }
#else // !XP_USE_FLOAT_TYPE == XP_MPFR_TYPE
OC_BOOL Xp_SignBit(OC_REAL4 x)
{
  union BITS {
    OC_REAL4 f;
    OC_UINT4 u;
  };
  BITS a = {1.0};
  BITS b = {-1.0};
  BITS c = { x };
  if((a.u ^ b.u) & c.u) return 1;
  return 0;
}
OC_BOOL Xp_SignBit(OC_REAL8 x)
{
  union BITS {
    OC_REAL8 f;
    OC_UINT8 u;
  };
  BITS a = {1.0};
  BITS b = {-1.0};
  BITS c = { x };
  if((a.u ^ b.u) & c.u) return 1;
  return 0;
}
OC_BOOL Xp_SignBit(long double x)
{ // This code could be sped up considerably, in particular
  // by determining a priori the one bit where 1.0 and -1.0
  // differ, and then checking just that one bit.
  assert(sizeof(long double) % sizeof(unsigned int) == 0);
  static const long double af =  1.0;
  static const long double bf = -1.0;
  unsigned int const * a = reinterpret_cast<unsigned int const *>(&af);
  unsigned int const * b = reinterpret_cast<unsigned int const *>(&bf);
  unsigned int const * c = reinterpret_cast<unsigned int const *>(&x);
  for(size_t i=0; i*sizeof(unsigned int)<sizeof(long double); ++i) {
    if((a[i] ^ b[i]) & c[i]) return 1;
  }
  return 0;
}
#endif // XP_USE_FLOAT_TYPE == XP_MPFR_TYPE


////////////////////////////////////////////////////////////////////////
/// End value safe block
////////////////////////////////////////////////////////////////////////

int HexFloatWidth()
{
  return (XP_DDFLOAT_MANTISSA_PRECISION+6)/4
    + 6 + (XP_DDFLOAT_HUGE_EXP<3000 ? 4 : 5);
}

std::string HexFloatFormat(XP_DDFLOAT_TYPE value)
{
  char buf[256+XP_DDFLOAT_MANTISSA_PRECISION/4];
  if(!Xp_IsFinite(value)) { // Use %g to decide how to print the value
    snprintf(buf,sizeof(buf),"%g",static_cast<double>(value));
    char* cptr=buf;
    while(*cptr != '\0') {
      *cptr = static_cast<char>(tolower(*cptr));
      ++cptr;
    }
    std::string str = buf;
    if(str.find("inf")== std::string::npos) {
      str = "NaN";
    } else {
      if(str[0]=='-') {
        str = "-Inf";
      } else {
        str = "Inf";
      }
    }
    return str;
  }

  char* cptr = buf;
  XP_DDFLOAT_TYPE mantissa;
  int exp;
  mantissa = XP_FREXP(value,&exp);
  if(!Xp_SignBit(value)) { // Xp_SignBit detects signed zero
    *(cptr++) = ' ';  // Leave sign space
  } else {
    *(cptr++) = '-';
    mantissa *= -1;
  }

  // Write "0x" prefix to mantissa string, and leading "1."
  *(cptr++) = '0';  *(cptr++) = 'x';
  if(mantissa == 0.0) {
    *(cptr++) = '0';
  } else {
    *(cptr++) = '1';
    mantissa = 2*mantissa - 1.0;
    exp -= 1;
  }
  *(cptr++) = '.';

  // Write mantissa as hex digits
  for(int offset=1; offset<XP_DDFLOAT_MANTISSA_PRECISION; offset+=4) {
    mantissa *= 16;
    XP_DDFLOAT_TYPE fval = floor(mantissa);
    int ival = static_cast<int>(fval); // i.e., floor(mantissa)
    /// MPFR library has some trouble taking int() on temporary.
    if(ival<10) *(cptr++) = static_cast<char>('0' + ival);
    else        *(cptr++) = static_cast<char>('A' + ival - 10);
    mantissa -= ival;
  }

  if(XP_DDFLOAT_HUGE_EXP<3000) {
    snprintf(cptr,sizeof(buf)-(cptr-buf),"p%+05d",exp);
  } else {
    snprintf(cptr,sizeof(buf)-(cptr-buf),"p%+06d",exp);
  }
  return std::string(buf);
}

XP_DDFLOAT_TYPE ReadOneHexFloat(const char* buf,const char*& remainder)
{
  const int MAXSTOP = 16384; // Maximum buf size; fallback stop
  const char* CSTOP = buf + MAXSTOP;
  // Skip to data start; data has form 0x*, Inf, or NaN
  const char* cptr = strpbrk(buf,"0iInN");
  if(cptr == 0) { // No data
    remainder = 0;
    return 0.0;
  }
  // Check sign
  XP_DDFLOAT_TYPE valsign = 1.0;
  if( cptr - buf > 0 && *(cptr-1) == '-') {
    valsign = -1.0;
  }

  // Check for special values
  if(tolower(cptr[0])=='i' && tolower(cptr[1])=='n'
     && tolower(cptr[2])=='f') { // Infinity
    remainder = cptr+3;
    return valsign*XP_INFINITY;
  }
  if(tolower(cptr[0])=='n' && tolower(cptr[1])=='a'
     && tolower(cptr[2])=='n') { // NaN
    remainder = cptr+3;
    return XP_NAN;
  }

  // Otherwise, cptr should be pointing at a standard hexfloat.
  // First, convert string up to "."  Should be at most one digit.
  // (Moreover, that digit is suppose to be "1".)
  char* endptr;
  XP_DDFLOAT_TYPE val = XP_DDFLOAT_TYPE(strtol(cptr+2,&endptr,16));
  if(*endptr != '.') {
    std::string msg = "Bad input to ReadOneHexFloat: " + std::string(buf);
    throw msg;
  }
  cptr = endptr+1;
  XP_DDFLOAT_TYPE scale = 1.0/16.0;
  while(*cptr != 'p' && *cptr != '\0' && cptr != CSTOP) {
    int ival = *cptr - '0';
    if(ival>9) {
      ival -= ('A' - '0');
      if(ival>5) ival -= ('a'-'A');
      ival += 10;
    }
    val += XP_DDFLOAT_TYPE(ival)*scale;
    scale *= 1.0/16.0;
    // One alternative is to parse from back to front using a Horner
    // type algorithm to scale and sum values.
    ++cptr;
  }
  if(*cptr == 'p' || *cptr == 'P') {
    // Read exponent
    int iexp = strtol(cptr+1,&endptr,10);
    // Many implementations of ldexp misbehave in underflow
    // situations, by either flushing to zero or not rounding
    // properly.  This is not a critical path, so just brute-force it:
    if(iexp>0) {
      for(int i=0;i<iexp;++i) val *= 2;
    } else if(iexp<0) {
      for(int i=0;i>iexp;--i) val *= 0.5;
    }
    cptr = endptr;
  }

  remainder = cptr;
  return valsign*val;
}


int HexBinaryFloatWidth()
{
  return (XP_DDFLOAT_MANTISSA_PRECISION+3)/4
    + 6 + (XP_DDFLOAT_HUGE_EXP<3000 ? 3 : 4);
}

std::string HexBinaryFloatFormat(XP_DDFLOAT_TYPE value)
{
  char buf[256+XP_DDFLOAT_MANTISSA_PRECISION/4];

  if(!Xp_IsFinite(value)) { // Use %g to decide how to print the value
    snprintf(buf,sizeof(buf),"%g",static_cast<double>(value));
    char* cptr=buf;
    while(*cptr != '\0') {
      *cptr = static_cast<char>(tolower(*cptr));
      ++cptr;
    }
    std::string str = buf;
    if(str.find("inf")== std::string::npos) {
      str = "NaN";
    } else {
      if(str[0]=='-') {
        str = "-Inf";
      } else {
        str = "Inf";
      }
    }
    return str;
  }

  char* cptr = buf;
  XP_DDFLOAT_TYPE mantissa;
  int exp;
  mantissa = XP_FREXP(value,&exp);
  if(!Xp_SignBit(value)) { // Xp_SignBit detects signed zero
    *(cptr++) = ' ';  // Leave sign space
  } else {
    *(cptr++) = '-';
    mantissa *= -1;
  }

  // Write "0x" prefix to mantissa string
  *(cptr++) = '0';
  *(cptr++) = 'x';

  // Lead shift; if precision is not divisible by 4, then write
  // "left-over" bits (which won't fill a full hex digit) first.
#if XP_DDFLOAT_MANTISSA_PRECISION%4 != 0
    mantissa *= pow(XP_DDFLOAT_TYPE(2.0),XP_DDFLOAT_MANTISSA_PRECISION%4);
    exp -= XP_DDFLOAT_MANTISSA_PRECISION%4;
#else
    mantissa *= 16;
    exp -= 4;
#endif

  // Write mantissa as hex digits
  for(int offset=0; 4*offset<XP_DDFLOAT_MANTISSA_PRECISION; ++offset) {
    XP_DDFLOAT_TYPE fval = floor(mantissa);
    int ival = static_cast<int>(fval); // i.e., floor(mantissa)
    /// MPFR library has some trouble taking int() on temporary.
    if(ival<10) *(cptr++) = static_cast<char>('0' + ival);
    else        *(cptr++) = static_cast<char>('A' + ival - 10);
    mantissa -= ival;
    mantissa *= 16;
    exp -= 4;
  }

  exp += 4;
  if(XP_DDFLOAT_HUGE_EXP<3000) {
    snprintf(cptr,sizeof(buf)-(cptr-buf),"xb%+04d",exp);
  } else {
    snprintf(cptr,sizeof(buf)-(cptr-buf),"xb%+05d",exp);
  }
  return std::string(buf);
}


XP_DDFLOAT_TYPE ScanHexBinFloat(const char* cptr)
{ // Support for HexHex-Float and HexBin-Float formats, which have form
  //
  //    smmm...mmmxseee         (HexHex-Float)
  //    smmm...mmmxbseee        (HexBin-Float)
  //
  // where s is an optional mantisa sign (+ or -), mmm...mmm is an
  // arbitrary length run of hexadecimal digits (0-9, a-f, A-F), followed
  // by a literal 'x' (or 'X') or 'xb' (or 'XB') indicating the base of
  // the exponent (16 or 2, respectively), followed by an optional
  // exponent sign, and then a run of decimal digits representing the
  // power of 16 or 2.  Examples of the HexHex-Float format:
  //
  //     5a0x3
  //
  // is the decimal number (5*16^2 + 10*16) * 16^3 = 1440 * 4096 = 5898240.
  // and
  //      120Fx-7
  //
  // is the decimal number (1*16^3 + 2*16^2 + 15) * 16^-7 = 4623/268435456
  //                                      = 0.0000172220170497894287109375
  //
  // Examples of the HexBin-format:
  //
  //     5a0xb3
  //
  // is the decimal number (5*16^2 + 10*16) * 2^3 = 1440 * 8 = 11520.
  // and
  //      120Fxb-7
  //
  // is the decimal number (1*16^3 + 2*16^2 + 15) * 2^-7 = 4623/128
  //                                                     = 36.1171875
  //
  // Note: C99 and C++11 specify a different hexfloat format, represented
  // by the format specifier %a.  See the ScanC99HexFloat routine for
  // details.

  // Safer to work with unsigned chars
  const unsigned char* ptr = (const unsigned char*)cptr;

  // Skip leading junk
  unsigned char ch;
  while((ch=*ptr) != 0x0 && ch!='+' && ch!='-' &&
        !(('0'<=ch && ch<='9')                                  \
          || ('a'<=ch && ch<='f') || ('A'<=ch && ch<='F'))) {
    ++ptr;
  }
  // Set sign on mantissa
  int sign = 1;
  if(ch=='-') {
    sign = -1;
    ch = *(++ptr);
  } else if(ch=='+') {
    ch = *(++ptr);
  }

  // Skip leading "0x", if any
  if(ch == '0') {
    ch = *(++ptr);
    if(ch == 'x' || ch == 'X') {
      ch = *(++ptr);
    }
  }

  // Read mantissa
  XP_DDFLOAT_TYPE value = 0.0;
  while(ch != 0x0) {
    unsigned int digit;
    if(ch=='-') {
      sign = -1;
    } else if(ch=='+') {
      sign = 1;
    } else {
      if('0'<=ch && ch<='9') {
        digit = static_cast<unsigned int>(ch - '0');
      } else if('a'<=ch && ch<='f') {
        digit = static_cast<unsigned int>(ch - 'a' + 10);
      } else if('A'<=ch && ch<='F') {
        digit = static_cast<unsigned int>(ch - 'A' + 10);
      } else { // Bad value
        break;
      }
      value = 16*value + digit;
    }
    ch=*(++ptr);
  }

  // Exponent
  int exponent=0;
  double base=16.;
  if(ch=='x' || ch=='X') {
    ch=*(++ptr);
    if(ch=='b' || ch=='B') {
      base=2.;
      ++ptr;
    }
    exponent = atoi((const char*)(ptr));
  }
  if(base == 16) {
    exponent *= 4;
  }

  // Put it all together.  Break exponent up as necessary to protect
  // against under/overflow.  This code also in ScanC99HexFloat().
  while(exponent > XP_DDFLOAT_HUGE_EXP-1) {
    value *= pow(XP_DDFLOAT_TYPE(2),XP_DDFLOAT_TYPE(XP_DDFLOAT_HUGE_EXP-1));
    exponent -= XP_DDFLOAT_HUGE_EXP-1;
  }
  while(exponent < XP_DDFLOAT_TINY_EXP+1) {
    value *= pow(XP_DDFLOAT_TYPE(2),XP_DDFLOAT_TYPE(XP_DDFLOAT_TINY_EXP+1));
    exponent -= XP_DDFLOAT_TINY_EXP+1;
  }
  value *= pow(XP_DDFLOAT_TYPE(2),XP_DDFLOAT_TYPE(exponent));

  return sign*value;
}

XP_DDFLOAT_TYPE ScanC99HexFloat(const char* cptr)
{ // The format for the C99/C++11 hexfloat format is
  //    sOx1.mmm...mmmpseee         (hexfloat)
  // where s is an optional mantisa sign (+ or -), 0x is a literal
  // identifier indicating a hexadecimal value, 1. is the first part of
  // the normalized hexadecimal floating point value. mmm...mmm is an
  // arbitrary length run of hexadecimal digits (0-9, a-f, A-F),
  // followed by a literal 'p' (or 'P') indicating the start of the
  // base-2 exponent, then an optional exponent sign, and then a run of
  // decimal digits representing the power of 2.  Examples of the C99
  // hexfloat format:
  //
  //     0x1.68p13
  //
  // is the decimal number (1+6/16+8/16^2) * 2^13 = (360/256) * 2^13
  // = 360 * 2^5= 11520, and
  //
  //    -0x1.20Fp+5
  //
  // is the decimal number -(1*16^3 + 2*16^2 + 15) * 2^(5-12)
  // = -4623/128 = -36.1171875
  //
  // This is very similar to the HexBin format above, the difference
  // being the location of the hex point in the mantissa---this
  // shifts the exponent and re-jiggers the binary-to-hex grouping
  // in the mantissa.

  // Safer to work with unsigned chars
  const unsigned char* ptr = (const unsigned char*)cptr;

  // Skip leading junk
  unsigned char ch;
  while((ch=*ptr) != 0x0 && ch!='+' && ch!='-' &&
        !(('0'<=ch && ch<='9')                                  \
          || ('a'<=ch && ch<='f') || ('A'<=ch && ch<='F'))) {
    ++ptr;
  }
  // Set sign on mantissa
  int sign = 1;
  if(ch=='-') {
    sign = -1;
    ch = *(++ptr);
  } else if(ch=='+') {
    ch = *(++ptr);
  }

  // Skip leading "0x", if any
  if(ch == '0') {
    ch = *(++ptr);
    if(ch == 'x' || ch == 'X') {
      ch = *(++ptr);
    }
  }

  // Next two characters should be "1." or "0.", the latter only on
  // zero input.
  if(ch == '0') {
    // Optimization bugs will sometimes drop sign on sign*0.0, so use
    // copysign instead.
#if XP_HAVE_COPYSIGN
    return copysign(static_cast<XP_DDFLOAT_TYPE>(0.0),
                    static_cast<XP_DDFLOAT_TYPE>(sign));
#else
    return sign*static_cast<XP_DDFLOAT_TYPE>(0.0);
#endif
  }
  if(ch != '1' || *(++ptr) != '.') {
    std::string errmsg
      = "Error in ScanC99HexFloat; Invalid C99 hexfloat string: ";
    errmsg += cptr;
    throw errmsg;
  }

  // Read mantissa
  XP_DDFLOAT_TYPE value = 1.0;
  int exponent = 0;
  while( (ch = *(++ptr)) != 0x0) {
    unsigned int digit;
    if('0'<=ch && ch<='9') {
      digit = static_cast<unsigned int>(ch - '0');
    } else if('a'<=ch && ch<='f') {
      digit = static_cast<unsigned int>(ch - 'a' + 10);
    } else if('A'<=ch && ch<='F') {
      digit = static_cast<unsigned int>(ch - 'A' + 10);
    } else { // Bad value
      break;
    }
    value = 16*value + digit;
    exponent -= 4;
  }

  // Exponent (base is 2)
  if(ch=='p' || ch=='P') {
    exponent += atoi((const char*)(++ptr));
  }

  // Put it all together.  Break exponent up as necessary to protect
  // against under/overflow.  This code also in ScanHexBinFloat().
  while(exponent > XP_DDFLOAT_HUGE_EXP-1) {
    value *= pow(XP_DDFLOAT_TYPE(2),XP_DDFLOAT_TYPE(XP_DDFLOAT_HUGE_EXP-1));
    exponent -= XP_DDFLOAT_HUGE_EXP-1;
  }
  while(exponent < XP_DDFLOAT_TINY_EXP+1) {
    value *= pow(XP_DDFLOAT_TYPE(2),XP_DDFLOAT_TYPE(XP_DDFLOAT_TINY_EXP+1));
    exponent -= XP_DDFLOAT_TINY_EXP+1;
  }
  value *= pow(XP_DDFLOAT_TYPE(2),XP_DDFLOAT_TYPE(exponent));

  return sign*value;
}

XP_DDFLOAT_TYPE ScanFloat(const char* cptr)
{
  // If string contains an p or P, then calls ScanC99HexFloat.
  // If string contains an x or X, then calls ScanHexBinFloat.
  // Otherwise uses strtold()
  // Note: Check for 'p' first since C99HexFloat also contains
  //       an 'x' in the '0x' prefix.
  XP_DDFLOAT_TYPE value;
  // First check for infinities and NaNs.
  const char* special = strstr(cptr,"Inf");
  if(special) {
    XP_DDFLOAT_TYPE value_sign = 1;
    if(special>cptr && *(special-1) == '-') {
      value_sign = -1;
    }
    return value_sign*XP_INFINITY;
  }
  if(strstr(cptr,"NaN")) {
    return XP_NAN;
  }

  // Else, determine string format and process accordingly.
  if(strchr(cptr,'p') || strchr(cptr,'P')) {
    // Assume C99 hex-float
    value = ScanC99HexFloat(cptr);
  } else if(strchr(cptr,'x') || strchr(cptr,'X')) {
    // Assume hexhex-float
    value = ScanHexBinFloat(cptr);
  } else {
    // Assume decimal float
    value = static_cast<XP_DDFLOAT_TYPE>(strtold(cptr,0));
  }

  return value;
}
