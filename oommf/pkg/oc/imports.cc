/* FILE: imports.cc              -*-Mode: c++-*-
 *
 *	Replacement implementations for missing Tcl/Tk APIs
 * in old Tcl/Tk releases.
 *
 * Last modified on: $Date: 2015/07/17 22:45:44 $
 * Last modified by: $Author: donahue $
 */

/* Standard libraries */
#include <string.h>
#include <errno.h>
#include <float.h>

/* Other classes and functions in this extension.  In particular,
 * imports.h includes ocport.h, which defined the OC_TCL_TYPE
 * macro referenced below.
 */
#include "oc.h"

/* Cygwin-specific routines */
#if OC_TCL_TYPE==OC_WINDOWS && defined(__CYGWIN__)
# include <unistd.h>
# include <sys/cygwin.h>
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# undef WIN32_LEAN_AND_MEAN
#endif


OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported.
/// This insures standard math functions like floor, ceil, sqrt
/// are available.

/* End includes */     

//////////////////////////////////////////////////////////////////////////
// Random number generator routines.  The Oc_Random class (with
// Oc_RandomState helper) is an implementation of the GLIBC random()
// function.  This class provides the default functionality of the
// OMF_RANDOM macro declared in ocport.h
//    The Oc_UnifRand() function returns a random value in [0,1] with
// unif. distrib.  It is a wrapper for OMF_RANDOM.  It may be
// (re)initialized by calling Oc_Srand() or Oc_Srand(seed).  In the
// first case the seed is determined by sampling the system clock.
//
// NOTE: The Oc_Random generator state (of type Oc_RandomState) is
// stored in a static variable.  This means that the state is shared
// program-wide, across threads.  Mutexes protect against re-entrancy
// problems, but if multiple threads access Oc_Random then the results
// can vary from run-to-run, depending on the relative access order
// between threads.
//
void Oc_RandomState::Init(OC_UINT4m seed)
{
  int i;

  if(seed==0) seed=1; // Hack, to match glibc behavior.
  arr[0] = seed;
  ihead = 0;

  const OC_UINT4m MOD  = 0x7FFFFFFF;
  const OC_UINT4m MULT = 16807;
  
  for(i=1;i<SIZE-1;++i) {
    /* Note 1: If a<MOD and b<MOD, then a+b will fit inside
     *         an unsigned 4-byte int without overflow, because
     *         MOD < 2^31.
     * Note 2: If c<2^16, then c*MULT<MOD.
     * Note 3: 2^16 * 2^16 = 2^32 = 2*MOD + 2.  Hence,
     *         (d*2^32)%MOD = (d*2)%MOD.
     */
    OC_UINT4m rl = ((arr[i-1]) & 0xFFFF) * MULT;
    OC_UINT4m rh = ((arr[i-1]) >> 16) * MULT;

    OC_UINT4m accum = (rh & 0xFFFF) << 16;
    if(accum >= MOD) accum -= MOD;
    accum += rl;
    if(accum >= MOD) accum -= MOD;
    accum += (rh >> 16) * 2 ;

    if(accum >= MOD) accum -= MOD;
    arr[i] = accum;
  }
  for(i=SIZE-1;i<SIZE-1+SEP;++i) arr[i] = arr[i-(SIZE-1)];
  for(i=0;i<WARMUP;++i) Step();
}

