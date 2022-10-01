/* FILE: varinfo.cc         -*-Mode: c++-*-
 *
 * Small program to probe system characteristics.
 *
 * Last modified on: $Date: 2014/10/29 23:55:24 $
 * Last modified by: $Author: donahue $
 *
 */

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * !!! NOTE: The plain text output from this program is parsed by the !!!
 * !!!       Oc_MakePortHeader proc in oommf/pkg/oc/procs.tcl during  !!!
 * !!!       the OOMMF build process.  Any changes to the output of   !!!
 * !!!       this program should be reflected in that proc.           !!!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */

#ifndef OOMMF_ATOMIC
# define OOMMF_ATOMIC 0
#endif

#if OOMMF_ATOMIC
# include <atomic>
#endif

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <climits>
#include <cmath>
#include <cfloat>

#include <iostream>

#ifdef __APPLE__
# define BUILD_MAC_OSX
# include <sys/types.h>
# include <sys/sysctl.h>
#ifndef CHECKQUICKEXIT
#  define CHECKQUICKEXIT 0
#endif
#endif

#if defined(__unix) || defined(_AIX) || defined(BUILD_MAC_OSX)
# define BUILD_UNIX
# include <unistd.h>
#endif /* __unix */

#ifdef _WIN32
# define BUILD_WINDOWS
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# undef WIN32_LEAN_AND_MEAN
# if defined(__MINGW_GCC) || defined(__MINGW32__)
/* If MinGW gcc uses the Microsoft C run-time libraries,
 * then the "long double" (e.g., Lg) format type is not
 * supported.  However, MinGW also has an ansi-compliant
 * stdio library that does.  Select accordingly.
 */
# if defined(__USE_MINGW_ANSI_STDIO) && __USE_MINGW_ANSI_STDIO!=0
#  define NO_L_FORMAT 0
# else
#  define NO_L_FORMAT 1
# endif
# endif
#endif

/* End includes */

// #define COMPLEX 1
#define VECTOR 1

#ifndef REPORT_PAGESIZE
# define REPORT_PAGESIZE 1
#endif

#ifndef REPORT_CACHE_LINESIZE
# define REPORT_CACHE_LINESIZE 1
#endif

#ifndef NO_L_FORMAT
# define NO_L_FORMAT 0
#endif

#ifndef EXTRALONG
# define EXTRALONG 1  /* Default is to do extra longs if possible */
#endif

#if EXTRALONG
# if !defined(EXTRALONGINT) && defined(LLONG_MAX)
#  define EXTRALONGINT 1
/* If LLONG_MAX is defined, then presumably "long long"
 * type is supported.
 */
# endif
#endif // EXTRALONG

#if EXTRALONG
# ifndef EXTRALONGINT
#  define EXTRALONGINT
# endif
# ifndef EXTRALONGDOUBLE
#  define EXTRALONGDOUBLE 1
# endif
#else /* EXTRALONG == 0 */
# undef EXTRALONGINT
# define EXTRALONGDOUBLE 0
#endif /* EXTRALONG */

#ifndef EXTRALONGINT
#  define EXTRALONGINT 0
#endif

#ifndef EXTRALONGDOUBLE
# define EXTRALONGDOUBLE 0
#endif

#ifndef USEQUADFLOAT
# define USEQUADFLOAT 0   /* Quad double-type; compiler specific extension. */
#endif

#ifndef CHECKQUICKEXIT
# define CHECKQUICKEXIT 1
#endif

#if EXTRALONGINT
# ifdef BUILD_WINDOWS
  typedef __int64 HUGEINT;
  const char* HUGEINTTYPE = "__int64";
# else
  typedef long long HUGEINT;
  const char* HUGEINTTYPE = "long long";
# endif
#endif

#if EXTRALONGDOUBLE
#include <cmath>    // Get long double overloads
typedef long double HUGEFLOAT;
const char* HUGEFLOATTYPE = "long double";
#endif

#if USEQUADFLOAT
# ifdef __GNUC__
/* Note 1: __float128 (aka _Float128) first appears in gcc 4.3
 *  Note 2: Include "-lquadmath" on link line.
 */
typedef __float128 QUADFLOAT;
const char* QUADFLOATTYPE = "__float128";
# include <quadmath.h>
# else
#  error Compiler specific directive for quad floating point type not declared.
# endif
#endif

#ifdef VECTOR
struct two_vector_float       { float x,y; };
struct two_vector_double      { double x,y; };
# if EXTRALONGDOUBLE
struct two_vector_long_double { long double x,y; };
# endif // EXTRALONGDOUBLE
# if USEQUADFLOAT
struct two_vector_quad_float { QUADFLOAT x,y; };
# endif

struct three_vector_float       { float x,y,z; };
struct three_vector_double      { double x,y,z; };
# if EXTRALONGDOUBLE
struct three_vector_long_double { long double x,y,z; };
# endif // EXTRALONGDOUBLE
# if USEQUADFLOAT
struct three_vector_quad_float { QUADFLOAT x,y,z; };
# endif
#endif // VECTOR

