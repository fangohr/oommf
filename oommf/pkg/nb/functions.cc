/* FILE: functions.cc                   -*-Mode: c++-*-
 *
 * Non-class C++ functions defined by the Nb extension.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/07/23 20:28:04 $
 * Last modified by: $Author: donahue $
 */

#include <cassert>
#include <cctype>
#include <climits>
#include <cstring>
#include <cstdarg>

#include "oc.h"
#include "functions.h"
#include "errhandlers.h"

#if (OC_SYSTEM_TYPE == OC_UNIX)
# include <pwd.h>  /* Include this after oc.h to get OC_SYSTEM_TYPE defines */
# if (OC_SYSTEM_SUBTYPE == OC_CYGWIN)
  /* Windows header file.  This is needed for the Nb_GetUserName and
   * Nb_GetPidUserName functions below.  If OC_SYSTEM_TYPE is OC_WINDOWS,
   * then this is already #include'd by ocport.h
   */
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  undef WIN32_LEAN_AND_MEAN
# endif
#endif

#if (OC_SYSTEM_TYPE == OC_UNIX) || (OC_SYSTEM_TYPE == OC_DARWIN)
// Header files for stat and other low level C I/O.
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#endif

#if (OC_SYSTEM_TYPE == OC_WINDOWS)
# include <io.h> // Header file for _access
# include <sys/types.h> // Header files for _stat
# include <sys/stat.h>
#endif


OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported.
/// This insures standard math functions like floor, ceil, sqrt
/// are available.

/* End includes */

// Define file access mode constants if they aren't already defined.
#ifndef F_OK
#    define F_OK 00
#endif
#ifndef X_OK
#    define X_OK 01
#endif
#ifndef W_OK
#    define W_OK 02
#endif
#ifndef R_OK
#    define R_OK 04
#endif

#define NB_FUNCTIONS_ERRBUFSIZE 1024

//////////////////////////////////////////////////////////////////////////
// Dummy routine to force evaluation of temporaries.  Used as a
// hack to work around some compiler bugs.

// Dummy routine to force evaluation of temporaries.  Used as a
// hack to work around some compiler bugs.
// NOTE: The <stdarg.h> macros do not support a function signature
//       with no fixed parameters.  We don't use the parameters
//       anyway, but to be safe we implement this via overloading.
//       Add more as needed.
void Nb_NOP() {}
void Nb_NOP(OC_REAL8m) {}
void Nb_NOP(void*) {}