OC_UINT4m Oc_RandomState::Step()
{ // Max return value is 0xFFFFFFFF
  int ia = ihead - SEP;      if(ia<0) ia += SIZE-1+SEP;
  int ib = ihead - (SIZE-1); if(ib<0) ib += SIZE-1+SEP;
  OC_UINT4m rnext = arr[ihead] = (arr[ia] + arr[ib]) & 0xFFFFFFFF;

  /// Portability warnings:
  ///   1) If OC_UINT4m is 32-bits (4 bytes), then arr[ia] + arr[ib]
  ///      may overflow.  This code assumes that in all cases the
  ///      lowest 32 bits are correct, and that higher bits, if
  ///      any, are dropped.  Indeed, the final '& 0xFFFFFFFF'
  ///      is a nop if sizeof(OC_UINT4m) is 4, and is only there to
  ///      handle the case where OC_UINT4m is bigger than 4 bytes.
  ///   2) On some architectures (e.g. DEC Alpha), 4-byte unsigned
  ///      arithmetic is slower than 4 or 8 byte signed or 8 byte
  ///      unsigned.  The arr type may be changed (without affecting
  ///      the results) to 8 byte signed or unsigned, or to 4 byte
  ///      signed if the signed arithmetic is handled via two's
  ///      complement arithmetic.

  if(++ihead>=SIZE-1+SEP) ihead = 0;

  return rnext;
}

Oc_RandomState Oc_Random::state;
#if OOMMF_THREADS
Tcl_Mutex Oc_Random::random_state_mutex = 0;  // Thread-safe hack.
#endif

void Oc_Srand(unsigned int seed)
{
  const int exercise=100;
  OMF_SRANDOM(seed+1); // On linux systems (others?), srandom(0) and
  /// srandom(1) generate the same sequence.  This is a) dumb, and
  /// b) isn't documented in the man pages.  Since 0 and 1 are likely
  /// to be common requests, work around this unexpected behavior by
  /// bumping all seeds up one.  BTW, on linux systems, srand/rand
  /// are the same as srandom/random.
  for(int i=0;i<exercise;i++) {
    OMF_RANDOM();  // "Exercise" generator, to keep it fit and trim.
  }
}

void Oc_Srand()
{
  unsigned int seed=0;
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL) seed=42;  // No interpreter available
  else {
    Tcl_SavedResult saved;
    Tcl_SaveResult(interp, &saved);
    // Use Tcl 'clock clicks' command to get a seed
    Tcl_Eval(interp,OC_CONST84_CHAR("clock clicks"));
    seed=(unsigned int)atol(Tcl_GetStringResult(interp));
    Tcl_RestoreResult(interp, &saved);
  }
  Oc_Srand(seed);
}

double Oc_UnifRand()
{ // Returns random value in [0,1] with unif. distrib.
  return ((double)OMF_RANDOM())/((double)OMF_RANDOM_MAX);
}

double Oc_UnifRand(Oc_RandomState& mystate)
{ // Returns random value in [0,1] with unif. distrib.  Note: Unlike
  // Oc_UnifRand(), this routine does not use OMF_RANDOM wrappers.  This
  // means that this routine can not be overridden by a user-specified
  // random number generator.
  return ((double)Oc_Random::Random(mystate))/((double)Oc_Random::MaxValue());
}


