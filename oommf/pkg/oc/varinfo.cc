/* FILE: varinfo.cc         -*-Mode: c++-*-
 *
 * Small program to probe system characteristics.
 *
 * Last modified on: $Date: 2012-09-11 14:12:51 $
 * Last modified by: $Author: donahue $
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <float.h>

#if defined(__unix) || defined(_AIX)
# define BUILD_UNIX
# include <unistd.h>
#endif /* __unix */

#ifdef _WIN32
# define BUILD_WINDOWS
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# undef WIN32_LEAN_AND_MEAN
# if defined(__MINGW_GCC) || defined(__MINGW32__)
/* MinGW gcc uses Microsoft C run-time libraries, which
 * don't support "long double" (e.g., Lg) format type.
 */
#  define NO_L_FORMAT 1
# endif
#endif

/* End includes */

#ifndef REPORT_PAGESIZE
# define REPORT_PAGESIZE 1
#endif

#ifndef REPORT_CACHE_LINESIZE
# define REPORT_CACHE_LINESIZE 1
#endif

#ifndef NO_L_FORMAT
# define NO_L_FORMAT 0
#endif

/****************
#define COMPLEX
#define EXTRALONG
#define EXTRALONGINT
#define EXTRALONGDOUBLE
****************/

#if !defined(EXTRALONGINT) && defined(LLONG_MAX)
# define EXTRALONGINT
/* If LLONG_MAX is defined, then presumably "long long"
 * type is supported.
 */
#endif

#ifdef __cplusplus
# if !defined(EXTRALONGDOUBLE) && !defined(__SUNPRO_CC) && !defined(__sun)
/* Some(?) releases of Sun Forte compiler missing floorl support. */
/* Some(?) g++ installations on Solaris missing floorl support. */
#  define EXTRALONGDOUBLE
# endif
#endif

#ifdef EXTRALONG
# ifndef EXTRALONGINT
#  define EXTRALONGINT
# endif
# ifndef EXTRALONGDOUBLE
#  define EXTRALONGDOUBLE
# endif
#endif

#ifdef EXTRALONGINT
# ifdef BUILD_WINDOWS
  typedef __int64 HUGEINT;
  const char* HUGEINTTYPE = "__int64";
# else
  typedef long long HUGEINT;
  const char* HUGEINTTYPE = "long long";
# endif
#endif

#ifdef EXTRALONGDOUBLE
typedef long double HUGEFLOAT;
const char* HUGEFLOATTYPE = "long double";
#endif

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
#ifdef EXTRALONGDOUBLE
#define LONGDOUBLECONST(l) \
            (l==4 ? 2.015747786L : \
            (l==8 ? 2.12598231449442521L : \
            (l==10 ? 4.06286812764321414839L : \
            (l>12  ? 4.031434063821607074202229911572472L : \
            (HUGEFLOAT)0.))))
#endif /* EXTRALONGDOUBLE */


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

double DblMachEps()
{
  volatile double one;
  double eps,epsok,check;
  one = 1.0;
  eps=1.0;
  epsok=2.0;
  check=1.0+eps;
  while(check>one) {
    epsok=eps;
    eps/=2.0;
    check = 1.0 + eps;
  }
  return epsok;
}

#ifdef EXTRALONGDOUBLE
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
#endif /* EXTRALONGDOUBLE */

void Usage()
{
  fprintf(stderr,"Usage: varinfo [-h|--help]"
	  " [--skip-atan2] [--skip-underflow]\n");
  exit(1);
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
#ifdef EXTRALONGINT
  HUGEINT ll;
#endif /* EXTRALONGINT */  
#ifdef EXTRALONGDOUBLE
  size_t ihf;
  HUGEFLOAT ld;
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

  /* Work length info */
  printf("Type        char is %2d bytes wide   ",(int)sizeof(char));
  st=sizeof(char);
  if(st!=1) printf("ERROR: char should be 1 byte wide!\n");
  else      printf("Byte order: 1\n");

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

#ifdef EXTRALONGINT
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

#ifdef EXTRALONGDOUBLE
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

  printf("\n");
  printf("Type      void * is %2d bytes wide\n",(int)sizeof(void *));

#ifdef COMPLEX
  printf("\n");
  printf("Type __complex__    int is %2d bytes wide\n",
	 sizeof(__complex__ int));
  printf("Type __complex__ double is %2d bytes wide\n",
	 sizeof(__complex__ double));
#endif /* COMPLEX */

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

  printf("\nFLT_EPSILON: %.9g\n",FLT_EPSILON);
  printf("SQRT_FLT_EPSILON: %.9g\n",sqrt(FLT_EPSILON));
  printf("DBL_EPSILON: %.17g\n",DBL_EPSILON);
  printf("SQRT_DBL_EPSILON: %.17g\n",sqrt(DBL_EPSILON));
  printf("CUBE_ROOT_DBL_EPSILON: %.17g\n",pow(DBL_EPSILON,1./3.));
#if !NO_L_FORMAT && defined(LDBL_EPSILON)
  printf("LDBL_EPSILON: %.20Lg\n",LDBL_EPSILON);
#endif

#if !NO_L_FORMAT && defined(EXTRALONGDOUBLE)
  printf("\nCalculated Double Epsilon:    %.17g\n",DblMachEps());
  printf(  "Calculated HUGEFLOAT Epsilon: %.20Lg\n",LongDblMachEps());
#else
  printf("\nCalculated Double Epsilon: %.17g\n",DblMachEps());
#endif /* EXTRALONGDOUBLE */

  printf("\nDBL_MIN: %.17g\n",DBL_MIN);
  printf("DBL_MAX: %.17g\n",DBL_MAX);

  if(!skip_atan2) {
    printf("\nReturn value from atan2(0,0): %g\n",atan2(zero(),zero()));
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
# ifdef NO_FLOORL_CHECK
  printf("\nLibrary function floorl assumed bad or missing.\n");
# else
  /* Check for bad long double floor and ceil */
  ld = -0.253L;
  if(floorl(ld) != -1.0L || (ld - floorl(ld)) != 1.0L - 0.253L) {
    printf("\nBad floorl.  You should set "
            "program_compiler_c++_property_bad_wide2int"
            " to 1 in the platform oommf/config/platforms/ file.\n");
  } else {
    printf("\nGood floorl.\n");
  }
# endif
#endif /* EXTRALONGDOUBLE */

#if REPORT_PAGESIZE
  {
    long int pagesize=0;
# if defined(BUILD_UNIX)
#  if defined(_SC_PAGESIZE)
    pagesize = sysconf(_SC_PAGESIZE);
#  elif defined(_SC_PAGE_SIZE) 
    pagesize = sysconf(_SC_PAGE_SIZE);
#  endif
# elif defined(BUILD_WINDOWS)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    pagesize = (long int)si.dwPageSize;
# endif
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
#  endif
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
  return 0.0;
}