/*
 * If "EXTRALONG" is defined, then types "long long" and "long double"
 * are included.  In particular, the second (long double), may be the
 * 10 byte IEEE 854 (3.4E-4932 to 3.4E+4932) "double-extended-precision"
 * format (as opposed to the 8-byte IEEE 754 "double-precision" format).
 * Your system might have <ieee754.h> and <ieee854.h> include files with
 * more information.
 *
 * Some "magic" values for the IEEE formats
 *
 *               Value                     Internal representation
 *                                         (MSB order, hexadecimal)
 *   4-byte: 2.015747786                   40 01 02 03
 *   8-byte: 2.12598231449442521           40 01 02 03 04 05 06 07
 *  10-byte: 4.06286812764321414839        40 01 82 03 04 05 06 07 08 09
 *  16-byte: 4.031434063821607074202229911572472
 *                       40 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
 *  16-byte: 6.031434063821607074202229911572472
 *                       40 01 82 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
 *
 *  The 8-byte value is
 *
 *        2 + (1/8)*(1 + 2/256 + 3/256^2 + 4/256^3 + ... + 7/256^6).
 *
 *  The 10-byte value is
 *
 *        4 + 8*(2/256 + 3/256^2 + 4/256^3 + ... + 9/256^8).
 *
 *  The first 16-byte value is
 *
 *        4*(1 + 2/256 + 3/256^2 + 4/256^3 + ... + 15/256^14).
 *
 *  The second 16-byte value is just two more than the first.
 *     Note that the third byte in the 10-byte value and the second 16-byte is
 *  '0x82', NOT '0x02'.  This is useful because in the IEEE 10-byte format,
 *  bit 63 is an explicit "integer" or "J-bit" representing the leading bit in
 *  the mantissa.  For normal (as opposed to denormal) numbers this bit must
 *  be 1.  The J-bit is implied to be 1 in the 4- and 8-byte formats.  The
 *  leading byte is 0x40 so that the value has a small exponent; this protects
 *  against compilers that complain about over- and under-flow errors when
 *  selecting from among the magic values, in particular in the FLOATCONST
 *  macro.
 *    The PrintByteOrder routine masks off the top 4 bits, so we can
 *  put any values we want in the top 4 bits of each byte, except
 *  the special value 0x00 which is treated as invalid.
 *
 *  ADDENDUM: Another long double format seen on SGI's and reported
 *  on some older IBM machines is a composite of two packed doubles.
 *  (More recent IBM machines support the 16-byte IEEE format.)  You
 *  can check for this format with this value/byte string:
 *
 *               Value                     Internal representation
 *                                         (MSB order, hexadecimal)
 *  16-byte: 2.1259823144944253251455054395627
 *                       40 01 02 03 04 05 06 07 3C A1 02 03 04 05 06 07
 *
 *  Let foo = the 8-byte value above.  Then this value is
 *
 *          foo += foo*pow(2,-54);
 *
 *  HOWEVER, this representation is not IEEE compliant, and is not
 *  not tested for in the code below.
 */


/* Debugging routine */
#ifdef __cplusplus
void DumpBytes(char *buf,int len)
#else
void DumpBytes(buf,len)
char *buf;
int len;
#endif
{
  int i;
  for(i=0;i<len;i++) printf("%02X ",(unsigned char)buf[i]);
  printf("\n");
}

#define FillByteOrder(x,i,l) \
   for(i=1,x=0x10;i<l;i++) { x<<=8; x|=(0x10+(unsigned char)i); }


/* IEEE 754 floating point format "magic" strings (Hex 0x1234, 0x12345678) */
#define FLOATCONST(l) (l==4 ? (float)2.015747786 : \
		       (l==8 ? (float)2.12598231449442521 : \
			(float)0.))
#define DOUBLECONST(l) (l==4 ? (double)2.015747786 : \
		       (l==8 ? (double)2.12598231449442521 : \
			(double)0.))
#if EXTRALONGDOUBLE
#define LONGDOUBLECONST(l) \
            (l==4 ? 2.015747786L : \
            (l==8 ? 2.12598231449442521L : \
            (l==10 ? 4.06286812764321414839L : \
            (l>12  ? 4.031434063821607074202229911572472L : \
            (HUGEFLOAT)0.))))
#endif /* EXTRALONGDOUBLE */

#if USEQUADFLOAT
#define QUADFLOATCONST(l) \
            (l==4 ? 2.015747786Q : \
            (l==8 ? 2.12598231449442521Q : \
            (l==10 ? 4.06286812764321414839Q : \
            (l>12  ? 4.031434063821607074202229911572472Q : \
            (QUADFLOAT)0.))))
#endif /* USEQUADFLOAT */


#ifdef __cplusplus
void PrintByteOrder(char *buf,int len)
#else
void PrintByteOrder(buf,len)
char* buf;
int len;
#endif
{
  int i,ch;
  printf("Byte order: ");
  for(i=0;i<len;i++) {
    if(buf[i]==0) {
      printf("x");
    } else {
      ch=0x0F & ((unsigned char)buf[i]);
      if(ch==0x0F) {
	  printf("G");  // Hack
      } else {
	  printf("%01X",ch+1);
      }
    }
  }
}

float FltMachEps()
{
  volatile float eps,epsok,check;
  eps=1.0;
  epsok=2.0;
  check=1.0f+eps;
  while(check>1.0f) {
    epsok=eps;
    eps/=2.0f;
    check = 1.0f + eps;
  }
  return epsok;
}

double DblMachEps()
{
  volatile double eps,epsok,check;
  eps=1.0;
  epsok=2.0;
  check=1.0+eps;
  while(check>1.0) {
    epsok=eps;
    eps/=2.0;
    check = 1.0 + eps;
  }
  return epsok;
}

int FltPrecision()
{ // Returns effective mantissa width, in bits
  volatile float test = 2.0;
  test /= 3.0; test -= 0.5;
  test *= 3;   test -= 0.5;
  if(test<0.0) test *= -1;
  return int(0.5-log(test)/log(float(2.)));
}