// Tcl wrappers for Oc_Srand and Oc_UnifRand
int OcSrand(ClientData,Tcl_Interp *interp,int argc,CONST84 char** argv)
{
  char buf[256];
  Tcl_ResetResult(interp);
  if(argc>2) {
    Oc_Snprintf(buf,sizeof(buf),
		"wrong # args: should be \"%.100s ?arg?\"",argv[0]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  if(argc<2) {
    Oc_Srand(); // Use clock-based seed
  } else {
    char* endptr;
    unsigned long int ulseed = strtoul(argv[1],&endptr,10);
    if(argv[1][0]=='\0' || *endptr!='\0') {
      // argv[1] is not an integer
      Oc_Snprintf(buf,sizeof(buf),
		  "bad seed \"%.100s\": must be integer",argv[1]);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }
    Oc_Srand(static_cast<unsigned int>(ulseed));
  }

  return TCL_OK;
}

int OcUnifRand(ClientData,Tcl_Interp *interp,int argc,CONST84 char** argv)
{
  static char buf[256];
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Oc_Snprintf(buf,sizeof(buf),
		"wrong # args: should be \"%.100s\"",argv[0]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  Oc_Snprintf(buf,sizeof(buf),"%.17g",Oc_UnifRand());
  Tcl_AppendResult(interp,buf,(char *)NULL);

  return TCL_OK;
}

#if (TCL_MAJOR_VERSION == 7)
// Tcl 7.x doesn't have rand() and srand() in expr.  Provide
// doppelgangers to Tcl 8.x versions (q.v.).
//
// NOTE 1: This implementation differs from the Tcl 8.x version in
//         that one seed is shared throughout the program, whereas
//         in Tcl each interp has its own seed.
// 
// NOTE 2: This code is not thread safe, but Tcl 7.x doesn't do
//         threads so thread-safety is a non-issue.
//
static int oc_tcl_randseed = 0;
double OcTclRand()
{
  const int RAND_IA =      16807;
  const int RAND_IM = 2147483647;  // largest positive 32-bit value
  const int RAND_IQ =     127773;
  const int RAND_IR =       2836;

  if(oc_tcl_randseed==0) {
    // randseed not set.  Get value from clock.
    Tcl_Interp* interp=Oc_GlobalInterpreter();
    if(interp==NULL) oc_tcl_randseed=42;  // No interpreter available
    else {
      Tcl_SavedResult saved;
      Tcl_SaveResult(interp, &saved);
      // Use Tcl 'clock clicks' command to get a seed
      int click_safety = 0;
      while(oc_tcl_randseed==0) {
        Tcl_Eval(interp,OC_CONST84_CHAR("clock clicks"));
        oc_tcl_randseed=atoi(Tcl_GetStringResult(interp));
        if(++click_safety>100) break;
      }
      if(oc_tcl_randseed==0) oc_tcl_randseed=42; // Punt
      Tcl_RestoreResult(interp, &saved);
    }
  }

  int tmp = oc_tcl_randseed/RAND_IQ;
  oc_tcl_randseed = RAND_IA*(oc_tcl_randseed - tmp*RAND_IQ) - RAND_IR*tmp;
  if (oc_tcl_randseed < 0) {
    oc_tcl_randseed += RAND_IM;
  }

  // Mask for 64-bit archs.
  oc_tcl_randseed &= ((((unsigned long) 0xfffffff) << 4) | 0xf);
  return oc_tcl_randseed * (1.0/RAND_IM);
}

int OcSrandTclMathProc(ClientData,Tcl_Interp*,
		       Tcl_Value* args,Tcl_Value* resultPtr)
{
  const int RAND_OFFSET = 123459876;
  if(args[0].type != TCL_INT) return TCL_ERROR; // Safety
  oc_tcl_randseed = args[0].intValue;
  if(oc_tcl_randseed==0) {
    // 0 seed breaks generator.  So just change it.
    oc_tcl_randseed = RAND_OFFSET;
  }
  resultPtr->type = TCL_DOUBLE;
  resultPtr->doubleValue = OcTclRand();
  return TCL_OK;
}

int OcUnifRandTclMathProc(ClientData,Tcl_Interp*,
			  Tcl_Value*,Tcl_Value* resultPtr)
{
  resultPtr->type = TCL_DOUBLE;
  resultPtr->doubleValue = OcTclRand();
  return TCL_OK;
}

#endif // TCL_MAJOR_VERSION == 7

//////////////////////////////////////////////////////////////////////////
/* Domain checked atan2 functions */
double Oc_Atan2(double y,double x)
{
  // The following #define trick allows atan2 to be defined
  // to be Oc_Atan2 in ocport.h, but allows us to call the system
  // atan2 function from inside this routine.  When the atan2 call
  // is processed by the compiler, if it has previously been
  // #define'd to be Oc_Atan2, then the macro #define'd here kicks
  // in and sets it back to atan2.  Since atan2 is the name of the
  // original macro that is being expanded, macro expansion stops
  // (as per Section 16.3 (?) of the ARM).  If atan2 was not
  // #define'd, or if it was #define'd to something other than
  // Oc_Atan2, then the macro here has no effect.
#define Oc_Atan2(y,x) atan2((y),(x))
  if(x==0.0 && y==0.0) return 0.0;
  return atan2(y,x);
#undef Oc_Atan2
}

/* Tcl wrapper for the above */
int
Oc_TclWrappedAtan2(ClientData, Tcl_Interp *interp,
		   Tcl_Value *args, Tcl_Value *resultPtr)
{
    errno = 0;
    double result = Oc_Atan2(args[0].doubleValue,args[1].doubleValue);
    if (errno != 0) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp,"errno set to ",NULL);
	switch (errno) {
	    case ERANGE:
		Tcl_AppendResult(interp,"ERANGE",NULL);
	    case EDOM:
		Tcl_AppendResult(interp,"EDOM",NULL);
	    default:
		Tcl_AppendResult(interp,"unexpected value",NULL);
	}
	Tcl_AppendResult(interp," by atan2()",NULL);
	return TCL_ERROR;
    }
    resultPtr->doubleValue = result;
    resultPtr->type = TCL_DOUBLE;
    return TCL_OK;
}

//////////////////////////////////////////////////////////////////////////
// Routines to calculate log(1+x), accurate for small x.
// The log1p function is in the C99 spec.  That version
// is probably fast and more accurate than the version below,
// so should probably change the code to use that if available.
double Oc_Log1p(double x)
{ if(fabs(x)>=0.5) return log(1.0+x);
  // Otherwise, use little trick.  One should check that
  // compiler doesn't screw this up with "extra" precision.
  double y1 = Oc_Nop(1.0 + x);
  if(y1 == 1.0) return x;
  double y2 = y1 - 1.0;
  double rat = log(y1)/y2;
  return x*rat;
}

OC_REALWIDE Oc_Log1pRW(OC_REALWIDE x)
{ // OC_REALWIDE version
  if(Oc_FabsRW(x)>0.5) return Oc_LogRW(1.0+x);
  OC_REALWIDE y1 = Oc_Nop(1.0 + x);
  if(y1 == OC_REALWIDE(1.0)) return x;
  OC_REALWIDE y2 = y1 - OC_REALWIDE(1.0);
  OC_REALWIDE rat = Oc_LogRW(y1)/y2;
  return x*rat;
}

//////////////////////////////////////////////////////////////////////////
// Utility filesystem command
//  Oc_FSync mimics the unix fysnc() or Windows _commit()/FlushFileBuffers()
// routines, which force data buffered by the OS to disk.  Note that this
// code may block, so only call when necessary.
// The return value is 0 on success, -1 on error.
int Oc_Fsync(Tcl_Channel chan)
{
  int errcode = 0;
  Tcl_Flush(chan);
  ClientData cd;
  if(Tcl_GetChannelHandle(chan,TCL_WRITABLE,&cd)!=TCL_OK) {
    return -1;
  }

#if OC_SYSTEM_TYPE==OC_UNIX
  // The FileGetHandleProc routine in tclUnixChan.c passes the (int)
  // file descriptor through client data via the Tcl INT2PTR macro.
  // Presumably the proper way to cast back from client data to an int
  // is through a signed int with width >= pointer width.  (It would be
  // nice for Tcl to provide an interface for this.)
  int handle = reinterpret_cast<OC_INDEX>(cd);
  errcode = fsync(handle);

#elif OC_SYSTEM_TYPE==OC_WINDOWS
  // The FileGetHandleProc routine in tclWinChan.c passes the file
  // descriptor (of type HANDLE) by casting from HANDLE directly to
  // ClientData.  So we should be able to cast directly back.  (Both
  // ClientData and HANDLE are, under the hood, void*.)
  HANDLE handle  = reinterpret_cast<HANDLE>(cd);
  errcode = !(FlushFileBuffers(handle));

#endif // OC_SYSTEM_TYPE

  return ( errcode == 0 ? 0 : -1 );
}

//////////////////////////////////////////////////////////////////////////
// Utility function for computing polynomials of order N
double Oc_Polynomial(double x, const double *coef, unsigned int N) {
    double ans = *coef++;
    while (N--) {
	ans = ans * x + *coef++;
    }
    return ans;
}

// Fallback implementation for those systems where the math
// library fails to provide an erf() function.  On those
// systems a macro in ocport.h will #define erf(x) to be Oc_Erf((x)).
//
// This implementation is one provided by Abdullah Daoud which has
// been passed down in the Institute for Magnetics Research at GWU.
// We should replace it with an implementation from GAMS with known
// numeric properties when we have time.
double Oc_Erf(double x)
{
    const double A[] = {
	 1.061405429,
	-1.453152027,
	 1.421413714,
	-0.284496736,
	 0.254829592,
	 0.0
    };
    const double p	= 0.3275911;
    double t		= 1.0 / (1.0 + p*fabs(x));
    double erfx = 1.0 - exp(-x*x) * Oc_Polynomial(t, A, 5);
    if (x>0) {
        return erfx;
    } else {
        return -erfx;
    }
}

//////////////////////////////////////////////////////////////////
// Version of chdir for use when using the cygwin libraries.  The
// problem arises when a Windows version of Tcl/Tk is used in the
// cygwin environment to build and run OOMMF.  (This is done because
// the cygwin-specific version of tclsh 8.4.1 has broken socket
// handling.)  In this circumstance a 'cd' command at the Tcl script
// level changes the Tcl and Windows view of the current working
// directory (cwd), but not the cygwin view of the cwd.  This leads to
// problems if a later call is made through the cygwin library to
// access a file via a path relative to the cwd.
//   This code is based on code from the Windows-specific branch
// of the Tcl sources.
#if OC_TCL_TYPE==OC_WINDOWS && defined(__CYGWIN__)
int OcCygwinChDir(ClientData,Tcl_Interp *interp,
                  int argc,CONST84 char** argv)
{
  char buf[4096+MAX_PATH+1];
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),
		"wrong # args: should be \"%.500s ?dirName?\"",argv[0]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Tcl_Obj* pathobj = Tcl_NewStringObj(argv[1],-1);
  Tcl_IncrRefCount(pathobj);
  Tcl_DString ds;
  CONST TCHAR *natpath = (CONST TCHAR *) Tcl_FSGetNativePath(pathobj);
  cygwin_conv_to_posix_path(Tcl_WinTCharToUtf(natpath,-1,&ds),buf);
  Tcl_DStringFree(&ds);
  Tcl_DecrRefCount(pathobj);
  pathobj = NULL; // Safety

  if(chdir(buf)!=0) {
    // Should probably include better error reporting here.
    Oc_Snprintf(buf,sizeof(buf),
		"Unable to change working directory to \"%.500s\"",
                argv[1]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}
#endif // OC_WINDOWS && __CYGWIN__


//////////////////////////////////////////////////////////////////////////
// Implementation of hypot(x,y) for use on systems where it isn't
// in the system libraries.  On those systems a macro in ocport.h
// will #define hypot(x,y) to be Oc_Hypot((x),(y)).
//    The accuracy of Oc_Hypot is close to but occasionally
// slightly less than what is obtained from the library version
// (where available), and is in all likelihood slower.
double Oc_Hypot(double x,double y)
{
  x = fabs(x);
  y = fabs(y);
  if(y<x) {
    double t = y; y = x; x = t;
  }
  if(y>0.5*sqrt(DBL_MAX)) { // y too big to square
    x /= y;
    return y*sqrt(1.0+x*x);
  }
  if(x<sqrt(DBL_MIN)) { // x too small to square
    if(y<x*sqrt(DBL_MAX)) {
      y /= x;
      return x*sqrt(1.0+y*y);
    }
  }
  return sqrt(x*x+y*y);
}


//////////////////////////////////////////////////////////////////////////
// Versions of exp() and pow() functions that will not raise exceptions
// on underflow.
int
Oc_Exp(ClientData, Tcl_Interp *interp, Tcl_Value *args, Tcl_Value *resultPtr)
{
    errno = 0;
    double result = exp(args[0].doubleValue);
    if ((errno == ERANGE) && (result == 0.0)) {
	// Special case.  Underflow.  Raise no exception.
    } else if (errno != 0) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp,"errno set to ",NULL);
	switch (errno) {
	    case ERANGE:
		Tcl_AppendResult(interp,"ERANGE",NULL);
	    case EDOM:
		Tcl_AppendResult(interp,"EDOM",NULL);
	    default:
		Tcl_AppendResult(interp,"unexpected value",NULL);
	}
	Tcl_AppendResult(interp," by exp()",NULL);
	return TCL_ERROR;
    }
    resultPtr->doubleValue = result;
    resultPtr->type = TCL_DOUBLE;
    return TCL_OK;
}

