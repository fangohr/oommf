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
  if(cache.state_id!=state->Id()) {
    // Fill cache with data
    CacheRequestIncrement(1);
    cache.state_id=0; // Safety
    ChunkScalarOutputInitialize(state,1);
    ChunkScalarOutput(state,0,state->spin.Size(),0);
    ChunkScalarOutputFinalize(state,1);
    CacheRequestIncrement(-1);

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
 int argc,const char** /* argv */)  // Args
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


// Definitions for explicit specializations of member function
// Oxs_FieldOutput<>::FillValueLabelsAndUnits() and
// Oxs_FieldOutput<>::FillValueLabelsAndUnits().  Declarations are in
// the .h file so the compiler knows to not provide the generic version,
// but the definitions are here since full specializations are not
// templates.
template<>
void Oxs_FieldOutput<OC_REAL8m>::FillValueLabelsAndUnits
(vector<String>& valuelabels,
 vector<String>& valueunits) const {
  valuelabels.push_back(OutputName());
  valueunits.push_back(OutputUnits());
}

template<>
void Oxs_FieldOutput<OC_REAL8m>::Register
(Oxs_Director* director_,OC_INT4m priority_)
{
  Oxs_Output::Register(director_,priority_);
  if(GetDirector()!=NULL) {
    String value;
    if(GetMifOption("scalar_field_output_format",value)) {
      SetOutputFormat(value);
    }
    if(GetMifOption("scalar_field_output_meshtype",value)) {
      SetOutputMeshtype(value);
    }
    if(GetMifOption("scalar_field_output_writeheaders",value)) {
      SetOutputWriteheaders(atoi(value.c_str()));
    }
  }
}

template<>
void Oxs_FieldOutput<ThreeVector>::FillValueLabelsAndUnits
(vector<String>& valuelabels,
 vector<String>& valueunits) const {
  valuelabels.push_back(OutputName()+String("_x"));
  valuelabels.push_back(OutputName()+String("_y"));
  valuelabels.push_back(OutputName()+String("_z"));
  valueunits.push_back(OutputUnits());
  valueunits.push_back(OutputUnits());
  valueunits.push_back(OutputUnits());
}

template<>
void Oxs_FieldOutput<ThreeVector>::Register
(Oxs_Director* director_,OC_INT4m priority_)
{
  Oxs_Output::Register(director_,priority_);
  if(GetDirector()!=NULL) {
    String value;
    if(GetMifOption("vector_field_output_format",value)) {
      SetOutputFormat(value);
    }
    if(GetMifOption("vector_field_output_meshtype",value)) {
      SetOutputMeshtype(value);
    }
    if(GetMifOption("vector_field_output_writeheaders",value)) {
      SetOutputWriteheaders(atoi(value.c_str()));
    }
  }
}

// Dummy generic versions of member function
// Oxs_FieldOutput<>::FillValueLabelsAndUnits() and
// Oxs_FieldOutput<>::FillValueLabelsAndUnits(), which are explicitly
// specialized above. This should never be instantiated; if they are
// then the static_assert will trigger compilation error.  I originally
// put these definitions in the header file, but although Visual C++
// 2019 compiled this fine, it threw a link error complaining that the
// specializations were unresolved.
template <class T>
void Oxs_FieldOutput<T>::Register(Oxs_Director*,OC_INT4m)
{
  static_assert(sizeof(T)==0,"Oxs_FieldOutput<T>::Register"
                " requires explicit specialization.");
  // Note: Make static_assert depend on the template
  //       parameter so that this code is not compiled
  //       unless being used for template instantiation.
}
template <class T>
void Oxs_FieldOutput<T>::FillValueLabelsAndUnits
(vector<String>&,vector<String>&) const
{
  static_assert(sizeof(T)==0,"Oxs_FieldOutput<T>::FillValueLabelsAndUnits"
                " requires explicit specialization.");
  // Note: Make static_assert depend on the template
  //       parameter so that this code is not compiled
  //       unless being used for template instantiation.
}