// Tcl wrapper for Nb_Gcd
int NbGcdCmd(ClientData,Tcl_Interp *interp,int argc,CONST84 char** argv)
{
  char buf[1024];
  Tcl_ResetResult(interp);
  if(argc!=3) {
    Oc_Snprintf(buf,sizeof(buf),"Nb_Gcd must be called with 2 arguments,"
	    " m and n (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  long int n=atol(argv[1]);
  long int m=atol(argv[2]);

  if(m<1 || n<1) {
    Oc_Snprintf(buf,sizeof(buf),"Nb_Gcd input must be two positive integers"
	    " (got %ld, %ld)", n,m);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Oc_Snprintf(buf,sizeof(buf),"%ld",Nb_Gcd(m,n));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

//////////////////////////////////////////////////////////////////////////
// Simple continued fraction-like rational approximator.  Unlike the usual
// continued fraction expansion, here we allow +/- on each term (rounds to
// nearest integer rather than truncating).  This converges faster (by
// producing fewer terms).
//   A more useful rat approx routine would probably take an error
// tolerance rather than a step count, but we'll leave that until the time
// when we have an actual use for this routine.  (There is a 4 term
// recursion relation for continued fractions in "Numerical Recipes in C"
// that can probably be used, and also remove the need for the coef[]
// array.)  -mjd, 23-Jan-2000.

void Nb_RatApprox(double x,int steps,int &num,int &denom)
{
  int xsign = 1;
  if(x<0) { xsign = -1; x*=-1; }
  if(steps<1) steps=1;
  int *coef=new int[size_t(steps)];
  coef[0]=int(OC_ROUND(x));
  double remainder=fabs(x)-fabs(double(coef[0]));
  // Expand
  int i=0;
  while( ++i < steps ) {
    if(fabs(remainder)*double(INT_MAX)<=1.0) break;
    remainder=1.0/remainder;
    coef[i]=int(OC_ROUND(remainder));
    remainder=fabs(remainder)-fabs(double(coef[i]));
  }
  // Collect
  int itop = i;
  int a,b;
  do {
    i = --itop;
    if(i<0) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,NULL,
                          "Nb_RatApprox",NB_FUNCTIONS_ERRBUFSIZE,
                          "Algorithm failure"));
    }
    a=1,b=coef[i];
    while( --i >= 0 ) {
      int sign=1;
      int c=coef[i];
      if(c<0) { sign=-1; c*=-1; }
      int temp=b;
      double check = double(b)*double(c)+double(a);
      b = int(check);
      if(double(b) != check) {
        // Integer overflow; retry with fewer coefs.  This may be slow
        // if the client requested an unreasonable number of steps---an
        // alternative is to throw an error.
        break;
      }
      // b=b*c+a;
      a=sign*temp;
    }
  } while(i>0);

  if(a<0) { a*=-1; b*=-1; }
  int div=1;
  if(a!=0 && b!=0) {
    div=Nb_Gcd(a,abs(b));
  }
  num=xsign*b/div;  denom=a/div;
  delete[] coef;
}

// Tcl wrapper for the above
int NbRatApprox(ClientData,Tcl_Interp *interp,int argc,CONST84 char** argv)
{
  char buf[1024];
  Tcl_ResetResult(interp);
  if(argc!=3) {
    Oc_Snprintf(buf,sizeof(buf),"Nb_RatApprox must be called with 2 arguments,"
	    " x and steps (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  double x=Nb_Atof(argv[1]);
  int steps=atoi(argv[2]);

  int num,denom;
  Nb_RatApprox(x,steps,num,denom);

  Oc_Snprintf(buf,sizeof(buf),"%d %d",num,denom);
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}


//////////////////////////////////////////////////////////////////////////
// Nb_FindRatApprox is similar to Nb_RatApprox, but takes x and y
// as separate imports, and also takes error tolerance imports.
// Fills exports p and q with best result, and returns 1 or 0 according
// to whether or not p/q meets the specified relative error.
int Nb_FindRatApprox
(OC_REALWIDE x,OC_REALWIDE y,
 OC_REALWIDE relerr, OC_REALWIDE maxq,
 OC_REALWIDE& p,OC_REALWIDE& q)
{ // Uses Euclid's algorithm to find rational approximation p/q to x/y

  int sign = 1;
  if(x<0) { x *= -1;  sign *= -1; }
  if(y<0) { y *= -1;  sign *= -1; }

  int swap = 0;
  if(x<y) { OC_REALWIDE t = x; x = y; y = t; swap = 1; }

  const OC_REALWIDE x0 = x;
  const OC_REALWIDE y0 = y;

  OC_REALWIDE p0 = 0;  OC_REALWIDE p1 = 1;
  OC_REALWIDE q0 = 1;  OC_REALWIDE q1 = 0;

  OC_REALWIDE m = floor(x/y);
  OC_REALWIDE r = x - m*y;
  OC_REALWIDE p2 = m*p1 + p0;
  OC_REALWIDE q2 = m*q1 + q0;
  while(q2<maxq && fabs(x0*q2 - p2*y0) > relerr*x0*q2) {
    x = y;
    y = r;
    m = floor(x/y);
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

//////////////////////////////////////////////////////////////////////////
// Version of atof that allows 'D' or 'd' to be used to denote exponent
double Nb_Atof(const char *nptr)
{ // NB: Mimicking the system atof(), this routine will seg fault
  // if nptr==NULL.  It is the resposibility of the calling routine
  // to insure this doesn't occur.  OTOH, the Nb_Atof(const char*,OC_BOOL&)
  // below checks and catches this error.
  const char *cptr;
  if((cptr=strchr(nptr,'d'))==NULL &&
      (cptr=strchr(nptr,'D'))==NULL  )
    return atof(nptr);  // Usual, C-compatible string
  /// The above checks seem to add about 10% overhead for short
  /// strings on a 486/66dx2, and it is faster there to check for
  /// each of 'd' and 'D' separately than to call strcspn(nptr,"dD");
  /// The base atof time on this machine is about 85 microseconds
  /// to start with (compared to about 6 microseconds for atoi).

  // Otherwise, assume we are working with a FORTRAN-style
  // "double precision" string.
  //   On the aforementioned 486, with gcc 2.7.2, this code costs
  // on average about an additional 10% over the above, if statbuf
  // is used.  Calls to new[] and delete[] add another 20%.  NOTE:
  // the strncpy function is really slow on this machine (it takes
  // as long as the atof call itself), so instead of using strncpy
  // to ensure we don't write past the end of statbuf, we take a
  // more flexible approach: see how big the string is, and if it
  // is too big (which is unlikely, but possible), then allocate
  // temporary storage just large enough to hold the string.  The
  // latter is a performance hit, so if it happens frequently one
  // should enlarge the size of statbuf.
  size_t exp_off=static_cast<size_t>(cptr-nptr);
  char statbuf[256]; // I don't expect many numeric strings this long
  char *buf=statbuf;
  size_t num_len=strlen(nptr)+1;
  if(num_len>sizeof(statbuf)) buf=new char[num_len];
  strcpy(buf,nptr);
  buf[exp_off]='e';         // Replace 'd' or 'D' with 'e'
  double result=atof(buf);
  if(buf!=statbuf) delete[] buf;
  return result;
}

// Version of strtod that allows 'D' or 'd' to be used to denote exponent
double Nb_Strtod(const char *nptr,char **endptr)
{
  // Replace first 'd' or 'D', if any, with 'e'
  char statbuf[256]; // I don't expect many num. strs this long
  char *buf=statbuf;
  const char *cptr;
  if((cptr=strchr(nptr,'d'))!=NULL ||
     (cptr=strchr(nptr,'D'))!=NULL  ) {
    size_t exp_off=static_cast<size_t>(cptr-nptr);
    size_t num_len=strlen(nptr)+1;
    if(num_len>sizeof(statbuf)) buf=new char[num_len];
    strcpy(buf,nptr);
    buf[exp_off]='e';         // Replace 'd' or 'D' with 'e'
    cptr=buf;
  } else {
    cptr=nptr;
  }

  // Convert to double
  double result=strtod(cptr,endptr);
  if(*endptr != NULL && cptr != nptr) {
    *endptr = const_cast<char*>(nptr) + (*endptr - const_cast<char*>(cptr));
  }

  // If extra buffer space was allocated above, then release it
  if(buf!=statbuf) delete[] buf;

  // All done!
  return result;
}

double Nb_Strtod(const unsigned char *nptr,unsigned char **endptr)
{
  const char* snptr = reinterpret_cast<const char*>(nptr);
  char** sendptr = reinterpret_cast<char**>(endptr);
  return Nb_Strtod(snptr,sendptr);
}

// Version of Nb_Atof with error checking.  Export value 'error'
// is set true if an error occurred, false otherwise.
double Nb_Atof(const char *nptr,OC_BOOL& error)
{
  if(nptr==NULL) { error=1; return 0.0; } // Null pointer check.
  /// Note that the Nb_Atof(const char*) function above fails
  /// if nptr==NULL.

  // Convert to double
  char* endptr;
  double result=Nb_Strtod(nptr,&endptr);

  // Check for errors
  if(endptr==nptr || *endptr!='\0') {
    error=1; // Error!
  } else {
    error=0; // Full conversion
  }

  // All done!
  return result;
}

// Overload of Nb_FloatToString template for long double case.  This
// version appends an "L" to the value.
String Nb_FloatToString(long double val)
{ // Output uses default float format, which is like %g
  std::stringbuf strbuf;
  std::ostream ostrbuf(&strbuf);
  ostrbuf << std::setprecision(std::numeric_limits<long double>::max_digits10)
          << val << "L";
  return strbuf.str();
}

//////////////////////////////////////////////////////////////////////////
// Atan2 inverse, in degrees.  If angle is an integer, then we can do
// range reduction on it without loss of precision.  This routine could
// undoubtably be shortened, but the overriding concern here is accuracy.
// NOTE: The import "angle" is in DEGREES!
void degcossin(double angle,double& cosine,double& sine)
{
  int sign=1;
  if(angle<0) { sign=-1; angle*=-1; }
  int base=int(floor(angle));
  angle-=base;
  base%=360;
  int quadrant=1;
  if(base>=270)      quadrant=4;
  else if(base>=180) quadrant=3;
  else if(base>=90)  quadrant=2;
  angle+=(base%90);
  int flip=0;
  if(angle>45) { angle=90.-angle; flip=1; }
  if(quadrant==2 || quadrant==4) flip=(flip+1)%2;
  double x,y;
  if(angle==0.0) {
    y=0.0; x=1.0;
  } else {
    angle*=(PI/180.);
    x=cos(angle);
    y=sin(angle);
    // x=sqrt(1.0-y*y);
    // x += (1-(x*x+y*y))/(2*x);  // Tiny linear correction
  }
  if(flip) { double temp=x; x=y; y=temp; }
  switch(quadrant)
    {
    case 2: x*=-1; break;
    case 3: x*=-1; y*=-1; break;
    case 4: y*=-1; break;
    }
  cosine=x;
  sine=y*sign;
}

//////////////////////////////////////////////////////////////////////////
// Routines to detect IEEE floating point NAN's and infinities.
// Note: The C99 spec includes an isfinite() function --- part of
//       the fpclassify package --- that can be used instead.
//       However, some compiler vendors have been slow to comply,
//       and anyway C99 is not C++.  So, for portability, we define
//       our own.
//       Of course, the code below ONLY WORKS FOR IEEE FLOATING POINT!
int Nb_IsFinite(OC_REAL4 x)
{
  // Inf's and nan's are indicated by all exponent bits (8) being set.
  // The top bit is the sign bit.
  unsigned char *cptr = (unsigned char *)(&x);
#if (OC_BYTEORDER == 4321)  // Little endian
  unsigned int code = (((unsigned int)cptr[3])<<8) + ((unsigned int)cptr[2]);
#else // Otherwise assume big endian
  unsigned int code = (((unsigned int)cptr[0])<<8) + ((unsigned int)cptr[1]);
#endif
  unsigned mask = 0x7F80;
  code &= mask;
  return (code!=mask);
}

int Nb_IsFinite(OC_REAL8 x)
{
  // Inf's and nan's are indicated by all exponent bits (11) being set.
  // The top bit is the sign bit.
  unsigned char *cptr = (unsigned char *)(&x);
#if (OC_BYTEORDER == 4321)  // Little endian
  unsigned int code = (((unsigned int)cptr[7])<<8) + ((unsigned int)cptr[6]);
#else // Otherwise assume big endian
  unsigned int code = (((unsigned int)cptr[0])<<8) + ((unsigned int)cptr[1]);
#endif
  unsigned mask = 0x7FF0;
  code &= mask;
  return (code!=mask);
}

#if !OC_REALWIDE_IS_OC_REAL8
int Nb_IsFinite(OC_REALWIDE x)
{
# if OC_REALWIDE_INTRINSIC_WIDTH == 8

  // Same tests as for OC_REAL8 case
  unsigned char *cptr = (unsigned char *)(&x);
#  if (OC_BYTEORDER == 4321)  // Little endian
  unsigned int code = (((unsigned int)cptr[7])<<8) + ((unsigned int)cptr[6]);
#  else // Otherwise assume big endian
  unsigned int code = (((unsigned int)cptr[0])<<8) + ((unsigned int)cptr[1]);
#  endif
  unsigned mask = 0x7FF0;
  code &= mask;
  return (code!=mask);

# elif  OC_REALWIDE_INTRINSIC_WIDTH == 16 && 105<LDBL_MANT_DIG && LDBL_MANT_DIG<108

  // Assume OC_REALWIDE is implemented as a double-double, and use REAL8 tests
  unsigned char *cptr = (unsigned char *)(&x);
#  if (OC_BYTEORDER == 4321)  // Little endian
  unsigned int code = (((unsigned int)cptr[7])<<8) + ((unsigned int)cptr[6]);
#  else // Otherwise assume big endian
  unsigned int code = (((unsigned int)cptr[0])<<8) + ((unsigned int)cptr[1]);
#  endif
  unsigned mask = 0x7FF0;
  code &= mask;
  return (code!=mask);

# elif  OC_REALWIDE_INTRINSIC_WIDTH == 10 || OC_REALWIDE_INTRINSIC_WIDTH == 16

  // For both 80-bit extended precision and 128-bit quad precision IEEE
  // formats, Inf's and nan's are indicated by all 15 exponent bits
  // being set.  (The top bit is the sign bit.)
  unsigned char *cptr = (unsigned char *)(&x);
#  if (OC_BYTEORDER == 4321)  // Little endian
  unsigned int code = (((unsigned int)cptr[OC_REALWIDE_INTRINSIC_WIDTH-1])<<8)
    + ((unsigned int)cptr[OC_REALWIDE_INTRINSIC_WIDTH-2]);
#  else // Otherwise assume big endian
  unsigned int code = (((unsigned int)cptr[0])<<8) + ((unsigned int)cptr[1]);
#  endif
  unsigned mask = 0x7FFF;
  code &= mask;
  return (code!=mask);

# else // OC_REALWIDE_INTRINSIC_WIDTH
#  error Unsupported OC_REALWIDE floating point type
# endif // OC_REALWIDE_INTRINSIC_WIDTH
}
#endif // !OC_REALWIDE_IS_OC_REAL8

//////////////////////////////////////////////////////////////////////////
// Routine to detect string containing nothing but whitespace.
int Nb_StrIsSpace(const char* str)
{
  if(str==NULL) return 1;
  do {
    char ch = *str;
    if(ch=='\0') break;
    if(!isspace(ch)) return 0; // Non-space, not '\0' character
  } while(++str);
  return 1; // Each string element satisfies isspace()==TRUE
}

//////////////////////////////////////////////////////////////////////////
// Portable case insensitive string comparison, modeled after
// strcasecmp().
int Nb_StrCaseCmp(const char *s1, const char *s2)
{
  const char *cptr1=s1,*cptr2=s2;
  int c1,c2;
  do {
    c1=tolower(*(cptr1++));
    c2=tolower(*(cptr2++));
  } while(c1==c2 && c1!='\0');
  if(c1<c2) return -1;
  if(c1>c2) return  1;
  return 0;
}

//////////////////////////////////////////////////////////////////////////
// Portable thread-safe string tokenizer, modeled after strsep().
// Compare also to strtok().
char* Nb_StrSep(char **stringp, const char *delim)
{
  if(*stringp == NULL) return NULL;
  char* token_start = *stringp + strspn(*stringp,delim);
  if(*token_start == '\0') {
    *stringp = token_start;
    return NULL;
  }
  char* token_end = token_start + strcspn(token_start,delim);
  if(*token_end=='\0') {
    *stringp = token_end;
  } else {
    *token_end='\0';
    *stringp = token_end + 1;
  }
  return token_start;
}

//////////////////////////////////////////////////////////////////////////
// Map color strings to real [0,1] rgb values.
// Returns 1 on success, 0 on error.
OC_BOOL Nb_GetColor(const char* color,
		 OC_REAL8m& red,OC_REAL8m& green,OC_REAL8m& blue)
{
  if(color==NULL || color[0]=='\0') return 0;

  // Translate "color" to lowercase and remove whitespace
  size_t size = strlen(color);
  char* buf = new char[size+1]; // Certainly big enough
  if(buf==NULL) return 0; // Safety
  const char* cptr1=color;
  char* cptr2=buf;
  char ch='\0';
  do {
    ch = *cptr1;
    ++cptr1;
    if(!isspace(ch)) {
      *cptr2 = static_cast<char>(tolower(ch));
      ++cptr2;
    }
  } while(ch!='\0');
  size = static_cast<size_t>(cptr2 - buf) - 1; // Resize

  OC_BOOL success=0;

  // Check to see if color is in Tk numeric color format
  if(buf[0] == '#') {
    size_t match_len = strspn(buf+1,"0123456789abcdef");
    if(match_len+1 == size) {
      unsigned int r=0,g=0,b=0;
      OC_REAL8m maxval = 1.0;
      switch(size) {
      case  4:
	if(sscanf(buf,"#%1x%1x%1x",&r,&g,&b)==3) {
          maxval = 0xF;
	  success=1;
	}
	break;
      case  7:
	if(sscanf(buf,"#%2x%2x%2x",&r,&g,&b)==3) {
          maxval = 0xFF;
	  success=1;
	}
	break;
      case 10:
	if(sscanf(buf,"#%3x%3x%3x",&r,&g,&b)==3) {
          maxval = 0xFFF;
	  success=1;
	}
	break;
      case 13:
	if(sscanf(buf,"#%4x%4x%4x",&r,&g,&b)==3) {
          maxval = 0xFFFF;
	  success=1;
	}
	break;
      default:
	success=0; // Safety
	break;
      }
      if(success) {
	red   = static_cast<OC_REAL8m>(r)/maxval;
	green = static_cast<OC_REAL8m>(g)/maxval;
	blue  = static_cast<OC_REAL8m>(b)/maxval;
      }
    }
  } else {
    // Check for gray# or grey# variants
    if(size>4 && size<8
       && buf[0]=='g' && buf[1]=='r' && (buf[2]=='a' || buf[2]=='e')
       && buf[3]=='y' && strspn(buf+4,"0123456789")==size-4) {
      int shade = atoi(buf+4);
      if(shade<=100) {
	red=green=blue=static_cast<OC_REAL8m>(shade)/100.;
 	success=1;
      }
    }
  }

  Tcl_Interp* interp=NULL;
  if(!success && (interp=Oc_GlobalInterpreter())!=NULL) {
    // If we get to here, then input string was either not in
    // Tk numeric color format, or else an error occurred
    // reading the string.  Assume symbolic name, and do a
    // lookup in the color database
    Tcl_SavedResult saved;
    Tcl_SaveResult(interp, &saved);

    const char* cptr=NULL;

    // First check to see if nbcolordatabase array has been
    // initialized.
    if(Tcl_GetVar2(interp,OC_CONST84_CHAR("nbcolordatabase"),
		   OC_CONST84_CHAR("black"),TCL_GLOBAL_ONLY)==NULL) {
      // Array not yet filled.  Evaluate a Tcl script to
      // source the file(s) specified in the Oc_Option
      // database.

      if(Tcl_GlobalEval(interp,
		OC_CONST84_CHAR("Oc_Option Get Color filename _fl ;"
				" foreach _f $_fl { source $_f }"))
	 != TCL_OK) {
	PlainWarning("Error sourcing color database file(s): %s",
		     Tcl_GetStringResult(interp));
	goto finished;
      }

      // Add black entry just to be sure that we don't
      // initialize again next time through.
      Tcl_SetVar2(interp,OC_CONST84_CHAR("nbcolordatabase"),
		  OC_CONST84_CHAR("black"),
		  OC_CONST84_CHAR("0 0 0"),TCL_GLOBAL_ONLY);
    }

    // Try to read color from database
    cptr = Tcl_GetVar2(interp,OC_CONST84_CHAR("nbcolordatabase"),
		       buf,TCL_GLOBAL_ONLY);
    if(cptr!=NULL) {
      // Success; Convert from hex triplet into floats
      unsigned int r,g,b;
      if(sscanf(cptr,"%X%X%X",&r,&g,&b)!=3) {
	PlainWarning("Invalid triplet in color database for color %s: %s",
		     buf,cptr);
      } else {
	red   = static_cast<OC_REAL8m>(r)/255.;
	green = static_cast<OC_REAL8m>(g)/255.;
	blue  = static_cast<OC_REAL8m>(b)/255.;
 	success=1;
      }
    }

  finished:
    Tcl_RestoreResult(interp, &saved);
  }

  delete[] buf;
  return success;
}

// Tcl wrapper for the above
int NbGetColor(ClientData,Tcl_Interp *interp,int argc,CONST84 char** argv)
{
  char buf[1024];
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),"Nb_GetColor must be called with 1 argument,"
	    " color (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_REAL8m red,green,blue;
  if(!Nb_GetColor(argv[1],red,green,blue)) {
    Oc_Snprintf(buf,sizeof(buf),"Unable to interpret color name \"%s\"",
		argv[1]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g %.17g",
              static_cast<double>(red),
              static_cast<double>(green),
              static_cast<double>(blue));
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

// C++ alternative to Tcl 'file exists'
OC_BOOL Nb_FileExists(const char *path)
{ // This code mimics the Tcl script command 'file exists'
  OC_BOOL success = 0;
  Oc_TclObj pathobj(path);
  if (Tcl_FSConvertToPathType(nullptr, pathobj.GetObj()) == TCL_OK) {
    success = (Tcl_FSAccess(pathobj.GetObj(), F_OK) == 0);
    // Tcl_FSAccess returns 0 on success, -1 otherwise
  }
  return success;
}

// C++ interface to the Tcl 'file join' command.
Nb_DString Nb_TclFileJoin(const Nb_List<Nb_DString>& parts)
{
  const OC_INDEX argc = parts.GetSize() + 2;
  Nb_DString* argv = new Nb_DString[size_t(argc)];
  argv[0] = "file";
  argv[1] = "join";
  int i=1;
  Nb_List_Index<Nb_DString> key;
  const Nb_DString* elt = parts.GetFirst(key);
  while(elt != NULL) {
    argv[++i] = *elt;
    elt = parts.GetNext(key);
  }
  Nb_DString cmd;
  assert(argc <= INT_MAX);
  cmd.MergeArgs(static_cast<int>(argc),argv);
  delete[] argv;
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,NULL,
                          "Nb_TclFileJoin",NB_FUNCTIONS_ERRBUFSIZE,
                          "No Tcl interpreter available"));
  }
  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  if(Tcl_Eval(interp,OC_CONST84_CHAR(cmd.GetStr())) != TCL_OK) {
    Oc_Exception foo(__FILE__,__LINE__,NULL,
                     "Nb_TclFileJoin",2001,
                     "%.2000s",Tcl_GetStringResult(interp));
    Tcl_DiscardResult(&saved);
    OC_THROW(foo);
  }
  Nb_DString result = Tcl_GetStringResult(interp);
  Tcl_RestoreResult(interp, &saved);
  return result;
}

// C++ interface to the Tcl 'glob' command.
// Import "options" is a string of glob options (multiple options
// allowed), and globstr is the glob string.  Option -nocomplain is
// included by default.  The export results is a (possibly empty) list
// of matching files, sorted by lsort.  The return value is the number
// of elements in the list.
OC_INDEX
Nb_TclGlob(const char* options,const char* globstr,
           Nb_List<Nb_DString>& results)
{
  Oc_AutoBuf cmd;
  cmd += "lsort [ glob -nocomplain ";
  cmd += options;
  cmd += " ";
  cmd += globstr;
  cmd += " ]";
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,NULL,
                          "Nb_TclGlob",NB_FUNCTIONS_ERRBUFSIZE,
                          "No Tcl interpreter available"));
  }
  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  if(Tcl_Eval(interp,OC_CONST84_CHAR(cmd.GetStr())) != TCL_OK) {
    Oc_Exception foo(__FILE__,__LINE__,NULL,
                     "Nb_TclGlob",2001,
                     "%.2000s",Tcl_GetStringResult(interp));
    Tcl_DiscardResult(&saved);
    OC_THROW(foo);
  }
  results.TclSplit(Tcl_GetStringResult(interp));
  Tcl_RestoreResult(interp, &saved);
  return results.GetSize();
}