int
Oc_Pow(ClientData, Tcl_Interp *interp, Tcl_Value *args, Tcl_Value *resultPtr)
{
    errno = 0;
    double result = pow(args[0].doubleValue,args[1].doubleValue);
    if ((errno == ERANGE) && (result == 0.0)) {
	// Special case.  Underflow.  Raise no exception.
    } else if (errno != 0) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp,"errno set to ",NULL);
	switch (errno) {
	    case ERANGE:
		Tcl_AppendResult(interp,"ERANGE",NULL);
	    case EDOM:
		Tcl_AppendResult(interp,"EDOM",NULL);
	    default:
		Tcl_AppendResult(interp,"unexpected value",NULL);
	}
	Tcl_AppendResult(interp," by pow()",NULL);
	return TCL_ERROR;
    }
    resultPtr->doubleValue = result;
    resultPtr->type = TCL_DOUBLE;
    return TCL_OK;
}

// Wrapper to bring variants of math functions into expr on an
// as needed basis.
void Oc_AddTclExprExtensions(Tcl_Interp* interp) {
  // If Tcl 7.x, add rand() and srand() functions to expr.
  // Note: Tcl_CreateMathFunc copies the contents of the argTypes
  //  array into an internal structure, so argTypes may be freed
  //  afterwards.  This is not actually documented, but is expected,
  //  and moreover is the way it is done in the Tcl 7.5 and 7.6
  //  code base, which is all we care about.
#if (TCL_MAJOR_VERSION == 7)
  Tcl_ValueType OcSrandTclMathProcArgTypes[1] = { TCL_INT };
  Tcl_CreateMathFunc(interp,OC_CONST84_CHAR("srand"),1,
		     OcSrandTclMathProcArgTypes,
		     (Tcl_MathProc *)OcSrandTclMathProc,
		     (ClientData)NULL);
  Tcl_CreateMathFunc(interp,OC_CONST84_CHAR("rand"),0,NULL,
		     (Tcl_MathProc *)OcUnifRandTclMathProc,
		     (ClientData)NULL);
#endif // TCL_MAJOR_VERSION == 7

  // Replace the exp() and pow() functions of Tcl's [expr] with versions
  // that will not raise exceptions on underflow.
  Tcl_ValueType argTypes[2] = {TCL_DOUBLE, TCL_DOUBLE};

  if (TCL_ERROR == Tcl_Eval(interp,OC_CONST84_CHAR("expr exp(-1000.)"))) {
    // exp() sets errno on underflow.  Wrap it.
    Tcl_CreateMathFunc(interp,OC_CONST84_CHAR("exp"),1,argTypes,Oc_Exp,NULL);
  }
  if (TCL_ERROR == Tcl_Eval(interp,OC_CONST84_CHAR("expr pow(10., -1000.)"))) {
    // pow() sets errno on underflow.  Wrap it.
    Tcl_CreateMathFunc(interp,OC_CONST84_CHAR("pow"),2,argTypes,Oc_Pow,NULL);
  }
  if (TCL_ERROR == Tcl_Eval(interp,OC_CONST84_CHAR("expr atan2(0.,0.)"))) {
    // atan2(0,0) throws domain error.  Arguably correct, but
    // inconvenient.  Wrap it.
    Tcl_CreateMathFunc(interp,OC_CONST84_CHAR("atan2"),2,argTypes,
                       Oc_TclWrappedAtan2,NULL);
  }
}

