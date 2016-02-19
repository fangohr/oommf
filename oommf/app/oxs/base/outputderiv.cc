/* FILE: ouputderiv.cc                -*-Mode: c++-*-
 *
 * Output classes derived from Oxs_Output.
 * For portability reasons, that code is in the header file, outputderiv.h.
 */

#include "outputderiv.h"

/* End includes */

//////////////////////////////////////////////////////////////
// Oxs_BaseChunkScalarOutput definitions
OC_REAL8m Oxs_BaseChunkScalarOutput::GetValue(const Oxs_SimState* state)
{
  if(state==NULL || state->Id()==0) {
    OXS_THROW(Oxs_BadParameter,
              "Invalid state passed to output object");
  }
  if(!IsCacheSupported() || cache.state_id!=state->Id()) {
    // Fill cache with data
    ForceCacheIncrement(1);
    cache.state_id=0; // Safety
    ChunkScalarOutputInitialize(state,1);
    ChunkScalarOutput(state,0,state->spin.Size(),0);
    ChunkScalarOutputFinalize(state,1);
    ForceCacheIncrement(-1);

    // Check that cache got filled
    if(cache.state_id!=state->Id()) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Output cache not filled by registered getscalar"
                " call (as required).");
    }
  }
  return cache.value;
}

int Oxs_BaseChunkScalarOutput::Output
(const Oxs_SimState* state,   // State to get output for
 Tcl_Interp* interp,          // Tcl interpreter
 int argc,CONST84 char** /* argv */)  // Args
{
  Tcl_ResetResult(interp);
  if(argc!=0) {
    Tcl_AppendResult(interp, "wrong # of args: should be none",
                     (char *) NULL);
    return TCL_ERROR;
  }
  if(state==NULL || state->Id()==0) {
    Tcl_AppendResult(interp,"Invalid state passed to output object",
                     (char *)NULL);
    return TCL_ERROR;
  }

  char buf[100];
  Oc_Snprintf(buf,sizeof(buf),output_format.c_str(),
              static_cast<double>(GetValue(state)));

  Tcl_ResetResult(interp); // Protection against badly behaved Oxs_Ext's.
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}