//////////////////////////////////////////////////////////////////////////
// C++ interface to Oc_TempName.  NB: This uses the global Tcl interp,
// so it can only be called from the main thread.  The returned Nb_DString
// references a newly created, empty file, but the handle used to open
// the file has been closed.  It is the responsibility of the caller to
// delete this file.
Nb_DString Nb_TempName
(const char* baseprefix,
 const char* suffix,
 const char* basedir)
{
  Nb_DString argv[4];
  argv[0] = "Oc_TempName";
  argv[1] = baseprefix;
  argv[2] = suffix;
  argv[3] = basedir;
  Nb_DString cmd;
  cmd.MergeArgs(sizeof(argv)/sizeof(argv[0]),argv);
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,NULL,
                          "Nb_TempName",NB_FUNCTIONS_ERRBUFSIZE,
                          "No Tcl interpreter available"));
  }
  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  if(Tcl_Eval(interp,OC_CONST84_CHAR(cmd.GetStr())) != TCL_OK) {
    Oc_Exception foo(__FILE__,__LINE__,NULL,
                     "Nb_TempName",2001,
                     "%.2000s",Tcl_GetStringResult(interp));
    Tcl_DiscardResult(&saved);
    OC_THROW(foo);
  }
  Nb_DString result = Tcl_GetStringResult(interp);
  Tcl_RestoreResult(interp, &saved);
  return result;
}