int OcAddTclExprExtensions(ClientData,Tcl_Interp *interp,
			   int argc,CONST84 char** argv)
{
  static char buf[4096];
  Tcl_ResetResult(interp);
  if(argc>2) {
    Oc_Snprintf(buf,sizeof(buf),
		"wrong # args: should be \"%.100s\" ?interp?",argv[0]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  if(argc==1) {
    // Add extensions to current interpreter
    Oc_AddTclExprExtensions(interp);
  } else {
    // Add extensions to child interpreter
    Tcl_Interp* slave = Tcl_GetSlave(interp,argv[1]);
    if(slave==NULL) {
      Oc_Snprintf(buf,sizeof(buf),"No slave interpreter named \"%.256s\"",
		  argv[1]);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      return TCL_ERROR;
    }
    Oc_AddTclExprExtensions(slave);
  }
  return TCL_OK;
}



#if (TCL_MAJOR_VERSION < 8)
int
Tcl_GetChannelHandle(Tcl_Channel channel, int dir, ClientData *handlePtr)
{
    Tcl_File tfile=Tcl_GetChannelFile(channel,dir);
    if(tfile==NULL) return TCL_ERROR;
    *handlePtr=Tcl_GetFileInfo(tfile,NULL);
    return TCL_OK;
}
#endif

#if ((TCL_MAJOR_VERSION < 8) || ((TCL_MAJOR_VERSION == 8) \
    && (TCL_MINOR_VERSION == 0) && (TCL_RELEASE_SERIAL < 6)))
