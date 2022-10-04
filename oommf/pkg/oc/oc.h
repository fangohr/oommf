/* FILE: oc.h                    -*-Mode: c++-*-
 *
 *	The OOMMF Core extension public header file.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/10/09 21:23:12 $
 * Last modified by: $Author: donahue $
 */

#ifndef _OC
#define _OC

/*
 * Get version macros from another file so they can be
 * automatically generated
 */
#include "version.h"

/* Header containing macros and typedefs describing the platform */
#include "ocport.h"

/* The various levels of SSE instructions and instrinsic
 * header files are:
 *
 *    mmintrin.h    MMX
 *   xmmintrin.h    SSE
 *   emmintrin.h    SSE2
 *   pmmintrin.h    SSE3
 *   tmmintrin.h    SSSE3 (supplemental SSE3)
 *   ammintrin.h    SSE4A
 *   bmmintrin.h    SSE5
 *
 * At present OOMMF only makes use of SSE2.
 * The macro OC_USE_SSE is defined in ocport.h
 */
#if OC_USE_SSE
# include <emmintrin.h>
#endif

/* System headers */
#include <string>

/* Prototypes imported from other extensions and libraries */
#include "imports.h"

/* Prototypes of functions in this extension */
#include "byte.h"
#include "messages.h"
#include "nullchannel.h"

/* Declarations of classes in this extension */
#include "autobuf.h"
#include "ocalloc.h"
#include "ocexcept.h"
#include "ocfpu.h"
#include "ocsse.h"
#include "octhread.h"
#include "ocnuma.h"

/* End includes */     /* Optional directive to pimake */

// Note: OC_STRINGIFY and OC_JOIN are #define'd in autobuf.h
#define OC_MAKE_VERSION(x) OC_STRINGIFY(OC_JOIN(x,_MAJOR_VERSION)) "." \
                           OC_STRINGIFY(OC_JOIN(x,_MINOR_VERSION)) \
                           OC_JOIN(x,_RELEASE_LEVEL) \
                           OC_STRINGIFY(OC_JOIN(x,_RELEASE_SERIAL))

#define OC_VERSION OC_MAKE_VERSION(OC)

/* Functions to be passed to the Tcl/Tk libraries */
Tcl_PackageInitProc	Oc_Init;

/* Prototypes for the routines defined in oc.cc */

/* Callers of Oc_GlobalInterpreter() should use
 * Tcl_SaveResult and Tcl_RestoreResult when using
 * the returned interp to leave the result of the
 * interp apparently unchanged.
 */
Tcl_Interp*	Oc_GlobalInterpreter();
OC_BOOL Oc_IsGlobalInterpThread(); // Returns true iff running thread
                                  /// is the thread with globalInterp.
int		Oc_InitScript(Tcl_Interp*, const char*, const char*);
int		Oc_AppMain(int,char**);
int		Oc_RegisterCommand(Tcl_Interp*,const char*,Tcl_CmdProc*);
#if TCL_MAJOR_VERSION >= 8  // Obj interface available
int		Oc_RegisterObjCommand(Tcl_Interp*,
                                      const char*,Tcl_ObjCmdProc*);
#endif
void		Oc_SetDefaultTkFlag(int);
void		Oc_Main(int,char**,Tcl_AppInitProc*);
float           Oc_Nop(float);
double          Oc_Nop(double);
long double     Oc_Nop(long double);
int             Oc_Nop(int);
long            Oc_Nop(long);
unsigned int    Oc_Nop(unsigned int);
unsigned long   Oc_Nop(unsigned long);
const void*     Oc_Nop(const void*);
const String&   Oc_Nop(const String& x);
Tcl_Channel     Oc_Nop(Tcl_Channel);
#if OC_USE_SSE
__m128d         Oc_Nop(__m128d);
__m128i         Oc_Nop(__m128i);
#endif
#if OC_SYSTEM_TYPE == OC_WINDOWS
__int64          Oc_Nop(__int64);
unsigned __int64 Oc_Nop(unsigned __int64);
#endif


size_t          Oc_CacheSize(int level=0);
void            Oc_DirectPathname(const char *nickname,
				  Oc_AutoBuf& fullname);
void            Oc_TclFileCmd(const char* subcmd,
			      const char* filename,
			      Oc_AutoBuf& result);

typedef void OcSigFunc(int,ClientData); // C++ linkage OK here
void Oc_AppendSigTermHandler(OcSigFunc* handler,ClientData);
void Oc_RemoveSigTermHandler(OcSigFunc* handler,ClientData);

// Read access to Oc_Option database
OC_BOOL Oc_GetOcOption(const std::string& classname,
                       const std::string& option,
                       std::string& value);

void Oc_StrError(int errnum,char* buf,size_t buflen);
// Note: There is a bare declare of this function in ocnuma.cc.  If the
// signature of Oc_StrError changes it should be updated both here and
// in ocnuma.cc.

#if (OC_SYSTEM_TYPE == OC_WINDOWS)
std::string Oc_WinStrError(DWORD errorID); // NB: Used in ocexcept.cc
/// If signature changes propagate changes to there too.
std::string Oc_WinGetFileSID(const std::string& filename);
std::string Oc_WinGetCurrentProcessSID();
std::string Oc_WinGetSIDAccountName(const std::string& sidstr);
#endif

#endif /* _OC */
