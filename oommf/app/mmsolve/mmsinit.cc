/* FILE: mmsInit.cc                 -*-Mode: c++-*-
 *
 * Initialization routines for a Tcl/Tk loadable module which adds
 * a set of Tcl commands to an interpreter suitable for controlling
 * a 3D micromagnetic simulation.
 *
 */

#include "oc.h"
#include "nb.h"

#include "grid.h"
#include "mifio.h"
#include "mmsolve.h"

/* End includes */

/*
 *----------------------------------------------------------------------
 *
 * Mmsolve_Init --
 *
 *      This procedure initializes the mmSolve extension.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int Mmsolve_Init(Tcl_Interp *interp)
{
#define RETURN_TCL_ERROR                                                \
    Tcl_AddErrorInfo(interp, ab("\n    (in Mmsolve_Init())"));          \
    return TCL_ERROR

  Oc_AutoBuf ab, ab1;

    if (Tcl_PkgRequire(interp, ab("Oc"), ab1("2"), 0) == NULL) {
        Tcl_AppendResult(interp, ab("\n\t(Mms " MMSOLVE_VERSION
                " needs Oc 2)"), NULL);
        RETURN_TCL_ERROR;
    }

    if (Tcl_PkgRequire(interp, ab("Nb"), ab1("2"), 0) == NULL) {
        Tcl_AppendResult(interp, ab("\n\t(Mms " MMSOLVE_VERSION
                " needs Nb 2)"), NULL);
        RETURN_TCL_ERROR;
    }

    if (Tcl_PkgRequire(interp, ab("Vf"), ab1("2"), 0) == NULL) {
        Tcl_AppendResult(interp, ab("\n\t(Mms " MMSOLVE_VERSION
                " needs Vf 2)"), NULL);
        RETURN_TCL_ERROR;
    }

  Oc_RegisterCommand(interp, "mms_mif", Mms_MifCmd);
  Oc_RegisterCommand(interp, "mms_grid2D", Mms_Grid2DCmd);
  if (Tcl_PkgProvide(interp, ab("Mms"), ab1(MMSOLVE_VERSION)) != TCL_OK) {
    RETURN_TCL_ERROR;
  }
  if (Oc_InitScript(interp, "Mms", MMSOLVE_VERSION) == TCL_OK) {
    return TCL_OK;
  }
  RETURN_TCL_ERROR;

#undef RETURN_TCL_ERROR
}

/*
 *----------------------------------------------------------------------
 *
 * Mmsolve_SafeInit --
 *
 *      This procedure initializes the example command for a safe
 *      interpreter.  You would leave out of this procedure any commands
 *      you deemed unsafe.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int Mmsolve_SafeInit(Tcl_Interp *interp)
{
    return Mmsolve_Init(interp);
}

/*
 * Support macros
 */

#define MYBUFSIZE 15000
static char mybuf[MYBUFSIZE]; // Scratch space, used in particular to
/// hold intermediate results being passed back to Tcl.
#define MYSTRFMT %s     

// Macros which take a list of conversion functions and return an argument
// list where those functions are applied to elements of argv.
#define ARGN0() ()
#define ARGN1(a1) (a1(argv[2])) 
#define ARGN2(a1,a2) (a1(argv[2]), a2(argv[3]))
#define ARGN3(a1,a2,a3) (a1(argv[2]), a2(argv[3]), a3(argv[4]))
#define ARGN4(a1,a2,a3,a4) (a1(argv[2]), a2(argv[3]), a3(argv[4]), a4(argv[5]))
#define ARGN5(a1,a2,a3,a4,a5) (a1(argv[2]), a2(argv[3]), a3(argv[4]), \
			       a4(argv[5]), a5(argv[6])) 
#define ARGN6(a1,a2,a3,a4,a5,a6) (a1(argv[2]), a2(argv[3]), a3(argv[4]), \
			          a4(argv[5]), a5(argv[6]), a6(argv[7])) 

#define ARGE0() (errorcode)
#define ARGE1(a1) (a1(argv[2]), errorcode) 
#define ARGE2(a1,a2) (a1(argv[2]), a2(argv[3]), errorcode)
#define ARGE3(a1,a2,a3) (a1(argv[2]), a2(argv[3]), a3(argv[4]), errorcode)
#define ARGE4(a1,a2,a3,a4) (a1(argv[2]), a2(argv[3]), a3(argv[4]), \
			    a4(argv[5]), errorcode)
#define ARGE5(a1,a2,a3,a4,a5) (a1(argv[2]), a2(argv[3]), a3(argv[4]), \
			    a4(argv[5]), a5(argv[6]), errorcode)
#define ARGE6(a1,a2,a3,a4,a5,a6) (a1(argv[2]), a2(argv[3]), a3(argv[4]), \
  	                 a4(argv[5]), a5(argv[6]), a6(argv[7]), errorcode) 

#define ARG(N,LIST,ERROR) ARG##ERROR##N LIST