char *
Tcl_PkgPresent(Tcl_Interp* interp, char* name, char* version, int exact)
{
    // Need Oc_AutoBuf to support Tcl 7.
    // Starting with Tcl 8.0p0, the interface is CONST
    Oc_AutoBuf ab;
    Tcl_DString buf;
    Tcl_DString loadedVersion;

    /*
     * Use [package provide] to query if the named package is
     * already in the interpreter.
     */ 

    Tcl_DStringInit(&buf);
    Tcl_DStringAppend(&buf, ab("package provide"), -1);
    Tcl_Eval(interp, Tcl_DStringAppendElement(&buf, name));
    Tcl_DStringFree(&buf);

    Tcl_DStringInit(&loadedVersion);
    Tcl_DStringGetResult(interp, &loadedVersion);
    if ( !(Tcl_DStringLength(&loadedVersion)) ) {
        Tcl_DStringFree(&loadedVersion);
        if (version != NULL) {
            Tcl_AppendResult(interp, "package ", name, " ", version,
                    " is not present", (char *) NULL);
        } else {
            Tcl_AppendResult(interp, "package ", name, 
                    " is not present", (char *) NULL);
        }
        return NULL;
    }
    Tcl_DStringFree(&loadedVersion);

    /*
     * At this point we know some version is loaded, so 
     * Tcl_PkgRequire will have no side effect.  Call it
     * to do the rest of the work.
     */

    return Tcl_PkgRequire(interp, name, version, exact);
}
#endif

