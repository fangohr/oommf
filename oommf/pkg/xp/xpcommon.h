/* FILE: xpcommon.h
 *
 * Header with common code for both doubledouble.h and
 * alt_doubledouble.h
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 */

#ifndef _XP_COMMON
#define _XP_COMMON

#include "oc.h"
#include "xpport.h" // Various compiler and compiler-option specific values.

/* End includes */

// Character I/O routines
int HexFloatWidth();
std::string HexFloatFormat(XP_DDFLOAT_TYPE value);
XP_DDFLOAT_TYPE ReadOneHexFloat(const char* buf,const char*& endptr);

int HexBinaryFloatWidth();

std::string HexBinaryFloatFormat(XP_DDFLOAT_TYPE value);

XP_DDFLOAT_TYPE ScanFloat(const char* cptr);

// Xp_BigFloatVec: Struct of integers to represent bit floats.
// The number represented is
//   sign * \sum_i=0^n chunk[i]*2^(b-i*m)
// where
//  sign = +1 or -1
//   n = chunk.size()
//  chunk = a vector of unsigned ints
//   b = offset
//   m = width of each element of chunk, in bits
//
// Note that each chunk[i] is an unsigned int, so width should be no
// bigger than 8*sizeof(unsigned int)
//
// This is essentially a radix two representation, so be careful when
// converting to any other radix.  In particular, in the template
// Xp_Unsplit, the division in each stage by a power of two may produce
// rounding error for other radices.  In that case a different
// composition algorithm (such as the one employed in proc Unsplit in
// ddtest.tcl) should be considered.
//
// The Tcl proc Split (or XpBigFloatVecInitString) in ddtest.tcl can be
// used to create data for this structure (requires the Tcl Mpexpr
// extension).
//
struct Xp_BigFloatVec {
public:
  int sign;    // +1 or -1
  int offset;  // Power-of-two offset of first chunk
  int width;   // Width of each chunk, in bits.
  std::vector<unsigned int> chunk;  // List of chunks
};

// Code to fill an object of type T with data from an Xp_BigFloatVec.
// This code assumes that type T can handle bit integer values.  (See
// the line marked "Integer sums".)
template <typename T>
T Xp_UnsplitBigFloat
(const Xp_BigFloatVec& data) // Import
{
  T two_m = pow(static_cast<T>(2),data.width);
  int n = data.chunk.size();
  T val = data.chunk[n-1];
  for(int i=n-2;i>=0;--i) {
    val = val/two_m + data.chunk[i];
    /// Note: No rounding error if T is a radix 2 float.
  }
  val *= data.sign*pow(2,data.offset);
  return val;
}

// NaN/Infinity/sign checks
OC_BOOL Xp_IsFinite(XP_DDFLOAT_TYPE x);
OC_BOOL Xp_IsPosInf(XP_DDFLOAT_TYPE x);
OC_BOOL Xp_IsNegInf(XP_DDFLOAT_TYPE x);
OC_BOOL Xp_IsNaN(XP_DDFLOAT_TYPE x);

OC_BOOL Xp_IsZero(XP_DDFLOAT_TYPE x);
OC_BOOL Xp_AreEqual(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y);

OC_BOOL Xp_SignBit(XP_DDFLOAT_TYPE x);

// Fast, non-broken(?) implementation of ldexp.
//
// There are many problems with system ldexp implementations.  Most are
// a good bit slower than multiplication by pow(2,n) (especially for
// small n).  Some don't round properly when the result is a denormal
// (gcc 4.4.7), some are missing (long double version, ldexpl, on
// cygwin).  The alternative of multiplying by pow(2,n) also has
// problems.  The first is that n has to be small enough that pow(2,n)
// doesn't overflow.  (For example, if x=2^-1000, then ldexp(x,2000) is
// the finite result 2^1000, but 2^2000 overflows in a standard 8-byte
// float.)  Another problem is that AFAICT, pow(2,n) universally flushes
// to zero on denormals.  The capper was discovering that on the Borland
// C++ 5.5.1 compiler, powl(2.L,16383)>2^16383.  I give up; what follows
// below is my own (mjd) implementation.

inline XP_DDFLOAT_TYPE
XpLdExp(XP_DDFLOAT_TYPE x,int m)
{ // Returns x * 2^m
  if(m==0) return x;

  unsigned int n;
  XP_DDFLOAT_TYPE base;
  if(m>0) {
    n = m;
    if(n & 1) x *= 2.0;
    base = 2.0;
  } else {
    n = -1*m;
    if(n & 1) x *= 0.5;
    base = 0.5;
  }

  // The nstop value below is set so that base^2 doesn't overflow.  The
  // shift amount is log_2(XP_DDFLOAT_HUGE_EXP).
#if XP_DDFLOAT_HUGE_EXP == 1024
  const unsigned int nstop = n >> 10;
#elif XP_DDFLOAT_HUGE_EXP == 16384
  const unsigned int nstop = n >> 14;
#else
  const unsigned int nstop = n >> int(floor(log(XP_DDFLOAT_HUGE_EXP)/log(2)));
#endif

  while( (n >>= 1u) > nstop) {
    // Base case: exponentiation by squaring
    base *= base;
    if(n & 1u) x *= base;
  }

  if(n>0u) {
    // Leftover bit in case where base*base would overflow
    x *= base;
    do {
      x *= base;
    } while( --n > 0u);
  }

  return x;
}

#endif /* _XP_COMMON */
