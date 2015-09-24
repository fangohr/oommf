/* FILE: mmsolve.cc                 -*-Mode: c++-*-
 *
 * Skeleton for mmsolve interpreter shell application.
 *
 */

#include "oc.h"
#include "nb.h"
#include "vf.h"
#include "mmsolve.h"	/* Mmsolve_Init, Mmsolve_SafeInit */
/* End includes */

#ifdef USE_MPI
// MPI process control routines
static MPI_Datatype MMS_MPI_ROUTER_TYPE;
void Mmsolve_MpiCleanup(ClientData dummy=NULL);
int mms_mpi_size(-1),mms_mpi_rank(-1);  // Initialized inside
/// Mmsolve_MpiInit().  Keeping this around in a static simplifies
/// the clean up routine because it eliminates the need to call
/// MPI_Comm_rank(), which one can't do after MPI_Finalize() has
/// been called.

// Auxiliary data-types
MPI_Datatype MMS_COMPLEX;

void Mmsolve_MpiCleanup(ClientData)
{
  static int cleanup_done(0);
  if(!cleanup_done) {
    // Set cleanup_done immediately to protect against re-entrancy
    cleanup_done=1;
    if(mms_mpi_rank==0) {
      Mmsolve_MpiWakeUp(NULL); // Wake up & kill all other processes
    }
    MPI_Type_free(&MMS_COMPLEX);
    MPI_Type_free(&MMS_MPI_ROUTER_TYPE);
    MPI_Finalize();
  }
}

void Mmsolve_MpiInit(int &argc, char** &argv)
{
  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD,&mms_mpi_size);
  MPI_Comm_rank(MPI_COMM_WORLD,&mms_mpi_rank);
fprintf(stderr,"Initializing node %d/%d\n",mms_mpi_rank,mms_mpi_size); /**/
  if(mms_mpi_rank==0) { // Set exit callback on root process
    Tcl_CreateExitHandler(Mmsolve_MpiCleanup,ClientData(0));
  }
  MPI_Type_contiguous(sizeof(MMS_MPI_ROUTER),MPI_BYTE,&MMS_MPI_ROUTER_TYPE);
  MPI_Type_commit(&MMS_MPI_ROUTER_TYPE);

  MPI_Type_contiguous(sizeof(MyComplex),MPI_BYTE,&MMS_COMPLEX);
  MPI_Type_commit(&MMS_COMPLEX);
}

void Mmsolve_MpiSleep()
{ // Puts any except root process into sleepmode.  In sleepmode,
  // slave processes wait for a function call request (typically
  // via a call on the root process to Mmsolve_MpiWakeUp()).  When
  // a non-null request is received, the process wakes up and
  // executes that function.  On return, the process goes back
  // into sleepmode.
  //   This should only be called once.  The only way to terminate
  // sleepmode is to send a function call request with address NULL,
  // which is interpreted as a kill-process request.
  if(mms_mpi_rank!=0) { // Root process doesn't sleep
    MMS_MPI_ROUTER func;
    do {
      MPI_Bcast(&func,1,MMS_MPI_ROUTER_TYPE,0,MPI_COMM_WORLD);
      if(func!=NULL) (*func)(); // Make requested function call
    } while(func!=NULL);
    Mmsolve_MpiCleanup();
    Tcl_Exit(0); // Don't do any further processing.
  }
}

void Mmsolve_MpiWakeUp(MMS_MPI_ROUTER func)
{ // When called by root process, direct all sleeping process
  // to jump to func()
  if(mms_mpi_rank==0) { // Only root process can wake others
    MPI_Bcast(&func,1,MMS_MPI_ROUTER_TYPE,0,MPI_COMM_WORLD);
  }
}
#endif // USE_MPI

int Tcl_AppInit(Tcl_Interp *interp)
{
    Oc_AutoBuf ab;
    Tcl_DString buf;

    Oc_SetDefaultTkFlag(0);
    if (Oc_Init(interp) == TCL_ERROR) {
       return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, ab("Oc"), Oc_Init, NULL);

    if (Nb_Init(interp) == TCL_ERROR) {
        Tcl_DStringInit(&buf);
        Tcl_DStringAppend(&buf, ab("Oc_Log Log"), -1);
        Tcl_DStringAppendElement(&buf, Tcl_GetStringResult(interp));
        Tcl_Eval(interp, Tcl_DStringAppendElement(&buf, ab("error")));
        Tcl_DStringFree(&buf);
    }
    Tcl_StaticPackage(interp, ab("Nb"), Nb_Init, NULL);
    if (Vf_Init(interp) == TCL_ERROR) {
        Tcl_DStringInit(&buf);
        Tcl_DStringAppend(&buf, ab("Oc_Log Log"), -1);
        Tcl_DStringAppendElement(&buf, Tcl_GetStringResult(interp));
        Tcl_Eval(interp, Tcl_DStringAppendElement(&buf, ab("error")));
        Tcl_DStringFree(&buf);
    }
    Tcl_StaticPackage(interp, ab("Vf"), Vf_Init, NULL);
    if (Mmsolve_Init(interp) == TCL_ERROR) {
        Tcl_DStringInit(&buf);
        Tcl_DStringAppend(&buf, ab("Oc_Log Log"), -1);
        Tcl_DStringAppendElement(&buf, Tcl_GetStringResult(interp));
        Tcl_Eval(interp, Tcl_DStringAppendElement(&buf, ab("panic")));
        Tcl_DStringFree(&buf);
        Tcl_Exit(1);
    }
    Tcl_StaticPackage(interp, ab("Mms"), Mmsolve_Init, NULL);
#ifdef USE_MPI
    Mmsolve_MpiSleep(); // Put all but root process to sleep
#endif // USE_MPI

    // Any mmsolve-based applications may assume that the
    // current working directory is the OOMMF root directory.
    // Make it so.
#if ((TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION > 0))       \
     || (TCL_MAJOR_VERSION > 8)
    Tcl_Chdir(Nb_GetOOMMFRootDir(ab));
#else
    Tcl_DStringInit(&buf);
    Tcl_DStringAppendElement(&buf, ab("cd"));
    Nb_GetOOMMFRootDir(ab);
    Tcl_Eval(interp, Tcl_DStringAppendElement(&buf, ab));
    Tcl_DStringFree(&buf);
#endif

    return TCL_OK;
}

int Oc_AppMain(int argc, char **argv)
{
#ifdef USE_MPI
  Mmsolve_MpiInit(argc,argv);
#endif // USE_MPI
  Oc_Main(argc, argv, Tcl_AppInit);
  return 0;
}