#if ((TCL_MAJOR_VERSION < 8) \
	|| ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION == 0)))
void
Tcl_SaveResult(Tcl_Interp *interp, Tcl_SavedResult *savedPtr)
{
    char *result = Tcl_GetStringResult(interp);

    if (interp->freeProc == TCL_STATIC) {
	/*
	 * String might go away with another Tcl_Eval() call,
	 * so make a copy.
	 */
	int length = strlen(result);
	savedPtr->result = Tcl_Alloc(length + 1);
	strcpy(savedPtr->result, result);
	savedPtr->freeProc = TCL_DYNAMIC;
    } else {
	savedPtr->result = result;
	savedPtr->freeProc = interp->freeProc;
	/*
	 * We've taken responsibility for savedPtr->result, so make
	 * sure the interp doesn't free it for us.
	 */
	interp->freeProc = TCL_STATIC;
    }
    Tcl_ResetResult(interp);
}

void
Tcl_RestoreResult(Tcl_Interp *interp, Tcl_SavedResult *savedPtr)
{
    Tcl_SetResult(interp, savedPtr->result, savedPtr->freeProc);
}

void
Tcl_DiscardResult(Tcl_SavedResult *savedPtr)
{
    if (savedPtr->freeProc == TCL_DYNAMIC) {
	Tcl_Free(savedPtr->result);
    } else {
	(*savedPtr->freeProc)(savedPtr->result);
    }
}
#endif