// Conversion routines
#define FLTFMT %.17g
#define FLTSTR "%.17g"
#define NOP(x) x
#define VOIDC(func) (func,"")    // Evaluate "void" return type function
/// func, and return the Tcl "void" return value of "".
static const char* Vec3Dtoa(Vec3D v)
{ // Converts a Vec3D to an ASCII character string.
  static char buf[256]; // Should be big enough!
  Oc_Snprintf(buf, sizeof(buf), FLTSTR " " FLTSTR " " FLTSTR,
              v.GetX(),v.GetY(),v.GetZ());
  return buf;
}
static Vec3D atoVec3D(const char *buf)
{ // Converts an ASCII character string to a Vec3D.
  static Vec3D v;
  double x,y,z;
  if(sscanf(buf,"%lg%lg%lg",&x,&y,&z)!=3) {
    PlainError(1,"Error in atoVec3D: "
	       "Invalid input: ->%s<-",buf);
  }
  v=Vec3D(x,y,z);
  return v;
}
static char atoc(const char *str)
{
  return str[0];
}
static const char* Stringtoa(const Nb_DString &str)
{
  return str.GetStr();
}

// Convert an ASCII character string representing a channel
// in Tcl interpreter *interp into the corresponding Tcl_Channel.
// NB: The variable "interp" must be properly defined inside the
//     routine where this macro is used.
#define atoChannel(x) Tcl_GetChannel(interp,x,NULL)

// A single macro that expands into a C function that wraps an instance
// method of a C++ class, providing a Tcl interface.  It is parameterized
// by several arguments, and several 'global' definitions assumed to be
// #define-d above and #undefine-d below uses of this macro.  The 'global'
// definitions help reduce the number of arguments.
//	Uses globals:
//		EXTENSION_TCL_PREFIXSTR
//		CLASS_C_NAME
//		CLASS_TCL_NAME
//	Uses arguments:
//		METHOD - name of method
//		NUMARGS - number of arguments which method takes
//		USAGE - a string describing the arguments to the 
//			methods, suitable for a usage message
//		CVT - a parenthesized list of NUMARGS conversion 
//			routines to be applied to strings to generate
//			arguments of the proper type for the C++ class
//			method
//		RCVT - a conversion routine to apply to the return value
//			of the method.
//		RFMT - an sprintf format string for converting the return
//			value of RCVT to a string.
//              ERROR - whether or not the C++ class method returns an error
//                     code.  Set ERROR to E if it does, N if not.
//
//
#define MAKE_INSTANCE_NAME1(x,y) x##Instance##y
#define MAKE_INSTANCE_NAME(x,y) MAKE_INSTANCE_NAME1(x,y)

#define INSTANCE_METHOD(METHOD,NUMARGS,USAGE,CVT,RCVT,RFMT,ERROR)       \
extern "C" {                                                            \
int                                                                     \
MAKE_INSTANCE_NAME(CLASS_C_NAME,METHOD)                                 \
(ClientData clientData, Tcl_Interp *interp, int argc,                   \
                                              CONST84 char **argv)      \
{                                                                       \
    int i;                                                              \
    if (argc != NUMARGS + 2) {                                          \
        Tcl_AppendResult(interp, "\n'<",                                \
                OC_STRINGIFY(EXTENSION_TCL_PREFIX),                     \
                "_", OC_STRINGIFY(CLASS_TCL_NAME), " instance> ",       \
		argv[1],"' usage:\n\t$instance ",                       \
                argv[1], " " USAGE,                                     \
                "\n\nInvalid call:\n\t$instance",                       \
                (char *) NULL);                                         \
        for (i=1; i<argc; i++) {                                        \
            Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);      \
        }                                                               \
        return TCL_ERROR;                                               \
    }                                                                   \
    int errorcode=0;                                                    \
    CLASS_C_NAME *classPtr = (CLASS_C_NAME *) clientData;               \
    /* interp->result is only 200 bytes wide, so use */                 \
    /* Tcl_AppendResult instead of printing directly */                 \
    /* into interp->result.                          */                 \
    Oc_Snprintf(mybuf, sizeof(mybuf), OC_STRINGIFY(RFMT),               \
            RCVT(classPtr->METHOD ARG(NUMARGS,CVT,ERROR)));             \
    Tcl_ResetResult(interp);                                            \
    Tcl_AppendResult(interp,mybuf,(char *)NULL);                        \
    if(errorcode>0) {                                                   \
        /* Error detected.  Overwrite return value with error       */  \
        /* message and return TCL_ERROR.  The static portion of     */  \
        /* interp->result is only 200 bytes wide, so we use         */  \
        /* Tcl_SetResult() in case of longer error message strings. */  \
        Nb_DString errmsg;                                              \
        MessageLocker::GetMessage(errmsg);                              \
        Tcl_ResetResult(interp);                                        \
	Tcl_AppendResult(interp,errmsg.GetStr(),(char *)NULL);          \
        MessageLocker::Reset();                                         \
        return TCL_ERROR;                                               \
    } else if(errorcode<0) {                                            \
        /* Warning message detected.  Use PlainWarning() to report. */  \
        Nb_DString errmsg;                                              \
        MessageLocker::GetMessage(errmsg);                              \
	PlainWarning(errmsg.GetStr());                                  \
        MessageLocker::Reset();                                         \
        return TCL_OK;                                                  \
    }                                                                   \
    return TCL_OK;                                                      \
}                                                                       \
}