// C++ interface to "Oc_Main GetOOMMFRootDir".  The return value is
// a convenience pointer to the buffer area of required import ab.
const char* Nb_GetOOMMFRootDir(Oc_AutoBuf& ab)
{
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,NULL,
                          "Nb_GetOOMMFRootDir",NB_FUNCTIONS_ERRBUFSIZE,
                          "No Tcl interpreter available"));
  }

  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  if(Tcl_Eval(interp,OC_CONST84_CHAR("Oc_Main GetOOMMFRootDir")) != TCL_OK) {
    Oc_Exception foo(__FILE__,__LINE__,NULL,
                     "Nb_GetOOMMFRootDir",2001,
                     "%.2000s",Tcl_GetStringResult(interp));
    Tcl_DiscardResult(&saved);
    OC_THROW(foo);
  }
  ab.Dup(Tcl_GetStringResult(interp));
  Tcl_RestoreResult(interp, &saved);
  return ab.GetStr();
}


// Slightly portable versions of fopen, remove, and rename
FILE *Nb_FOpen(const char *path,const char *mode)
{
  Oc_TclObj objpath(path);
  const void* nativepath = Tcl_FSGetNativePath(objpath.GetObj());
#if (OC_SYSTEM_TYPE != OC_WINDOWS)
  return fopen(static_cast<const char*>(nativepath),mode);
#else // Windows version. Uses WCHAR for intl char support.
  Oc_AutoWideBuf wmode(mode);
  return _wfopen(static_cast<const wchar_t*>(nativepath),wmode);
#endif
}