int DblPrecision()
{ // Returns effective mantissa width, in bits
  volatile double test = 2.0;
  test /= 3.0; test -= 0.5;
  test *= 3;   test -= 0.5;
  if(test<0.0) test *= -1;
  return int(0.5-log(test)/log(double(2.)));
}

#if EXTRALONGDOUBLE
HUGEFLOAT LongDblMachEps()
{
  volatile HUGEFLOAT eps,epsok,check;
  eps=1.0;
  epsok=2.0;
  check=1.0+eps;
  while(check>1.0) {
    epsok=eps;
    eps/=2.0;
    check = 1.0 + eps;
  }
  return epsok;
}

int LongDblPrecision()
{ // Returns effective mantissa width, in bits
  volatile HUGEFLOAT test = 2.0;
  test /= 3.0; test -= 0.5;
  test *= 3;   test -= 0.5;
  if(test<0.0) test *= -1;
  return int(0.5-log(test)/log(HUGEFLOAT(2.)));
}
#endif /* EXTRALONGDOUBLE */

#if USEQUADFLOAT
QUADFLOAT QuadFloatMachEps()
{
  volatile QUADFLOAT eps,epsok,check;
  eps=1.0;
  epsok=2.0;
  check=1.0+eps;
  while(check>1.0) {
    epsok=eps;
    eps/=2.0;
    check = 1.0 + eps;
  }
  return epsok;
}

int QuadFloatPrecision()
{ // Returns effective mantissa width, in bits
  volatile QUADFLOAT test = 2.0;
  test /= 3.0; test -= 0.5;
  test *= 3;   test -= 0.5;
  if(test<0.0) test *= -1;
  return int(0.5-log(test)/log(QUADFLOAT(2.)));
}
#endif /* USEQUADFLOAT */

int DecimalDigits(int precision)
{ // Returns the number of decimal digits needed to exactly specify a
  // binary floating point value of precision bits.  For details, see
  //
  //    "How to print floating point numbers accurately," Guy L. Steele
  //    and Jon L. White, ACM SIGPLAN NOTICES, Volume 39, Issue 4,
  //    372-389 (Apr 2004).  DOI: 10.1145/989393.989431
  //
  // (Also available in Proceedings of the ACM SIGPLAN’SO Conference on
  // Programming Language Design and Implementation, White Plains, New
  // York, June 20-22, 1990, pp 112-126.)
  if(precision == 24) {
    return 9;
  }
  if(precision == 53) {
    return 17;
  }
  if(precision == 64) {
    return 21;
  }
  return 2 + int(floor(precision*log(2.)/log(10.)));
}

#if CHECKQUICKEXIT
// Use SFINAE to determine if quick_exit() is available
using namespace std;
template <typename T>
auto CheckQuickExit(T val,int& chk,int) -> decltype(quick_exit(val)) {
  chk = 1;
  std::cout << val; // Quiet compiler "val set but not used" warning
}
template <typename T>
auto CheckQuickExit(T,int& chk,double)  -> void {
  chk = 0;
}
#endif // CHECKQUICKEXIT

void Usage()
{
  fprintf(stderr,"Usage: varinfo [-h|--help]"
	  " [--skip-atan2] [--skip-underflow]\n");
  exit(99);
}

/* Declaration for dummy zero routine.  Definition at bottom of file. */
double zero();