#define EXTENSION_TCL_PREFIX  mms
#define         CLASS_C_NAME  MIF
#define       CLASS_TCL_NAME  mif
INSTANCE_METHOD(GetA,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetCellSize,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetEdgeK1,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetK1,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetLogLevel,0,"",(),NOP,%d,N)
INSTANCE_METHOD(GetMaxTimeStep,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetMinTimeStep,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetMs,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetOutBaseName,0,"",(),NOP,MYSTRFMT,N)
INSTANCE_METHOD(GetMagnetizationOutputFormat,0,"",(),NOP,MYSTRFMT,N)
INSTANCE_METHOD(GetTotalFieldOutputFormat,0,"",(),NOP,MYSTRFMT,N)
INSTANCE_METHOD(GetDataTableOutputFormat,0,"",(),NOP,MYSTRFMT,N)
INSTANCE_METHOD(GetPartHeight,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetPartThickness,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetPartWidth,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetRandomizerSeed,0,"",(),NOP,%d,N)
INSTANCE_METHOD(GetUserComment,0,"",(),Stringtoa,MYSTRFMT,N)
INSTANCE_METHOD(GetUserInteractionLevel,0,"",(),NOP,%d,N)
INSTANCE_METHOD(GetDefaultControlPointSpec,0,"",(),Stringtoa,MYSTRFMT,N)
INSTANCE_METHOD(GetUserReportCode,0,"",(),NOP,%d,N)
INSTANCE_METHOD(Read,1,"<filename>",(NOP),VOIDC,MYSTRFMT,E)
INSTANCE_METHOD(SetA,1,"<A>",(Nb_Atof),NOP,FLTFMT,N)
INSTANCE_METHOD(SetCellSize,1,"<cellsize>",(Nb_Atof),NOP,FLTFMT,N)
INSTANCE_METHOD(SetDemagType,1,"<demagname>",(NOP),VOIDC,MYSTRFMT,E)
INSTANCE_METHOD(SetEdgeK1,1,"<eK1>",(Nb_Atof),NOP,FLTFMT,N)
INSTANCE_METHOD(SetK1,1,"<K1>",(Nb_Atof),NOP,FLTFMT,N)
INSTANCE_METHOD(SetMagInitArgs,1,"<args>",(NOP),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(SetMs,1,"<Ms>",(Nb_Atof),NOP,FLTFMT,N)
INSTANCE_METHOD(SetOutBaseName,1,"<basename>",(NOP),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(SetMagnetizationOutputFormat,1,\
		"<basename>",(NOP),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(SetTotalFieldOutputFormat,1,\
		"<basename>",(NOP),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(SetDataTableOutputFormat,1,\
		"<basename>",(NOP),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(SetPartHeight,1,"<height>",(Nb_Atof),NOP,FLTFMT,N)
INSTANCE_METHOD(SetPartThickness,1,"<thickness>",(Nb_Atof),NOP,FLTFMT,N)
INSTANCE_METHOD(SetPartWidth,1,"<width>",(Nb_Atof),NOP,FLTFMT,N)
INSTANCE_METHOD(SetRandomizerSeed,1,"<iseed>",(atoi),NOP,%d,N)
INSTANCE_METHOD(SetUserComment,1,"<comment>",(NOP),VOIDC,MYSTRFMT,N)
#undef EXTENSION_TCL_PREFIX
#undef CLASS_C_NAME
#undef CLASS_TCL_NAME


#define EXTENSION_TCL_PREFIX  mms
#define         CLASS_C_NAME  Grid2D
#define       CLASS_TCL_NAME  grid2d
INSTANCE_METHOD(BinDumph,2,"<filename note>",(NOP,NOP),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(BinDumpm,2,"<filename note>",(NOP,NOP),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(WriteFieldComponent,6, \
		"<component datastyle precision filename realname note>",\
		(NOP,NOP,NOP,NOP,NOP,NOP),VOIDC,MYSTRFMT,E)
INSTANCE_METHOD(WriteOvf,6, \
		"<type datastyle precision filename realname note>",\
		(atoc,NOP,NOP,NOP,NOP,NOP),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(GetAveMag,0,"",(),Vec3Dtoa,MYSTRFMT,N)
INSTANCE_METHOD(GetAveH,0,"",(),Vec3Dtoa,MYSTRFMT,N)
#ifdef CONTRIB_SPT001
INSTANCE_METHOD(GetResistance,0,"",(),NOP,FLTFMT,N)
#endif // CONTRIB_SPT001
INSTANCE_METHOD(GetCellSize,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetEnergyDensity,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetFieldComponentNorm,2,"component thick",\
		(NOP,Nb_Atof),NOP,FLTFMT,E)
INSTANCE_METHOD(GethUpdateCount,0,"",(),NOP,%d,N)
INSTANCE_METHOD(GetMaxNeighborAngle,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetMaxMxH,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetStepSize,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetTimeStep,0,"",(),NOP,FLTFMT,N)
INSTANCE_METHOD(GetNomBApplied,0,"",(),Vec3Dtoa,MYSTRFMT,N)
INSTANCE_METHOD(MarkhInvalid,0,"",(),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(Reset,0,"",(),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(ResetODE,0,"",(),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(RoughenBoundaryThickness,0,"",(),VOIDC,MYSTRFMT,N)
INSTANCE_METHOD(StepODE,2,"mintimestep maxtimestep",(Nb_Atof,Nb_Atof),\
		NOP,FLTFMT,E)
INSTANCE_METHOD(SetAppliedField,2,"<B_list fieldstep>",(atoVec3D,atoi),\
		VOIDC,MYSTRFMT,N)
#undef EXTENSION_TCL_PREFIX
#undef CLASS_C_NAME
#undef CLASS_TCL_NAME


////////////////////////////////////////////////////////////////////////
//
//  Special Cases
//

extern "C" {

int
MIFInstanceSetDefaultControlPointSpec
(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char **argv)
{
    int i;
    if (argc < 3 ) {
        Tcl_AppendResult(interp, "\n'<mms_mif instance> ", argv[1],
                "' usage:\n\t$instance ", argv[1]," <control spec(s)>",
		"\n\nReturns: <control spec(s)>",
                "\n\nInvalid call:\n\t$instance",
	        (char *) NULL);
        for (i=1; i<argc; i++) {
            Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);
        }
        return TCL_ERROR;
    }
    MIF *classPtr = (MIF *) clientData;
    char* newspec=Tcl_Merge(argc-2,argv+2); // Combine args into 1 string
    classPtr->SetDefaultControlPointSpec(newspec);
    Tcl_AppendResult(interp, newspec, (char *)NULL);
    Tcl_Free(newspec);

    return TCL_OK;
}

int
MIFInstanceGetFieldRangeList
(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char **argv)
{ // This is handled specially so we can return arbitrarily
  // long strings without going though 'mybuf'.
    int i;
    if (argc != 2 ) {
        Tcl_AppendResult(interp, "\n'<mms_mif instance> ", argv[1],
                "' usage:\n\t$instance ", argv[1],
		"\n\nReturns: <List of field ranges>",
                "\n\nInvalid call:\n\t$instance",
	        (char *) NULL);
        for (i=1; i<argc; i++) {
            Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);
        }
        return TCL_ERROR;
    }
    MIF *classPtr = (MIF *) clientData;
    Nb_DString fields = classPtr->GetFieldRangeList();
    Tcl_AppendResult(interp, fields.GetStr(), (char *)NULL);

    return TCL_OK;
}

int
MIFInstanceWrite
(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char **argv)
{ // This is handled specially so we can return arbitrarily
  // long strings without going though 'mybuf'.
    int i;
    if (argc != 2 ) {
        Tcl_AppendResult(interp, "\n'<mms_mif instance> ", argv[1],
                "' usage:\n\t$instance ", argv[1],
		"\n\nReturns: <MIF data file>",
                "\n\nInvalid call:\n\t$instance",
	        (char *) NULL);
        for (i=1; i<argc; i++) {
            Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);
        }
        return TCL_ERROR;
    }
    int errorcode=0;
    MIF *classPtr = (MIF *) clientData;
    Nb_DString str = classPtr->Write(errorcode);
    if(errorcode>0) {
        /* Error detected.  Overwrite return value with error       */
        /* message and return TCL_ERROR.  The static portion of     */
        /* interp->result is only 200 bytes wide, so we use         */
        /* Tcl_SetResult() in case of longer error message strings. */
        Nb_DString errmsg;
        MessageLocker::GetMessage(errmsg);
        Tcl_ResetResult(interp);
	Tcl_AppendResult(interp,errmsg.GetStr(),(char *)NULL);
        MessageLocker::Reset();
        return TCL_ERROR;
    } else {
      if(errorcode<0) {
        /* Warning message detected.  Use PlainWarning() to report. */
        Nb_DString errmsg;
        MessageLocker::GetMessage(errmsg);
	PlainWarning(errmsg.GetStr());
        MessageLocker::Reset();
        return TCL_OK;
      }
      /* Normal return */
      Tcl_AppendResult(interp, str.GetStr(), (char *)NULL);
    }

    return TCL_OK;
}

int
Grid2DInstanceGetDimens
(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char **argv)
{
    int i;
    if (argc != 2 ) {
        Tcl_AppendResult(interp, "\n'<mms_grid2d instance> ", argv[1],
                "' usage:\n\t$instance ", argv[1]," GetDimens",
		"\n\nReturns: Nx Ny",
                "\n\nInvalid call:\n\t$instance",
	        (char *) NULL);
        for (i=1; i<argc; i++) {
            Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);
        }
        return TCL_ERROR;
    }
    Grid2D *classPtr = (Grid2D *) clientData;

    int Nx,Ny;
    Nx=Ny=0;  // Just to keep some compilers quiet.
    classPtr->GetDimens(Nx,Ny);

    Oc_Snprintf(mybuf, sizeof(mybuf), "%4d %4d",Nx,Ny);
    Tcl_AppendResult(interp, mybuf, (char *)NULL);
    return TCL_OK;
}

int
Grid2DInstanceGetEnergyDensities
(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char **argv)
{
    int i;
    if (argc != 2 ) {
        Tcl_AppendResult(interp, "\n'<mms_grid2d instance> ", argv[1],
                "' usage:\n\t$instance ", argv[1]," GetEnergyDensities",
		"\n\nReturns: Eexch Eanis Edemag Ezeeman Etotal",
                "\n\nInvalid call:\n\t$instance",
	        (char *) NULL);
        for (i=1; i<argc; i++) {
            Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);
        }
        return TCL_ERROR;
    }
    Grid2D *classPtr = (Grid2D *) clientData;

    double exch,anis,demag,zeeman,total;
    exch=anis=demag=zeeman=total=0.0;  // Just to keep compiler quiet.
    classPtr->GetEnergyDensities(exch,anis,demag,zeeman,total);

    Oc_Snprintf(mybuf, sizeof(mybuf), 
                FLTSTR " " FLTSTR " " FLTSTR " " FLTSTR " " FLTSTR,
	        exch,    anis,    demag,   zeeman,  total);
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp,mybuf,(char *)NULL);

    return TCL_OK;
}

int
Grid2DInstanceGetStepStats
(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char **argv)
{
    int i;
    if (argc != 2 ) {
        Tcl_AppendResult(interp, "\n'<mms_grid2d instance> ", argv[1],
                "' usage:\n\t$instance ", argv[1],
		"\n\nReturns: steps rejects energy_rejects position_rejects",
                "\n\nInvalid call:\n\t$instance",
	        (char *) NULL);
        for (i=1; i<argc; i++) {
            Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);
        }
        return TCL_ERROR;
    }
    Grid2D *classPtr = (Grid2D *) clientData;

    int steps,rejects,energy_rejects,position_rejects;
    steps=rejects=energy_rejects=position_rejects=0; // Quiet compiler.
    classPtr->GetStepStats(steps,rejects,energy_rejects,position_rejects);

    Oc_Snprintf(mybuf, sizeof(mybuf), "%d %d %d %d",
	    steps,rejects,energy_rejects,position_rejects);
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp,mybuf,(char *)NULL);

    return TCL_OK;
}

int
Grid2DInstanceGetUsageTimes
(ClientData clientData, Tcl_Interp *interp, int argc, CONST84 char **argv)
{
    int i;
    if (argc != 2 ) {
        Tcl_AppendResult(interp, "\n'<mms_grid2d instance> ", argv[1],
                "' usage:\n\t$instance ", argv[1],
		"\n\nReturns: Usage_times_string",
                "\n\nInvalid call:\n\t$instance",
	        (char *) NULL);
        for (i=1; i<argc; i++) {
            Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);
        }
        return TCL_ERROR;
    }
    Grid2D *classPtr = (Grid2D *) clientData;

    Nb_DString str;
    classPtr->GetUsageTimes(str);

    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp,str.GetStr(),(char *)NULL);

    return TCL_OK;
}

int
MIFInstanceDelete
(ClientData /*clientData*/, Tcl_Interp *interp, int /*argc*/, CONST84 char **argv)
{
    static char buf[256]; // Should be big enough
    Oc_Snprintf(buf, sizeof(buf), "rename %s {}",argv[0]);
    return Tcl_GlobalEval(interp, buf);
}

int
Grid2DInstanceDelete
(ClientData /*clientData*/, Tcl_Interp *interp, int /*argc*/, CONST84 char **argv)
{
    static char buf[256]; // Should be big enough
    Oc_Snprintf(buf, sizeof(buf), "rename %s {}",argv[0]);
    return Tcl_GlobalEval(interp, buf);
}

} // end extern "C"

#define MMSFUNCLIST(NAME,METHOD) { #METHOD , NAME##Instance##METHOD }
struct funcmap { const char* methodname; Tcl_CmdProc *methodfunc; };
struct funcmap MifMethods[] = {
  MMSFUNCLIST(MIF,GetMinTimeStep), // Ordering affects lookup time in
  MMSFUNCLIST(MIF,GetMaxTimeStep), // InstanceCmd (below), so put
  MMSFUNCLIST(MIF,GetA),	   // frequently called methods first.
  MMSFUNCLIST(MIF,GetCellSize),
  MMSFUNCLIST(MIF,GetDefaultControlPointSpec),
  MMSFUNCLIST(MIF,GetFieldRangeList),
  MMSFUNCLIST(MIF,GetEdgeK1),
  MMSFUNCLIST(MIF,GetK1),
  MMSFUNCLIST(MIF,GetLogLevel),
  MMSFUNCLIST(MIF,GetMs),
  MMSFUNCLIST(MIF,GetOutBaseName),
  MMSFUNCLIST(MIF,GetMagnetizationOutputFormat),
  MMSFUNCLIST(MIF,GetTotalFieldOutputFormat),
  MMSFUNCLIST(MIF,GetDataTableOutputFormat),
  MMSFUNCLIST(MIF,GetPartHeight),
  MMSFUNCLIST(MIF,GetPartThickness),
  MMSFUNCLIST(MIF,GetPartWidth),
  MMSFUNCLIST(MIF,GetRandomizerSeed),
  MMSFUNCLIST(MIF,GetUserComment),
  MMSFUNCLIST(MIF,GetUserInteractionLevel),
  MMSFUNCLIST(MIF,GetUserReportCode),
  MMSFUNCLIST(MIF,Read),
  MMSFUNCLIST(MIF,SetA),
  MMSFUNCLIST(MIF,SetCellSize),
  MMSFUNCLIST(MIF,SetDefaultControlPointSpec),
  MMSFUNCLIST(MIF,SetDemagType),
  MMSFUNCLIST(MIF,SetEdgeK1),
  MMSFUNCLIST(MIF,SetK1),
  MMSFUNCLIST(MIF,SetMagInitArgs),
  MMSFUNCLIST(MIF,SetMs),
  MMSFUNCLIST(MIF,SetOutBaseName),
  MMSFUNCLIST(MIF,SetMagnetizationOutputFormat),
  MMSFUNCLIST(MIF,SetTotalFieldOutputFormat),
  MMSFUNCLIST(MIF,SetDataTableOutputFormat),
  MMSFUNCLIST(MIF,SetPartHeight),
  MMSFUNCLIST(MIF,SetPartThickness),
  MMSFUNCLIST(MIF,SetPartWidth),
  MMSFUNCLIST(MIF,SetRandomizerSeed),
  MMSFUNCLIST(MIF,SetUserComment),
  MMSFUNCLIST(MIF,Write),
  { "Delete" , MIFInstanceDelete },
  { (const char *)NULL, (Tcl_CmdProc *)NULL }
};

struct funcmap Grid2DMethods[] = {
  MMSFUNCLIST(Grid2D,StepODE),     // Ordering affects lookup time in
  MMSFUNCLIST(Grid2D,GetTimeStep), // InstanceCmd (below), so put
  MMSFUNCLIST(Grid2D,GetMaxMxH),   // frequently called methods first.
  MMSFUNCLIST(Grid2D,GetMaxNeighborAngle),
  MMSFUNCLIST(Grid2D,BinDumph),		// obsolete?
  MMSFUNCLIST(Grid2D,BinDumpm),		// obsolete?
  MMSFUNCLIST(Grid2D,GetAveMag),	
  MMSFUNCLIST(Grid2D,GetAveH),		// obsolete?
#ifdef CONTRIB_SPT001
  MMSFUNCLIST(Grid2D,GetResistance),		// obsolete?
#endif // CONTRIB_SPT001
  MMSFUNCLIST(Grid2D,GetCellSize),
  MMSFUNCLIST(Grid2D,GetDimens),		// obsolete?
  MMSFUNCLIST(Grid2D,GetEnergyDensities),
  MMSFUNCLIST(Grid2D,GetEnergyDensity),		// obsolete?
  MMSFUNCLIST(Grid2D,GetFieldComponentNorm),		// obsolete?
  MMSFUNCLIST(Grid2D,GethUpdateCount),
  MMSFUNCLIST(Grid2D,GetStepSize),
  MMSFUNCLIST(Grid2D,GetStepStats),
  MMSFUNCLIST(Grid2D,GetUsageTimes),
  MMSFUNCLIST(Grid2D,GetNomBApplied),		// obsolete?
  MMSFUNCLIST(Grid2D,MarkhInvalid),		// obsolete?
  MMSFUNCLIST(Grid2D,Reset),
  MMSFUNCLIST(Grid2D,ResetODE),		// obsolete?
  MMSFUNCLIST(Grid2D,RoughenBoundaryThickness),		// obsolete?
  MMSFUNCLIST(Grid2D,SetAppliedField),
  MMSFUNCLIST(Grid2D,WriteFieldComponent),
  MMSFUNCLIST(Grid2D,WriteOvf),
  { "Delete" , Grid2DInstanceDelete },
  { (const char *)NULL, (Tcl_CmdProc *)NULL }
};


// Macro that expands into the instance method invoking command
//	NAME and
//	TCLNAME
#define MMS_INSTANCE(NAME,TCLNAME)                                      \
int                                                                     \
Mms_##NAME##InstanceCmd                                                 \
(ClientData clientData, Tcl_Interp *interp, int argc,                   \
                                              CONST84 char **argv)      \
{                                                                       \
    int i;                                                              \
    if (argc < 2) {                                                     \
        Tcl_AppendResult(interp, "no value given for parameter \"",     \
                "method\" to \"", argv[0], "\"", (char *) NULL);        \
        return TCL_ERROR;                                               \
    }                                                                   \
    for(i=0 ; NAME##Methods[i].methodname!=NULL ; i++) {                \
        if(strcmp(argv[1],NAME##Methods[i].methodname)==0) {            \
            return NAME##Methods[i].methodfunc(clientData,interp,       \
                    argc,argv);                                         \
        }                                                               \
    }                                                                   \
    Tcl_AppendResult(interp, "\n'<mms_", OC_STRINGIFY(TCLNAME),         \
                     " instance>' usage:\n\t",                          \
		     "$instance <instance_method> ?args?\n\n",          \
		     "Invalid call:\n\t$instance", (char *) NULL);      \
    for (i=1; i<argc; i++) {                                            \
      Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);            \
    }                                                                   \
    Tcl_AppendResult(interp, "\n\nInvalid instance method: ", argv[1],  \
		     "\n\nValid instance methods:\n\t", (char *) NULL); \
    for(i=0 ; NAME##Methods[i].methodname!=NULL ; i++) {                \
        Tcl_AppendResult(interp, " ", NAME##Methods[i].methodname,      \
                (char *) NULL);                                         \
    }                                                                   \
    return TCL_ERROR;                                                   \
}     