int
Nb_Remove(const char *path)
{
  Oc_TclObj objpath(path);
  const void* nativepath = Tcl_FSGetNativePath(objpath.GetObj());
#if (OC_SYSTEM_TYPE != OC_WINDOWS)
  return remove(static_cast<const char*>(nativepath));
#else // Windows version. Uses WCHAR for intl char support.
  return _wremove(static_cast<const wchar_t*>(nativepath));
#endif
}

void Nb_RenameNoInterp
(const char* oldpath,
 const char* newpath,
 int sync)
{ // This routine uses no Tcl interp access and so can be called from
  // any thread.  The sync parameter determines whether or not this all
  // blocks until the data is flushed to disk (sync == 1 == blocks, sync
  // == 0 == don't block).
  //
  // This routine throws an exception if an error occurs.
  //
  // On some platforms the sync parameter may be ignored.
  //
  // On Windows, the MoveFileEx function is used instead of the ANSI C
  // rename to allow overwrite of an existing file.  This also allows
  // moving between volumes, and provides international character
  // support.
  //
  // Beginning with Tcl 8.4, the Tcl C library has a number of Tcl_FS*
  // calls that provide cross-platform filesystem access, such as
  // Tcl_FSRenameFile and Tcl_FSCopyFile.  However, according to the
  // docs these routines only work if the source and target are on the
  // same filesystem; it is also not clear what happens if the target
  // exists, and depending on the filesystem these routines might not
  // even be implemented.  The Tcl_FS* routines are virtual filesystem
  // aware, so they may be useful in that circumstance.

  // Checks for case where oldpath and newpath point to the same file:
  // 1) Catch simplest case where strings are identical
  if(strcmp(oldpath,newpath)==0) return; // Nothing to do

  // Get normalized versions of paths
  Oc_TclObj oldobj(oldpath);  Oc_TclObj newobj(newpath);
  Tcl_Obj* oldnorm = Tcl_FSGetNormalizedPath(nullptr,oldobj.GetObj());
  Tcl_Obj* newnorm = Tcl_FSGetNormalizedPath(nullptr,newobj.GetObj());
  if(oldnorm == nullptr) {
    Oc_Exception foo(__FILE__,__LINE__,NULL,
                     "Nb_RenameNoInterp",1200,
                     "Error: Invalid oldpath spec: %.1000s",
                     oldpath);
    OC_THROW(foo);
  }
  if(newnorm == nullptr) {
    Oc_Exception foo(__FILE__,__LINE__,NULL,
                     "Nb_RenameNoInterp",1200,
                     "Error: Invalid newpath spec: %.1000s",
                     newpath);
    OC_THROW(foo);
  }
  if(strcmp(Tcl_GetString(oldnorm),Tcl_GetString(newnorm))==0) return; // Same file

  // Get native versions of paths for use by system calls. On Windows
  // the return is WCHAR*; otherwise it's char*.
  const void* oldnative = Tcl_FSGetNativePath(oldnorm);
  const void* newnative = Tcl_FSGetNativePath(newnorm);

  // On Windows, one can use the GetFileInformationByHandle call and
  // check the VolumeSerialNumber and FileIndex in the
  // BY_HANDLE_FILE_INFORMATION structure to see if oldpath and newpath
  // point to the same file.  But tests indicate (on Win 8.1, at least)
  // that if oldpath and newpath point to the same file, then MoveFileEx
  // will report success but not change anything.  Since MoveFileEx does
  // what we want, we don't bother with identity checks on Windows.

  // On unix systems, if oldpath and newpath resolve to the same file,
  // then one expects the C rename function to do the right thing, and
  // it does so on all the systems I've tested --- but there is no
  // guarantee in the C spec so it seems best to make an explicit test
  // using stat().  (BTW, the stat() call is supported in Windows, but
  // the st_ino field is meaningless on Windows so stat() can't be used
  // to test identity on Windows.)

#if (OC_SYSTEM_TYPE == OC_UNIX) || (OC_SYSTEM_TYPE == OC_DARWIN)
  /// Add more tests if an extant unix variant turns up w/o stat
  const char* oldfile = static_cast<const char*>(oldnative);
  const char* newfile = static_cast<const char*>(newnative);
  struct stat old_statbuf;
  if(stat(oldfile,&old_statbuf)!=0) {
      Oc_Exception foo(__FILE__,__LINE__,NULL,
                       "Nb_RenameNoInterp",1200,
                       "Error; can't stat source file %.1000s",
                       oldfile);
      OC_THROW(foo);
  }
  struct stat new_statbuf;
  if(stat(newfile,&new_statbuf)==0) {
    // If stat fails on newpath, assume newpath does not exist.
    if(old_statbuf.st_ino == new_statbuf.st_ino
       && old_statbuf.st_dev == new_statbuf.st_dev) {
      return;  // Same file; nothing to do
    }
  }
#elif (OC_SYSTEM_TYPE == OC_WINDOWS)
  // Windows has several _stat calls, but Windows file systems don't
  // do inodes so you can't do inode comparison as above.  One way is
  // to check to see if the files are the same is to open both files
  // and call GetFileInformationByHandle().  Compare the values of
  // dwVolumeSerialNumber, nFileIndexLow, and nFileIndexHigh.  But
  // hopefully MoveFileEx can figure this out and do the right thing.
  // Instead, just check that the source file exists.
  if(!Nb_FileExists(oldpath)) {
    Oc_Exception foo(__FILE__,__LINE__,NULL,
                     "Nb_RenameNoInterp",1200,
                     "Source file %.1000s does not exist",
                     oldpath);
    OC_THROW(foo);
  }
#endif

#if (OC_SYSTEM_TYPE == OC_WINDOWS)
  const WCHAR* oldfile = static_cast<const WCHAR*>(oldnative);
  const WCHAR* newfile = static_cast<const WCHAR*>(newnative);
  BOOL result = MoveFileExW(oldfile,newfile,
                            MOVEFILE_COPY_ALLOWED
                            | MOVEFILE_REPLACE_EXISTING
                            | (sync ? MOVEFILE_WRITE_THROUGH : 0));
  if(result == 0) { // Error
    String winerr = Oc_WinStrError(GetLastError());
    String errmsg = String("Error moving file ")
      + String(oldpath) + String(" to " )
      + String(newpath) + String(": " )
      + winerr;
    Oc_Exception foo(__FILE__,__LINE__,NULL,
                     "Nb_RenameNoInterp",errmsg.c_str());
    OC_THROW(foo);
  }
#else
  int errorcode = rename(oldfile,newfile);
  if(errorcode != 0) {
    // Some some failure occurred.  In particular, the ANSI C rename
    // call can fail if the paths are on different volumes.  Fall back
    // to a brute-force copy and delete.  (Note: Don't open the
    // target for writing if source doesn't exist; that case is an
    // error in which case we don't want the target truncated.)
    errorcode = 1;
    FILE* fin = fopen(oldfile,"rb");
    FILE* fout = (fin!=0 ? fopen(newfile,"wb") : 0);
    if(fin!=0 && fout!=0) {
      const unsigned int BSIZE = 32768;
      char buf[BSIZE];
      size_t count,recount;
      while(1) {
        count = fread(buf,1,BSIZE,fin);
        recount = fwrite(buf,1,count,fout);
        if(recount == BSIZE) continue;
        if(recount != count) break; // Write error
        if(feof(fin)) errorcode=0;
        break;
      }
    }
    if(fin) fclose(fin);
    if(fout) fclose(fout);
    if(errorcode == 0) {
      // Copy was successful.  Remove original file
      errorcode = remove(oldfile);
    }
    if(errorcode) {
      Oc_Exception foo(__FILE__,__LINE__,NULL,
                       "Nb_RenameNoInterp",2200,
                       "Error moving file %.1000s to %.1000s",
                       oldfile,newfile);
      OC_THROW(foo);
    }
  }
#endif

  if(sync) {
    // Force write to disk by opening newfile and flushing buffers.
    int sync_error = 0;
#if (OC_SYSTEM_TYPE == OC_WINDOWS)
    // Note: CreateFile() supports longer file names than OpenFile().
    HANDLE hFile = CreateFileW(newfile,GENERIC_WRITE,0,0,
                               OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(hFile == INVALID_HANDLE_VALUE) {
      ++sync_error;
    } else {
      if(!FlushFileBuffers(hFile)) {
        ++sync_error;
      }
      if(!CloseHandle(hFile)) {
        ++sync_error;
      }
    }
#elif (OC_SYSTEM_TYPE == OC_UNIX) || (OC_SYSTEM_TYPE == OC_DARWIN)
    /// Add more tests if an extant unix variant turns up w/o fsync
    int fd = open(newfile,O_RDWR);
    /// Note: O_APPEND not directly supported on NFS.
    if(fd == -1) {
      ++sync_error;
    } else {
      if(fsync(fd)!=0) {
        ++sync_error;
      }
      if(close(fd)!=0) {
        ++sync_error;
      }
    }
#endif
    if(sync_error) {
      Oc_Exception foo(__FILE__,__LINE__,NULL,
                       "Nb_RenameNoInterp",1200,
                       "Error syncing file %.1000s",
                       newpath);
      OC_THROW(foo);
    }
  }
}

// Interfaces to Tcl_Write.  The first is intended mainly
// as an interface layer in case we decide in the future to
// use Tcl_WriteObj in place of Tcl_Write.  NB: The buffer
// in Nb_WriteChannel is 'char*', not 'const char*'.  Blame pre-8.4
// Tcl_Write for this.  If bytecount is <0, then buf should
// be NULL terminated, and everything up to the NULL byte is
// output.  The return value is the number of bytes written,
// or -1 in case of error.
OC_INDEX
Nb_WriteChannel(Tcl_Channel channel,const char* buf,OC_INDEX bytecount)
{
  if(bytecount<0) { // Special case
    return static_cast<OC_INDEX>(Tcl_Write(channel,const_cast<char*>(buf),-1));
  }

  // According to the docs, the type specifier for the byte count
  // to Tcl_Write is an "int".  Handle case where import bytecount
  // is too big to fit in an "int"
  OC_INDEX bytes_out = 0;
  while(bytes_out<bytecount) {
    int attempt_size = INT_MAX;
    if(bytecount - bytes_out < OC_INDEX(attempt_size)) {
      attempt_size = int(bytecount-bytes_out);
    }
    if (attempt_size
        != Tcl_Write(channel,const_cast<char*>(buf),attempt_size)) {
      return -1; // Error
    }
    buf += attempt_size;
    bytes_out += attempt_size;
  }
  return bytecount;
}

OC_INDEX
Nb_FprintfChannel(Tcl_Channel channel,char* buf,OC_INDEX bufsize,
		      const char* format, ...)
{ // NOTE: This code is not re-entrant if buf==NULL.
  // If buf==NULL, then use local buffer.
  static char* localbuf=NULL;
  static OC_INDEX localbuf_size=0;
  if(buf==NULL) {
    OC_INDEX worksize=bufsize;
    if(worksize<1024) worksize=1024; // Min localbuf size
    if(worksize>localbuf_size) {
      // Allocate new buffer
      if(localbuf!=NULL) delete[] localbuf;
      localbuf_size=worksize;
      localbuf = new char[size_t(localbuf_size)];
    }
    buf=localbuf;
  }

  // Fill buf with formatted string
  va_list arg_ptr;
  va_start(arg_ptr,format);
  OC_INDEX len = Oc_Vsnprintf(buf,static_cast<size_t>(bufsize),format,arg_ptr);
  va_end(arg_ptr);
  if(len<0 || len>=bufsize) {
    /// NOTE: According to the Linux sprintf man page, as of glibc 2.1 the
    /// return value is the number of bytes (excluding the trailing '\0')
    /// that would have been written if enough space had been available.
    OC_THROW(Oc_Exception(__FILE__,__LINE__,NULL,
           "Nb_FprintfChannel",NB_FUNCTIONS_ERRBUFSIZE,
           "Write error (buffer overflow?)"));
  }

  if(len==0) return 0;

  // Write formatted string to channel.
  OC_INDEX result = Nb_WriteChannel(channel,buf,len);
  if(result != len) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,NULL,
           "Nb_FprintfChannel",NB_FUNCTIONS_ERRBUFSIZE,
           "Write error (device full?)"));
  }

  return static_cast<OC_UINT4m>(result);
}