#ifdef __cplusplus
int main(int argc,char** argv)
#else
int main(argc,argv)
int argc;
char **argv;
#endif
{
  short s;
  int i;
  long l;
  float f;
  double d;
#if EXTRALONGINT
  HUGEINT ll;
#endif /* EXTRALONGINT */
#if EXTRALONGDOUBLE
  size_t ihf;
  HUGEFLOAT ld;
#endif /* EXTRALONGDOUBLE */
#if USEQUADFLOAT
  size_t iqf;
  QUADFLOAT qf;
#endif /* EXTRALONGDOUBLE */
#ifdef COMPLEX
  __complex__ int ci;
  __complex__ double cd;
#endif /* COMPLEX */
  size_t st,loop;
  int skip_atan2,skip_underflow;

  /* Check command line flags */
  if(argc>3) Usage();
  skip_atan2=0;
  skip_underflow=0;
  if(argc>1) {
    int ia;
    for(ia=1;ia<argc;ia++) {
      if(strcmp("--skip-atan2",argv[ia])==0) skip_atan2=1;
      else if(strcmp("--skip-underflow",argv[ia])==0) skip_underflow=1;
      else Usage();
    }
  }

  /* Wide type info */
#if EXTRALONGINT
  printf("HUGEINTTYPE: %s\n",HUGEINTTYPE);
#endif
#if EXTRALONGDOUBLE
  printf("HUGEFLOATTYPE: %s\n",HUGEFLOATTYPE);
#endif
#if USEQUADFLOAT
  printf("QUADFLOATTYPE: %s\n",QUADFLOATTYPE);
#endif
#if EXTRALONGINT || EXTRALONGDOUBLE || USEQUADFLOAT
  printf("\n");
#endif

#ifdef UNICODE
  printf("Macro UNICODE is defined\n\n");
#else
  printf("Macro UNICODE is not defined\n\n");
#endif

  /* Work length info */
  printf("Type        char is %2d bytes wide   ",(int)sizeof(char));
  st=sizeof(char);
  if(st!=1) printf("ERROR: char should be 1 byte wide!\n");
  else      printf("Byte order: 1\n");
#ifdef _WIN32
  printf("Type       TCHAR is %2d bytes wide\n",(int)sizeof(TCHAR));
#endif // _WIN32

  printf("\n");
  printf("Type       short is %2d bytes wide   ",(int)sizeof(short));
  FillByteOrder(s,loop,sizeof(short));
  PrintByteOrder((char *)&s,(int)sizeof(short));      printf("\n");

  printf("Type         int is %2d bytes wide   ",(int)sizeof(int));
  FillByteOrder(i,loop,sizeof(int));
  PrintByteOrder((char *)&i,(int)sizeof(int));        printf("\n");

  printf("Type        long is %2d bytes wide   ",(int)sizeof(long));
  FillByteOrder(l,loop,sizeof(long));
  PrintByteOrder((char *)&l,(int)sizeof(long));       printf("\n");

#if EXTRALONGINT
  printf("Type %11s is %2d bytes wide   ",HUGEINTTYPE,(int)sizeof(HUGEINT));
  FillByteOrder(ll,loop,sizeof(HUGEINT));
  PrintByteOrder((char *)&ll,(int)sizeof(HUGEINT)); printf("\n");
#endif /* EXTRALONGINT */

  printf("\n");
  printf("Type       float is %2d bytes wide   ",(int)sizeof(float));
  f=FLOATCONST(sizeof(float));
  PrintByteOrder((char *)&f,(int)sizeof(float));      printf("\n");

  printf("Type      double is %2d bytes wide   ",(int)sizeof(double));
  d=DOUBLECONST(sizeof(double));
  PrintByteOrder((char *)&d,(int)sizeof(double));     printf("\n");

#if EXTRALONGDOUBLE
  printf("Type %11s is %2d bytes wide   ",
         HUGEFLOATTYPE,(int)sizeof(HUGEFLOAT));
  for(ihf=0;ihf<sizeof(HUGEFLOAT);++ihf) { ((char *)&ld)[ihf]=0; }
# if defined(LDBL_EPSILON)
  if(1e-20<LDBL_EPSILON && LDBL_EPSILON<1e-17) {
      ld=LONGDOUBLECONST(10); // Only 10 bytes are active; rest is filler
  } else {
      ld=LONGDOUBLECONST(sizeof(HUGEFLOAT));
  }
# else
  ld=LONGDOUBLECONST(sizeof(HUGEFLOAT));
# endif
  PrintByteOrder((char *)&ld,(int)sizeof(HUGEFLOAT));  printf("\n");
#endif /* EXTRALONGDOUBLE */

#if USEQUADFLOAT
  printf("Type %11s is %2d bytes wide   ",
         QUADFLOATTYPE,(int)sizeof(QUADFLOAT));
  for(iqf=0;iqf<sizeof(QUADFLOAT);++iqf) { ((char *)&qf)[iqf]=0; }
  qf=QUADFLOATCONST(sizeof(QUADFLOAT));
  PrintByteOrder((char *)&qf,(int)sizeof(QUADFLOAT));     printf("\n");
#endif /* USEQUADFLOAT */

  printf("\n");
  printf("Type      void * is %2d bytes wide\n",(int)sizeof(void *));

#ifdef COMPLEX
  printf("\n");
  printf("Type __complex__    int is %2d bytes wide\n",
	 sizeof(__complex__ int));
  printf("Type __complex__ double is %2d bytes wide\n",
	 sizeof(__complex__ double));
#endif /* COMPLEX */

#ifdef VECTOR
  printf("\n");
  printf("Type          two_vector_float is %2d bytes wide\n",
         (int)sizeof(struct two_vector_float));
  printf("Type         two_vector_double is %2d bytes wide\n",
         (int)sizeof(struct two_vector_double));
# if EXTRALONGDOUBLE
  printf("Type    two_vector_long_double is %2d bytes wide\n",
         (int)sizeof(struct two_vector_long_double));
# endif // EXTRALONGDOUBLE
# if USEQUADFLOAT
  printf("Type     two_vector_quad_float is %2d bytes wide\n",
         (int)sizeof(struct two_vector_quad_float));
# endif // EXTRALONGDOUBLE

  printf("\n");
  printf("Type        three_vector_float is %2d bytes wide\n",
         (int)sizeof(struct three_vector_float));
  printf("Type       three_vector_double is %2d bytes wide\n",
         (int)sizeof(struct three_vector_double));
# if EXTRALONGDOUBLE
  printf("Type  three_vector_long_double is %2d bytes wide\n",
         (int)sizeof(struct three_vector_long_double));
# endif // EXTRALONGDOUBLE
# if USEQUADFLOAT
  printf("Type   three_vector_quad_float is %2d bytes wide\n",
         (int)sizeof(struct three_vector_quad_float));
# endif // EXTRALONGDOUBLE
#endif

#if OOMMF_ATOMIC
  /* Atomic operation info */
  /* Support for std::atomic_id_lock_free and related functions incomplete
   * in some versions of g++.  So just report macro settings.
   */
  printf("\n");
# ifdef ATOMIC_CHAR_LOCK_FREE
  printf("ATOMIC_CHAR_LOCK_FREE = %d\n",ATOMIC_CHAR_LOCK_FREE);
# else
  printf("ATOMIC_CHAR_LOCK_FREE not defined");
# endif
# ifdef ATOMIC_SHORT_LOCK_FREE
  printf("ATOMIC_SHORT_LOCK_FREE = %d\n",ATOMIC_SHORT_LOCK_FREE);
# else
  printf("ATOMIC_SHORT_LOCK_FREE not defined");
# endif
# ifdef ATOMIC_INT_LOCK_FREE
  printf("ATOMIC_INT_LOCK_FREE = %d\n",ATOMIC_INT_LOCK_FREE);
# else
  printf("ATOMIC_INT_LOCK_FREE not defined");
# endif
# ifdef ATOMIC_LONG_LOCK_FREE
  printf("ATOMIC_LONG_LOCK_FREE = %d\n",ATOMIC_LONG_LOCK_FREE);
# else
  printf("ATOMIC_LONG_LOCK_FREE not defined");
# endif
# ifdef ATOMIC_LLONG_LOCK_FREE
  printf("ATOMIC_LLONG_LOCK_FREE = %d\n",ATOMIC_LLONG_LOCK_FREE);
# else
  printf("ATOMIC_LLONG_LOCK_FREE not defined");
# endif
# ifdef ATOMIC_POINTER_LOCK_FREE
  printf("ATOMIC_POINTER_LOCK_FREE = %d\n",ATOMIC_POINTER_LOCK_FREE);
# else
  printf("ATOMIC_POINTER_LOCK_FREE not defined");
# endif
#endif /* OOMMF_ATOMIC */

  /* Byte order info */
  printf("\n");  fflush(stdout);
  st=sizeof(double);
  if(st!=8) {
    fprintf(stderr,"Can't test byte order; sizeof(double)!=8\n");
  }
  else {
    union {
      double x;
      unsigned char c[8];
    } bytetest;
    double xpattern=1.234;
    unsigned char lsbpattern[9],msbpattern[9];
    strncpy((char *)lsbpattern,"\130\071\264\310\166\276\363\077",
	    sizeof(lsbpattern));
    strncpy((char *)msbpattern,"\077\363\276\166\310\264\071\130",
	    sizeof(msbpattern));

    /* The bit pattern for 1.234 on a little endian machine (e.g.,
     * Intel's x86 series and Digital's AXP) should be
     * 58 39 B4 C8 76 BE F3 3F, while on a big endian machine it is
     * 3F F3 BE 76 C8 B4 39 58.  Examples of big endian machines
     * include ones using the MIPS R4000 and R8000 series chips,
     * for example SGI's.  Note: At least some versions of the Sun
     * 'cc' compiler apparently don't grok '\xXX' hex notation.  The
     * above octal constants are equivalent to the aforementioned
     * hex strings.
     */
    bytetest.x=xpattern;

#ifdef DEBUG
    printf("sizeof(lsbpattern)=%d\n",sizeof(lsbpattern));
    printf("lsb pattern="); DumpBytes(lsbpattern,8);
    printf("msb pattern="); DumpBytes(msbpattern,8);
    printf("  x pattern="); DumpBytes(bytetest.c,8);
#endif /* DEBUG */

    /* Floating point format */
    if(strncmp((char *)bytetest.c,(char *)lsbpattern,8)==0) {
      printf("Floating point format is IEEE in LSB (little endian) order\n");
    }
    else if(strncmp((char *)bytetest.c,(char *)msbpattern,8)==0) {
      printf("Floating point format is IEEE in MSB (big endian) order\n");
    }
    else {
      printf("Floating point format is either unknown"
             " or uses an irregular byte order\n");
    }
  }

  /* Additional system-specific information (ANSI) */
  printf("\nWidth of clock_t variable: %7lu bytes\n",
	 (unsigned long)sizeof(clock_t));
#ifdef CLOCKS_PER_SEC
  printf(  "           CLOCKS_PER_SEC: %7lu\n",
	   (unsigned long)CLOCKS_PER_SEC);
#else
  printf(  "           CLOCKS_PER_SEC: <not defined>\n");
#endif
#ifdef CLK_TCK
  printf(  "                  CLK_TCK: %7lu\n",(unsigned long)CLK_TCK);
#else
  printf(  "                  CLK_TCK: <not defined>\n");
#endif

  printf("\nFLT_MANT_DIG: %3d\n",FLT_MANT_DIG);
#if defined(FLT_MAX_EXP)
  printf("FLT_MAX_EXP: %4d\n",FLT_MAX_EXP);
#endif
#if defined(FLT_MIN_EXP)
  printf("FLT_MIN_EXP: %4d\n",FLT_MIN_EXP);
#endif
  int fltdigs = DecimalDigits(FLT_MANT_DIG) - 1; // "-1" to use %e
  printf("FLT_MIN: %.*e\n",fltdigs,FLT_MIN);
  printf("SQRT_FLT_MIN: %.*e\n",fltdigs,sqrt(FLT_MIN));
  printf("FLT_MAX: %.*e\n",fltdigs,FLT_MAX);
  printf("SQRT_FLT_MAX: %.*e\n",fltdigs,sqrt(FLT_MAX));
  printf("FLT_EPSILON: %.*e\n",fltdigs,FLT_EPSILON);
  printf("SQRT_FLT_EPSILON: %.*e\n",fltdigs,sqrt(FLT_EPSILON));
  printf("CUBE_ROOT_FLT_EPSILON: %.*e\n",
         fltdigs,pow(double(FLT_EPSILON),1./3.));

  printf("\nDBL_MANT_DIG: %4d\n",DBL_MANT_DIG);
#if defined(DBL_MAX_EXP)
  printf("DBL_MAX_EXP: %5d\n",DBL_MAX_EXP);
#endif
#if defined(DBL_MIN_EXP)
  printf("DBL_MIN_EXP: %5d\n",DBL_MIN_EXP);
#endif
  int dbldigs = DecimalDigits(DBL_MANT_DIG) - 1; // "-1" to use %e
  printf("DBL_MIN: %.*e\n",dbldigs,DBL_MIN);
  printf("SQRT_DBL_MIN: %.*e\n",dbldigs,sqrt(DBL_MIN));
  printf("DBL_MAX: %.*e\n",dbldigs,DBL_MAX);
  printf("SQRT_DBL_MAX: %.*e\n",dbldigs,sqrt(DBL_MAX));
  printf("DBL_EPSILON: %.*e\n",dbldigs,DBL_EPSILON);
  printf("SQRT_DBL_EPSILON: %.*e\n",dbldigs,sqrt(DBL_EPSILON));
  printf("CUBE_ROOT_DBL_EPSILON: %.*e\n",dbldigs,pow(DBL_EPSILON,1./3.));

#if EXTRALONGDOUBLE
  printf("\n");
# if defined(LDBL_MANT_DIG)
  printf("LDBL_MANT_DIG: %5d\n",LDBL_MANT_DIG);
  int ldbldigs = DecimalDigits(LDBL_MANT_DIG);
# else
  int ldbldigs = DecimalDigits(LongDblPrecision());
# endif
  --ldbldigs; // Precision in %e format is number of digits
  /// to the right of decimal point; the total number of digits
  /// printed is thus one more than the %e precision.
# if defined(LDBL_MAX_EXP)
  printf("LDBL_MAX_EXP: %6d\n",LDBL_MAX_EXP);
# endif
# if defined(LDBL_MIN_EXP)
  printf("LDBL_MIN_EXP: %6d\n",LDBL_MIN_EXP);
# endif
# if !NO_L_FORMAT && defined(LDBL_MIN) && defined(LDBL_MAX)
  printf("LDBL_MIN: %.*LeL\n",ldbldigs,LDBL_MIN);
  printf("LDBL_MAX: %.*LeL\n",ldbldigs,LDBL_MAX);
# elif defined(LDBL_MANT_DIG) && defined(LDBL_MIN_EXP) && defined(LDBL_MAX_EXP)
  // Probably missing L format, but handle here also case where LDBL_MIN
  // and/or LDBL_MAX are missing.  It's a little tricky to get the
  // long double representations right using just doubles, in part
  // because while %.17g is lossless for doubles (assuming 53-bit mantissa
  // doubles), you generally can't just suffix an L (e.g., "%.17gL")
  // and get a correct long double value, because unless the value is and
  // integer then it won't round to the right long double.  Bottom line:
  // if you change one character in the following code, be certain to
  // carefully test that it still works.
  if(sizeof(double) == sizeof(HUGEFLOAT) &&
     DblPrecision() == LongDblPrecision() ) {
    // Presumably HUGEFLOAT and double are identical, so
    // it doesn't matter that L modifier is missing.
    printf("LDBL_MIN: %.*e\n",dbldigs,double(LDBL_MIN));
    printf("LDBL_MAX: %.*e\n",dbldigs,double(LDBL_MAX));
  } else {
    double pot = pow(2.0,DBL_MAX_EXP-1);
    int ldblexp = -1*LDBL_MIN_EXP;
    printf("LDBL_MIN: (1.0L/(2.0L");
    while(ldblexp > DBL_MAX_EXP-1) {
      printf("*%.*e",dbldigs,pot);
      ldblexp -= (DBL_MAX_EXP-1);
    }
    printf("*%.*e))\n",dbldigs,pow(2.0,ldblexp));

    double tweaka=1, tweakb=1;
    for(i=0;i<DBL_MANT_DIG-2;++i) tweaka *= 2.0;
    for(;i<LDBL_MANT_DIG-1;++i) tweakb *= 2.0;
    printf("LDBL_MAX: ((2.0L - 1.0L/(%.*eL*%.*eL))",
	   dbldigs,tweaka,dbldigs,tweakb);
    ldblexp = LDBL_MAX_EXP - 1;
    while(ldblexp > DBL_MAX_EXP-1) {
      printf("*%.*e",dbldigs,pot);
      ldblexp -= (DBL_MAX_EXP-1);
    }
    printf("*%.*e)\n",dbldigs,pow(2.0,ldblexp));
  }
# endif
# if defined(LDBL_EPSILON)
#  if NO_L_FORMAT
  printf("LDBL_EPSILON: %.*e\n",dbldigs,double(LDBL_EPSILON));
  printf("SQRT_LDBL_EPSILON: %.*e\n",dbldigs,double(sqrt(LDBL_EPSILON)));
  printf("CUBE_ROOT_LDBL_EPSILON: %.*e\n",dbldigs,
         pow(LDBL_EPSILON,HUGEFLOAT(1.)/3.));
#  else
  printf("LDBL_EPSILON: %.*LeL\n",ldbldigs,LDBL_EPSILON);
  // sqrt and pow may overload to long versions, but if not then
  // fix up with a few Newton steps.
  HUGEFLOAT sqrteps = sqrt(LDBL_EPSILON);
  HUGEFLOAT check = sqrteps*sqrteps;
  HUGEFLOAT err = check - LDBL_EPSILON;
  HUGEFLOAT abserr = (err>0?err:-1*err);
  int safety=0;
  while(abserr > 2*LDBL_EPSILON*LDBL_EPSILON && ++safety < 5) {
    // Newton step for sqrt
    sqrteps = 0.5*(check+LDBL_EPSILON)/sqrteps;
    check = sqrteps*sqrteps;
    err = check - LDBL_EPSILON;
    abserr = (err>0?err:-1*err);
  }
  if(safety>=5) {
    fprintf(stderr,"\nERROR: Failure extracting sqrt(LDBL_EPSILON)\n");
    exit(10);
  }
  printf("SQRT_LDBL_EPSILON: %.*LeL\n",ldbldigs,sqrteps);
  HUGEFLOAT crteps = pow(HUGEFLOAT(LDBL_EPSILON),HUGEFLOAT(1.)/3.);
  check = crteps*crteps*crteps;
  err = check - LDBL_EPSILON;
  abserr = (err>0?err:-1*err);
  safety=0;
  while(abserr > 4*LDBL_EPSILON*LDBL_EPSILON && ++safety < 5) {
    // Newton step for cube root
    crteps = (2*check+LDBL_EPSILON)/(3*crteps*crteps);
    check = crteps*crteps*crteps;
    err = check - LDBL_EPSILON;
    abserr = (err>0?err:-1*err);
  }
  if(safety>=5) {
    fprintf(stderr,"\nERROR: Failure extracting cube root of LDBL_EPSILON\n");
    exit(11);
  }
  printf("CUBE_ROOT_LDBL_EPSILON: %.*LeL\n",ldbldigs,crteps);
#  endif
# endif
#endif /* EXTRALONGDOUBLE */

#if USEQUADFLOAT
  /* Note: These macros may be compiler dependent.  The following set is
   * used by GCC.  Depending on your gcc build, the %Qe format specifier
   * for printf may or may not be supported.  Instead, the code below
   * uses quadmath_snprintf to do the __float128 -> string conversion.
   * Either way, check that -lquadmath is included in the link command.
   */
  char quadbuf[1024];
  printf("\nFLT128_MANT_DIG: %4d\n",FLT128_MANT_DIG);
#if defined(FLT128_MAX_EXP)
  printf("FLT128_MAX_EXP: %5d\n",FLT128_MAX_EXP);
#endif
#if defined(FLT128_MIN_EXP)
  printf("FLT128_MIN_EXP: %5d\n",FLT128_MIN_EXP);
#endif
  int quaddigs = DecimalDigits(FLT128_MANT_DIG) - 1; // "-1" to use %e
  quadmath_snprintf(quadbuf,sizeof(quadbuf),"%.*Qe",quaddigs,FLT128_MIN);
  printf("FLT128_MIN: %s\n",quadbuf);
  quadmath_snprintf(quadbuf,sizeof(quadbuf),"%.*Qe",
                    quaddigs,sqrtq(FLT128_MIN));
  printf("SQRT_FLT128_MIN: %s\n",quadbuf);
  quadmath_snprintf(quadbuf,sizeof(quadbuf),"%.*Qe",
                    quaddigs,FLT128_MAX);
  printf("FLT128_MAX: %s\n",quadbuf);
  quadmath_snprintf(quadbuf,sizeof(quadbuf),"%.*Qe",
                    quaddigs,sqrtq(FLT128_MAX));
  printf("SQRT_FLT128_MAX: %s\n",quadbuf);
  quadmath_snprintf(quadbuf,sizeof(quadbuf),"%.*Qe",
                    quaddigs,FLT128_EPSILON);
  printf("FLT128_EPSILON: %s\n",quadbuf);
  quadmath_snprintf(quadbuf,sizeof(quadbuf),"%.*Qe",
                    quaddigs,sqrtq(FLT128_EPSILON));
  printf("SQRT_FLT128_EPSILON: %s\n",quadbuf);
  quadmath_snprintf(quadbuf,sizeof(quadbuf),"%.*Qe",
                    quaddigs,powq(FLT128_EPSILON,1.Q/3.Q));
  printf("CUBE_ROOT_FLT128_EPSILON: %s\n",quadbuf);
#endif /* USEQUADFLOAT */


  printf("\n");
  int typespace = 6;
#if EXTRALONGDOUBLE
  if(typespace<(int)strlen(HUGEFLOATTYPE)) {
    typespace = (int)strlen(HUGEFLOATTYPE);
  }
#endif
#if USEQUADFLOAT
  if(typespace<(int)strlen(QUADFLOATTYPE)) {
    typespace = (int)strlen(QUADFLOATTYPE);
  }
#endif

  printf("Calculated %*s epsilon: %.*e\n",
         typespace,"float",fltdigs,FltMachEps());
  printf("Calculated %*s epsilon: %.*e\n",
         typespace,"double",dbldigs,DblMachEps());
#if EXTRALONGDOUBLE
# if NO_L_FORMAT
  printf("Calculated %*s epsilon: %.*e\n",
         typespace,HUGEFLOATTYPE,dbldigs,double(LongDblMachEps()));
# else
  printf("Calculated %*s epsilon: %.*LeL\n",
         typespace,HUGEFLOATTYPE,ldbldigs,LongDblMachEps());
# endif
#endif /* EXTRALONGDOUBLE */
#if USEQUADFLOAT
  quadmath_snprintf(quadbuf,sizeof(quadbuf),"%.*Qe",
                    quaddigs,QuadFloatMachEps());
  printf("Calculated %*s epsilon: %sQ\n",
         typespace,QUADFLOATTYPE,quadbuf);
#endif /* USEQUADFLOAT */

  printf("\n");
  printf("Calculated %*s precision: %3d bits\n",typespace,"float",FltPrecision());
  printf("Calculated %*s precision: %3d bits\n",typespace,"double",DblPrecision());
#if EXTRALONGDOUBLE
  printf("Calculated %*s precision: %3d bits\n",
         typespace,HUGEFLOATTYPE,LongDblPrecision());
#endif /* EXTRALONGDOUBLE */
#if USEQUADFLOAT
  printf("Calculated %*s precision: %3d bits\n",
         typespace,QUADFLOATTYPE,QuadFloatPrecision());
#endif /* USEQUADFLOAT */

  if(!skip_atan2) {
    errno = 0;
    printf("\nReturn value from atan2(0,0): %g\n",atan2(zero(),zero()));
    if(errno==0) {
      printf("Errno not set on atan2(0,0)\n");
    } else {
      printf("Errno set on atan2(0,0) to %d: %s\n",
             errno,strerror(errno));
    }
  }

  if(!skip_underflow) {
    d = -999999.;
    errno = 0;
    d = exp(d);
    if(d!=0.0) {
      fprintf(stderr,"\nERROR: UNDERFLOW TEST FAILURE: d=%g\n\n",d);
      exit(1);
    }
    if(errno==0) {
      printf("\nErrno not set on underflow\n");
    } else {
      printf("\nErrno set on underflow to %d: %s\n",
             errno,strerror(errno));
    }
  }

#ifdef EXTRALONGDOUBLE
  /* Check for bad long double floor and ceil.                */
  /* Note: Use "std::floor" rather than "floor" because there */
  /*       might not be an overload in place for the latter.  */
  ld = -0.253L;
  if(std::floor(ld) != -1.0L || (ld - std::floor(ld)) != 1.0L - 0.253L) {
    printf("\nBad std::floor(long double) -- failure type A\n");
  } else {
    ld = 1.0L - LDBL_EPSILON;
    if(std::floor(ld) != 0.0L) {
      printf("\nBad std::floor(long double) -- failure type B\n");
    } else {
      ld = -1.0L - LDBL_EPSILON;
      if(std::floor(ld) != -2.0L) {
        printf("\nBad std::floor(long double) -- failure type C\n");
      } else {
        printf("\nGood std::floor(long double)\n");
      }
    }
  }
  ld = -0.253L;
  if(std::ceil(ld) != 0.0L || (ld - std::ceil(ld)) != 0.0L - 0.253L) {
    printf("Bad std::ceil(long double) -- failure type A\n");
  } else {
    ld = 1.0L + LDBL_EPSILON;
    if(std::ceil(ld) != 2.0L) {
      printf("Bad std::ceil(long double) -- failure type B\n");
    } else {
      ld = -1.0L + LDBL_EPSILON;
      if(std::ceil(ld) != 0.0L) {
        printf("Bad std::ceil(long double) -- failure type C\n");
      } else {
        printf("Good std::ceil(long double)\n");
      }
    }
  }
#endif /* EXTRALONGDOUBLE */

#if CHECKQUICKEXIT
  {
    int chk;
    CheckQuickExit(0,chk,0);
    if(chk) puts("\nquick_exit is supported.");
    else    puts("\nquick_exit is not supported.");
  }
#endif // CHECKQUICKEXIT

#if REPORT_PAGESIZE
  {
    long int pagesize=0;
# if defined(BUILD_UNIX)
#  if defined(_SC_PAGESIZE)
    pagesize = sysconf(_SC_PAGESIZE);
#  elif defined(_SC_PAGE_SIZE)
    pagesize = sysconf(_SC_PAGE_SIZE);
#  elif defined(PAGESIZE)
    pagesize = sysconf(PAGESIZE);
#  elif defined(PAGE_SIZE)
    pagesize = sysconf(PAGE_SIZE);
#  endif
# elif defined(BUILD_WINDOWS)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    pagesize = (long int)si.dwPageSize;
# endif /* BUILD_UNIX */
    if(pagesize>0) {
      printf("\nMemory pagesize: %ld bytes\n",pagesize);
    } else {
      printf("\nMemory pagesize: unknown\n");
    }
  }
#endif // REPORT_PAGESIZE

#if REPORT_CACHE_LINESIZE
  {
    long int cache_linesize = 0;
# if defined(BUILD_UNIX)
#  if defined(_SC_LEVEL1_DCACHE_LINESIZE)
    cache_linesize = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
#  elif defined(LEVEL1_DCACHE_LINESIZE)
    cache_linesize = sysconf(LEVEL1_DCACHE_LINESIZE);
#  endif
#  if defined(BUILD_MAC_OSX)
    if(cache_linesize==0) {
      int dummy=0;
      size_t dummy_size = sizeof(dummy);
      if(sysctlbyname("machdep.cpu.cache.linesize",
		      &dummy,&dummy_size,0,0) == 0) {
	cache_linesize = dummy;
      }
    }
#  endif
    /* On Windows one can call GetLogicalProcessorInformation to get an
     * array of SYSTEM_LOGICAL_PROCESSOR_INFORMATION structs (presumably
     * one per processor), each of which contain CACHE_DESCRIPTOR
     * structs (presumably one for each cache), each of which has a
     * LineSize member.
     */
# endif // !BUILD_UNIX
    if(cache_linesize>0) {
      printf("\nCache linesize: %ld bytes\n",cache_linesize);
    } else {
      printf("\nCache linesize: unknown\n");
    }
  }
#endif // REPORT_CACHE_LINESIZE

  return 0;
}

/* Dummy routine that returns 0.  This is a workaround for aggressive
 * compilers that try to resolve away constants at compile-time (or try
 * to and fall over with an internal compiler error...)
 */
double zero()
{
  volatile double z = 0.0;
  return z;
}