// Note: The following two lines expand to function definitions,
// so they don't need a trailing semicolon.
MMS_INSTANCE(Mif,mif)
MMS_INSTANCE(Grid2D,grid2D)


// Macro that expands into a cleanup command to be called when the
// corresponding instance command is deleted in a Tcl interpreter.
#define MMS_CLASSDELETEPROC(NAME,CLASS)                                 \
void                                                                    \
Mms##NAME##DeleteProc(ClientData clientData)                            \
{                                                                       \
    CLASS *classPtr = (CLASS *) clientData;                             \
    delete classPtr;                                                    \
}

// Note: The following two lines expand to function definitions,
// so they don't need a trailing semicolon.
MMS_CLASSDELETEPROC(Mif,MIF)
MMS_CLASSDELETEPROC(Grid2D,Grid2D)


// Macro that expands into a Tcl_CmdProc for the 'new' proc of a Tcl class
// which takes zero or one arguments.  It links a Tcl instance command 
// to an instance of the C++ class CLASS.  When an argument is available
// that argument is interpreted as the Tcl instance command connected to
// another instance of the C++ class CLASS, to be copied.
#define MMS_CLASSNEWPROC_0P(NAME,CLASS)                                 \
int                                                                     \
Mms##NAME##New                                                          \
(ClientData /* clientData */, Tcl_Interp *interp, int argc,             \
                                                   CONST84 char **argv) \
{                                                                       \
    int i;                                                              \
    if ((argc > 4) || (argc < 3)) {                                     \
        Tcl_AppendResult(interp, "\n'", argv[0], " ", argv[1],          \
                "' usage:\n\t", argv[0], " ", argv[1],                  \
                " <varName>\n\nInvalid call:\n\t", argv[0],             \
                (char *) NULL);                                         \
        for (i=1; i<argc; i++) {                                        \
            Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);      \
        }                                                               \
        return TCL_ERROR;                                               \
    }                                                                   \
    CLASS *classPtr;                                                    \
    if (argc == 4) {                                                    \
        classPtr = new CLASS(Mms##NAME##Ref(interp, argv[3]));          \
    } else {                                                            \
        classPtr = new CLASS();                                         \
    }                                                                   \
    static int nextinstance = 0;                                        \
    char buf[200];                                                      \
    Oc_Snprintf(buf, sizeof(buf), "_%s%d", argv[0], nextinstance);      \
    if (Tcl_SetVar(interp, argv[2], buf, TCL_LEAVE_ERR_MSG) == NULL ) { \
        return TCL_ERROR;                                               \
    }                                                                   \
    Tcl_ResetResult(interp);                                            \
    Tcl_AppendResult(interp, buf, (char *) NULL);                       \
    Tcl_CreateCommand(interp, Tcl_GetStringResult(interp),              \
            (Tcl_CmdProc *) Mms_##NAME##InstanceCmd,                    \
	    (ClientData) classPtr,                                      \
            (Tcl_CmdDeleteProc *) Mms##NAME##DeleteProc);               \
    nextinstance += 1;                                                  \
    return TCL_OK;                                                      \
}


// Macro that expands into a Tcl_CmdProc for the 'new' proc of a Tcl class
// which takes 1 argument.  It links a Tcl instance command to an
// instance of the C++ class CLASS.
//	USAGE_STR is the call specific usage message
//	CNVT_FUNC1 converts the string argument into the type needed
//		by the CLASS constructor.
#define MMS_CLASSNEWPROC_1P(NAME,CLASS,USAGE_STR,CNVT_FUNC1)            \
int                                                                     \
Mms##NAME##New                                                          \
(ClientData /* clientData */, Tcl_Interp *interp, int argc,             \
                                                   CONST84 char **argv) \
{                                                                       \
    int i;                                                              \
    if (argc != 4) {                                                    \
        Tcl_AppendResult(interp, "\n'", argv[0], " ", argv[1],          \
                "' usage:\n\t", argv[0], " ", argv[1], " " USAGE_STR,   \
                "\n\nInvalid call:\n\t", argv[0], (char *) NULL);       \
        for (i=1; i<argc; i++) {                                        \
            Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);      \
        }                                                               \
        return TCL_ERROR;                                               \
    }                                                                   \
    CLASS *classPtr = new CLASS(CNVT_FUNC1(argv[3]));                   \
    static int nextinstance = 0;                                        \
    char buf[200];                                                      \
    Oc_Snprintf(buf, sizeof(buf), "_%s%d", argv[0], nextinstance);      \
    if (Tcl_SetVar(interp, argv[2], buf, TCL_LEAVE_ERR_MSG) == NULL ) { \
        return TCL_ERROR;                                               \
    }                                                                   \
    Tcl_ResetResult(interp);                                            \
    Tcl_AppendResult(interp, buf, (char *) NULL);                       \
    Tcl_CreateCommand(interp, Tcl_GetStringResult(interp),              \
            (Tcl_CmdProc *) Mms_##NAME##InstanceCmd,                    \
	    (ClientData) classPtr,                                      \
            (Tcl_CmdDeleteProc *) Mms##NAME##DeleteProc);               \
    nextinstance += 1;                                                  \
    return TCL_OK;                                                      \
}


// Macro that expands into a function that returns the CLASS reference
// associated with the Tcl class instance command 'name' in 'interp'
#define MMS_CLASS_REF(NAME,CLASS)                                       \
CLASS &                                                                 \
Mms##NAME##Ref(Tcl_Interp *interp, const char *name)                    \
{                                                                       \
    Tcl_CmdInfo info;                                                   \
    if (!Tcl_GetCommandInfo(interp, (char *) name, &info)) {            \
     Tcl_Panic(Oc_AutoBuf("No object corresponding to name %s"), name); \
    }                                                                   \
    return (CLASS &) (*((CLASS *) info.clientData));                    \
}


// Macro that adds the interp argument to a function call
#define MMS_ADDINTERP(ARG) (interp, ARG)

// Note: The following three lines expand to function definitions,
// so they don't need a trailing semicolon.
MMS_CLASS_REF(Mif,MIF)
MMS_CLASSNEWPROC_0P(Mif,MIF)
MMS_CLASSNEWPROC_1P(Grid2D,Grid2D,"<varName> <mms_mif instance>",
		    MmsMifRef MMS_ADDINTERP)


// Macro that expands into a Tcl_CmdProc which implements the Tcl command
// indicated by NAME.  This command serves as the Tcl interface for
// a C++ class of the same name (modulo capitalization).
#define MMS_CLASS(NAME)                                                 \
int                                                                     \
Mms_##NAME##Cmd                                                         \
(ClientData clientData, Tcl_Interp *interp, int argc,                   \
                                              CONST84 char **argv)      \
{                                                                       \
    char c;                                                             \
    int i;                                                              \
    if (argc < 2) {                                                     \
        Tcl_AppendResult(interp, "no value given for parameter \"",     \
                "proc\" to \"", argv[0], "\"", (char *) NULL);          \
        return TCL_ERROR;                                               \
    }                                                                   \
    c = argv[1][0];                                                     \
    if ((c == 'N') && (strcmp(argv[1], "New") == 0)) {                  \
        return Mms##NAME##New(clientData, interp, argc, argv);          \
    }                                                                   \
    Tcl_AppendResult(interp, "\n'", argv[0], "' usage:\n\t",            \
           argv[0], " <class_proc> ?args?\n\nInvalid call:\n\t",        \
           argv[0], (char *) NULL);                                     \
    for (i=1; i<argc; i++) {                                            \
        Tcl_AppendResult(interp, " ", argv[i], (char *) NULL);          \
    }                                                                   \
    Tcl_AppendResult(interp, "\n\nInvalid class proc: ", argv[1],       \
            "\n\nValid class procs:\n\tNew", (char *) NULL);            \
    return TCL_ERROR;                                                   \
}

// Note: The following two lines expand to function definitions,
// so they don't need a trailing semicolon.
MMS_CLASS(Mif)
MMS_CLASS(Grid2D)