// Interface to system user id information.  Tcl provides a version of
// this in the tcl_platform array, but some versions of Tcl on some
// platforms just grab this from the USER environment variable, which
// is not reliable.  (In particular, the user may lie!)

// The Unix version of Nb_GetUserName defined below is not thread
// safe, and fixing it is a bit of a PITA because of differing
// definitions of the getpwuid_r function on different unices (some
// implementations take 4 args, others take 5). We probably don't
// actually need Nb_GetUserName on Unix, and moreover Windows doesn't
// really support a numeric id, so rather than trying to fit prisms into
// cylindrical openings, we just do the following:
// On Unix: Define Nb_GetUserId (numeric form) only.
// On Windows: Define Nb_GetUserName (string form) and the related
//      Nb_GetPidUserName functions, but nothing returning a numeric id.
// On other: Don't define anything (at present).
#if (OC_SYSTEM_TYPE == OC_UNIX)
OC_BOOL Nb_GetUserId(OC_INT4m& numeric_id)
{ // Numeric form of user id (unix only).
  // Get "real user id"; any chance we might want
  // "effective user id" instead?
  numeric_id = static_cast<OC_INT4m>(getuid());
  return 1;
}

// Tcl wrapper around Nb_GetUserId
int NbGetUserId(ClientData,Tcl_Interp *interp,
                int argc,CONST84 char** /* argv */)
{ // Numeric form of user id (unix only).
  Tcl_ResetResult(interp);
  if(argc!=1) {
    char errbuf[1024];
    Oc_Snprintf(errbuf,sizeof(errbuf),"Nb_GetUserId must be called"
                " with no arguments (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,errbuf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_INT4m nid;  // Numeric version of user id
  Nb_GetUserId(nid);

  char numbuf[256];
  Oc_Snprintf(numbuf,sizeof(numbuf),"%d",nid);
  Tcl_AppendResult(interp,numbuf,(char *)NULL);

  return TCL_OK;
}

#if 0 && (OC_SYSTEM_SUBTYPE != OC_CYGWIN)
// Generally on Unix, one expects to use the numeric user id preferentially
// to the string user id, and given the re-entrancy issues with the
// Nb_GetUserName routine below, let's just not define it on Unix at least
// until we find an actual use for it.
OC_BOOL Nb_GetUserName(Nb_DString& string_id)
{ // String form of user id.
  // Get "real user id"; any chance we might want
  // "effective user id" instead?
  uid_t myuid = getuid();

  // Convert numeric uid to string form.  The getpwuid function used
  // below is not safe for use across multiple threads, because the
  // results may be stored in a common area.  Unfortunately (grrr) the
  // re-entrant version of this routine (getpwuid_r) comes in two
  // flavors, one that takes 4 args (e.g., Solaris) and one that takes 5
  // args (e.g., Linux).  This is a Tcl wrapper around getpwuid_r that
  // handles this (TclpGetPwUid), but I'm not sure in what release of
  // Tcl it first appears, and anyway it is not in the exposed API.
  // Hopefully, getpwuid and friends don't get called from more than one
  // thread.  Still, this is a potential security hole so it should be
  // fixed.

  passwd* mypwd = getpwuid(myuid);
  string_id = mypwd->pw_name;

  if(myuid != mypwd->pw_uid) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,NULL,
           "Nb_GetUserId",NB_FUNCTIONS_ERRBUFSIZE,
           "Corrupted user info (thread re-entrancy error?)"));
  }

  return 1; // Success
}