#if (TK_MAJOR_VERSION < 8)
/*
 * We have not yet implemented a "real" Tcl 7.x version of the null
 * channels, so we have no console channel replacements that will work.
 * If we ever do, remove this restriction.
 * For now, EOF on stdin will kill shells build with Tcl 7.
 */
# if OC_USE_TK
void Tk_InitConsoleChannels(Tcl_Interp*) {}
# endif /* OC_USE_TK */
#else
/*
 * In Tcl 8.x, a set of null channels can replace the standard channels
 * to prevent EOF on stdin, or other problems from terminating the shell
 * too soon, until a working version is provided by Tk.  
 */

#  if ((OC_SYSTEM_TYPE == OC_WINDOWS) \
	&& (TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION < 2)) \
	|| (0 && (OC_SYSTEM_TYPE == OC_UNIX) && (OC_SYSTEM_SUBTYPE != OC_DARWIN))
/*
 * Tk_InitConsoleChannels was first made public in Tk 8.2.
 * But on Unix, it breaks [exec].  
 *
 * When it's not provided, or broken, provide the following replacement.
 */

void
Tk_InitConsoleChannels(Tcl_Interp* interp)
{
    Tcl_Channel channel;

    channel = Nullchannel_Open();
    if (channel != NULL) {
        Tcl_SetChannelOption(NULL, channel,
			     OC_CONST84_CHAR("-translation"),
			     OC_CONST84_CHAR("lf"));
        Tcl_SetChannelOption(NULL, channel,
			     OC_CONST84_CHAR("-buffering"),
			     OC_CONST84_CHAR("none"));
#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 1)
	Tcl_SetChannelOption(NULL, channel,
			     OC_CONST84_CHAR("-encoding"),
			     OC_CONST84_CHAR("utf-8"));
#endif
    }
    Tcl_SetStdChannel(channel, TCL_STDIN);
    Tcl_RegisterChannel(interp, channel);

    channel = Nullchannel_Open();
    if (channel != NULL) {
        Tcl_SetChannelOption(NULL, channel,
			     OC_CONST84_CHAR("-translation"),
			     OC_CONST84_CHAR("lf"));
        Tcl_SetChannelOption(NULL, channel,
			     OC_CONST84_CHAR("-buffering"),
			     OC_CONST84_CHAR("none"));
#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 1)
	Tcl_SetChannelOption(NULL, channel,
			     OC_CONST84_CHAR("-encoding"),
			     OC_CONST84_CHAR("utf-8"));
#endif
    }
    Tcl_SetStdChannel(channel, TCL_STDOUT);
    Tcl_RegisterChannel(interp, channel);

#if (OC_SYSTEM_TYPE == OC_WINDOWS)
    /*
     * A fake stderr is what breaks [exec] on Unix.  Don't do it.
     */
    channel = Nullchannel_Open();
    if (channel != NULL) {
        Tcl_SetChannelOption(NULL, channel,
			     OC_CONST84_CHAR("-translation"),
			     OC_CONST84_CHAR("lf"));
        Tcl_SetChannelOption(NULL, channel,
			     OC_CONST84_CHAR("-buffering"),
			     OC_CONST84_CHAR("none"));
#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION >= 1)
	Tcl_SetChannelOption(NULL, channel,
			     OC_CONST84_CHAR("-encoding"),
			     OC_CONST84_CHAR("utf-8"));
#endif
    }
    Tcl_SetStdChannel(channel, TCL_STDERR);
    Tcl_RegisterChannel(interp, channel);
#endif

}

#  endif		
#endif