// Tcl wrapper around Nb_GetUserName
int NbGetUserName(ClientData,Tcl_Interp *interp,
                int argc,CONST84 char** /* argv */)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    char buf[16384];
    Oc_Snprintf(buf,sizeof(buf),"Nb_GetUserName must be called"
                " with no arguments (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Nb_DString sid; // String version of id
  if(!Nb_GetUserName(sid)) {
    Oc_AutoBuf ab = "Unable to determine user name";
    Tcl_AppendResult(interp,ab.GetStr(),(char *)NULL);
    return TCL_ERROR;
  }

  Oc_AutoBuf ab = sid.GetStr();
  Tcl_AppendResult(interp,ab.GetStr(),(char *)NULL);
  return TCL_OK;
}

#endif // 0
#endif // UNIX

#if (OC_SYSTEM_TYPE == OC_WINDOWS) || (OC_SYSTEM_SUBTYPE == OC_CYGWIN)
OC_BOOL Nb_GetUserName(Nb_DString& string_id)
{
  TCHAR namebuf[32767];
  DWORD namesize = sizeof(namebuf)/sizeof(namebuf[0]);
  if(!GetUserName(namebuf,&namesize)) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,NULL,
          "Nb_GetUserId",NB_FUNCTIONS_ERRBUFSIZE,
                        "Unable to get user name"));
  }

  string_id = namebuf;

  return 1; // Success
}

// Tcl wrapper around Nb_GetUserName
int NbGetUserName(ClientData,Tcl_Interp *interp,
                int argc,CONST84 char** /* argv */)
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    char buf[16384];
    Oc_Snprintf(buf,sizeof(buf),"Nb_GetUserName must be called"
                " with no arguments (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Nb_DString sid; // String version of id
  if(!Nb_GetUserName(sid)) {
    Oc_AutoBuf ab = "Unable to determine user name";
    Tcl_AppendResult(interp,ab.GetStr(),(char *)NULL);
    return TCL_ERROR;
  }

  Oc_AutoBuf ab = sid.GetStr();
  Tcl_AppendResult(interp,ab.GetStr(),(char *)NULL);
  return TCL_OK;
}

// Get username associated with a pid.  This will normally
// fail unless the owner of pid is the same as the owner
// of the current process.  For our uses, this probably
// suffices.
int Nb_GetPidUserName(OC_INT4m pid,Nb_DString& username)
{
  username = "";
  // Get handle to process
  HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION,0,
                            static_cast<DWORD>(pid));
  if(hProc == NULL) {
    return 1;  // Unable to get handle to process
  }

  HANDLE hToken;
  OC_BOOL result = OpenProcessToken(hProc,TOKEN_QUERY,&hToken);
  CloseHandle(hProc);
  if(result == 0) {
#if 0
    DWORD errid = GetLastError();
    fprintf(stderr,"Can't get process token: %llu\n",errid);
    LPTSTR buf;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,errid,0,(LPTSTR)&buf,0,NULL);
    fprintf(stderr,"ERRMSG: %s\n",buf);
    LocalFree(buf);
#endif
    return 2; // Unable to get process token
  }

  DWORD owner_size = 0;
  if(!GetTokenInformation(hToken,TokenOwner,NULL,
                          owner_size,&owner_size)
     && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    CloseHandle(hToken);
    return 3; // Can't get token owner (a)
  }

  PTOKEN_OWNER owner = (PTOKEN_OWNER)(new char[owner_size]);


  if(!GetTokenInformation(hToken,TokenOwner,owner,
                          owner_size,&owner_size)) {
    delete[] owner;
    CloseHandle(hToken);
    return 4; // Can't get token owner (b)
  }

  const DWORD max_len = 32767;
  TCHAR owner_string[max_len];
  DWORD owner_len=max_len;
  TCHAR domain_string[max_len];
  DWORD domain_len=max_len;
  SID_NAME_USE eUse;
  if(!LookupAccountSid(NULL,
                       owner->Owner,
                       owner_string,&owner_len,
                       domain_string,&domain_len,&eUse)) {
    delete[] owner;
    CloseHandle(hToken);
    return 5; // Can't convert to string representation
  }
  username = Oc_AutoBuf(domain_string);
  username += "\\";
  username += Oc_AutoBuf(owner_string);

  delete[] owner;
  CloseHandle(hToken);

  return 0; // Success
}

// Tcl wrapper around Nb_GetPidUserName.  Returns an empty string
// if the lookup fails for any reason.  Otherwise, returns a string
// of the form "domain\username".
int NbGetPidUserName(ClientData,Tcl_Interp *interp,
                     int argc,CONST84 char** argv)
{
  Tcl_ResetResult(interp);
  if(argc!=2) {
    char buf[16384];
    Oc_Snprintf(buf,sizeof(buf),"Nb_GetPidUserName must be called"
                " with 1 argument: PID (%d arguments passed).",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  OC_INT4m pid = atoi(argv[1]);
  Nb_DString username; // String version
  if(Nb_GetPidUserName(pid,username) == 0) {
    // On failure, return an empty string.
    Oc_AutoBuf ab = username.GetStr();
    Tcl_AppendResult(interp,ab.GetStr(),(char *)NULL);
  }
  return TCL_OK;
}
#endif // WINDOWS or CYGWIN
