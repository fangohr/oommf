/* FILE: outputderiv.h                 -*-Mode: c++-*-
 *
 * Templated output classes derived from output.h.  Each class providing
 * output should embed an instance of an output class for each output
 * provided.
 *
 * NOTE: This code looks pretty fragile, because there is a lot
 *  of coupling between the output classes and their owner class.
 *  Maybe we can figure out a cleaner, more robust solution in
 *  the future.
 */

#ifndef _OXS_OUTPUTDERIV
#define _OXS_OUTPUTDERIV

#include <stdlib.h>

#include <string>

#include "oc.h"
#include "nb.h"

#include "output.h"

#include "exterror.h"
#include "meshvalue.h"
#include "simstate.h"
#include "oxsexcept.h"
#include "threevector.h"
#include "util.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported
OC_USE_STRING;

/* End includes */

class Oxs_Director;   // Forward declarations
class Oxs_SimState;

//////////////////////////////////////////////////////////////////
// Templated cache class, used by templated output classes.
template<class T> struct Oxs_OutputCache {
public:
  Oxs_OutputCache() : state_id(0) {}
  OC_UINT4m state_id;
  T value;
};

//////////////////////////////////////////////////////////////////
// Templated output classes.  The templated class T should be the
// embedding class.

////////////////////////
// Scalar output
template<class T> class Oxs_ScalarOutput:public Oxs_Output {
  typedef void (T::* GetScalar)(const Oxs_SimState&);
private:
  T* owner;
  GetScalar getscalar;
  /// Without the typedef, this would be
  ///    void (T::* getscalar)(const Oxs_SimState&)

  String output_format; // Default is "%.17g"

  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_ScalarOutput(const Oxs_ScalarOutput<T> &);
  Oxs_ScalarOutput<T>& operator=(const Oxs_ScalarOutput<T>&);

public:
  Oxs_ScalarOutput() : Oxs_Output(),
    owner(NULL), getscalar(NULL), output_format("%.17g")
    {
      cache.value=0; // Safety
      CacheRequestIncrement(1);  // Temporary measure: until /**/
      /// cache management is implemented in Tcl interface,  /**/
      /// enable caching on all scalar outputs.              /**/
    }

  void Setup(T* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             OC_BOOL cache_support_,const GetScalar member_function)
  {
    owner = owner_;
    getscalar = member_function;
    Oxs_Output::Setup(owner_name_,output_name_,
                      "scalar",output_units_,
                      cache_support_);
  }

  void Register(Oxs_Director* director_,OC_INT4m priority_)
  {
    Oxs_Output::Register(director_,priority_);
    if(GetDirector()!=NULL) {
      String value;
      if(GetMifOption("scalar_output_format",value)) {
        SetOutputFormat(value);
      }
    }
  }

  // Note: Base class member function GetMifOption() calls the director
  // to get the output format strings stored in the MIF options array.
  // We can't directly access member functions of director_ here because
  // this header file defines a forward declaration to the Oxs_Director
  // class, but does not #include "director.h".  We don't #include
  // "director.h" because director.h needs to include this file
  // (output.h) in order to get the definitions for the templated
  // Oxs_Output child classes Oxs_ScalarOutput and Oxs_VectorFieldOutput
  // (which it uses to provide total energy, total field and energy calc
  // counts).  Moreover, we can't move this constructor definition to
  // output.cc because of the template instantiation model being used
  // (what gcc calls the "Borland model"), which requires the inclusion
  // of full template definitions in each module using the template.
  // OTOH, the base class Oxs_Output is *not* templated, so
  // Oxs_Output::GetMifOption() *can* be located in output.cc, which
  // *can* #include "director.h", and so *can* call
  // director->GetMifOption().

  // Public data member for result drop-off/storage.
  // The owner should test GetCacheRequestCount() inside the output
  // calculation code, and adjust the cache as needed.
  Oxs_OutputCache<OC_REAL8m> cache;

  virtual int SetOutputFormat(const String& format);
  virtual String GetOutputFormat() const { return output_format; }

  virtual int Output(const Oxs_SimState* state,Tcl_Interp* interp,
                     int argc,CONST84 char** argv);
  // The owner getscalar() function can assume on entry that the
  // cache value stored here is invalid, but it may become valid
  // during the lifetime of the getscalar() call (if getscalar()
  // calls some other function that sets the cache).
};

//////////////////////////////////////////////////////////////////
// Vector field output

// Define a non-templated cache class so clients can
// access stored values without having to know the owner.
class Oxs_VectorFieldOutputCache : public Oxs_Output {
private:
  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_VectorFieldOutputCache(const Oxs_VectorFieldOutputCache&);
  Oxs_VectorFieldOutputCache&
     operator=(const Oxs_VectorFieldOutputCache&);

public:
  Oxs_VectorFieldOutputCache() : Oxs_Output() {}

  // Public data member for result drop-off/storage.  The owner should
  // test GetCacheRequestCount() inside the output calculation code, and
  // adjust the cache as needed.
  Oxs_OutputCache<Oxs_MeshValue<ThreeVector> > cache;

  virtual OC_BOOL UpdateCache(const Oxs_SimState*) = 0;
  /// Returns 1 on success, 0 if caching not supported.
  /// Throws an exception if an error occurs.

  virtual void FillBuffer(const Oxs_SimState* state,
                          Oxs_MeshValue<ThreeVector>& buffer) = 0;
  /// Throws an exception if an error occurs.  Import "buffer"
  /// is automatically sized as necessary.
};

// Main vector field output class
template<class T> class Oxs_VectorFieldOutput
  :public Oxs_VectorFieldOutputCache {
  typedef void (T::* GetVectorField)(const Oxs_SimState&);
private:
  T* owner;
  GetVectorField getvectorfield;
  /// Without the typedef, this would be
  ///    void (T::* getvectorfield)(const Oxs_SimState&)

  OC_BOOL output_format_writeheaders; // Default is 1

  String output_format_meshtype;   // Default is "rectangular"
  String output_format_datatype;   // Default is "binary"
  String output_format_precision;  // Default is "8"

  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_VectorFieldOutput(const Oxs_VectorFieldOutput<T>&);
  Oxs_VectorFieldOutput<T>& operator=(const Oxs_VectorFieldOutput<T>&);

public:

  Oxs_VectorFieldOutput() : Oxs_VectorFieldOutputCache(),
    owner(NULL), getvectorfield(NULL),
    output_format_writeheaders(1),
    output_format_meshtype("rectangular"),
    output_format_datatype("binary"),
    output_format_precision("8")
    {}

  void Setup(T* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             OC_BOOL cache_support_,
             const GetVectorField member_function)
    {
      owner = owner_;
      getvectorfield = member_function;
      Oxs_Output::Setup(owner_name_,output_name_,
                        "vector field",output_units_,
                        cache_support_);
    }

  void Register(Oxs_Director* director_,OC_INT4m priority_)
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

  // Note: Base class member function GetMifOption() calls the director
  // to get the output format strings stored in the MIF options array.
  // We can't directly access member functions of director_ here because
  // this header file defines a forward declaration to the Oxs_Director
  // class, but does not #include "director.h".  We don't #include
  // "director.h" because director.h needs to include this file
  // (output.h) in order to get the definitions for the templated
  // Oxs_Output child classes Oxs_ScalarOutput and Oxs_VectorFieldOutput
  // (which it uses to provide total energy, total field and energy calc
  // counts).  Moreover, we can't move this constructor definition to
  // output.cc because of the template instantiation model being used
  // (what gcc calls the "Borland model"), which requires the inclusion
  // of full template definitions in each module using the template.
  // OTOH, the base class Oxs_Output is *not* templated, so
  // Oxs_Output::GetMifOption() *can* be located in output.cc, which
  // *can* #include "director.h", and so *can* call
  // director->GetMifOption().

  virtual int SetOutputFormat(const String& format);
  virtual String GetOutputFormat() const;

  int SetOutputWriteheaders(OC_BOOL writeheaders) {
    output_format_writeheaders = writeheaders;
    return 0;
  }
  OC_BOOL GetOutputWriteheaders() const {
    return output_format_writeheaders;
  }

  int SetOutputMeshtype(const String& meshtype);
  String GetOutputMeshtype() const;

  virtual OC_BOOL UpdateCache(const Oxs_SimState*);
  /// Returns 1 on success, 0 if caching not supported.
  /// Throws an exception if an error occurs.

  virtual void FillBuffer(const Oxs_SimState* state,
                          Oxs_MeshValue<ThreeVector>& buffer);
  /// Throws an exception if an error occurs.  Import "buffer"
  /// is automatically sized as necessary.

  virtual int Output(const Oxs_SimState* state,Tcl_Interp* interp,
                     int argc,CONST84 char** argv);
};

//////////////////////////////////////////////////////////////////
// Scalar field output

// Define a non-templated cache class so clients can
// access stored values without having to know the owner.
class Oxs_ScalarFieldOutputCache : public Oxs_Output {
private:
  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_ScalarFieldOutputCache(const Oxs_ScalarFieldOutputCache&);
  Oxs_ScalarFieldOutputCache&
     operator=(const Oxs_ScalarFieldOutputCache&);

public:
  Oxs_ScalarFieldOutputCache() : Oxs_Output() {}

  // Public data member for result drop-off/storage.  The owner should
  // test GetCacheRequestCount() inside the output calculation code, and
  // adjust the cache as needed.
  Oxs_OutputCache<Oxs_MeshValue<OC_REAL8m> > cache;

  virtual OC_BOOL UpdateCache(const Oxs_SimState*) = 0;
  /// Returns 1 on success, 0 if caching not supported.
  /// Throws an exception if an error occurs.
};

// Main scalar field output class
template<class T> class Oxs_ScalarFieldOutput
  :public Oxs_ScalarFieldOutputCache {
  typedef void (T::* GetScalarField)(const Oxs_SimState&);
private:
  T* owner;
  GetScalarField getscalarfield;
  /// Without the typedef, this would be
  ///    void (T::* getscalarfield)(const Oxs_SimState&)

  OC_BOOL output_format_writeheaders; // Default is 1

  String output_format_meshtype;   // Default is "rectangular"
  String output_format_datatype;   // Default is "binary"
  String output_format_precision;  // Default is "8"

  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_ScalarFieldOutput(const Oxs_ScalarFieldOutput<T>&);
  Oxs_ScalarFieldOutput<T>& operator=(const Oxs_ScalarFieldOutput<T>&);

public:

  Oxs_ScalarFieldOutput() : Oxs_ScalarFieldOutputCache(),
    owner(NULL), getscalarfield(NULL),
    output_format_writeheaders(1),
    output_format_meshtype("rectangular"),
    output_format_datatype("binary"),
    output_format_precision("8")
    {}

  void Setup(T* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             OC_BOOL cache_support_,
             const GetScalarField member_function)
    {
      owner = owner_;
      getscalarfield = member_function;
      Oxs_Output::Setup(owner_name_,output_name_,
                        "scalar field",output_units_,
                        cache_support_);
    }

  void Register(Oxs_Director* director_,OC_INT4m priority_)
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

  // Note: Base class member function GetMifOption() calls the director
  // to get the output format strings stored in the MIF options array.
  // We can't directly access member functions of director_ here because
  // this header file defines a forward declaration to the Oxs_Director
  // class, but does not #include "director.h".  We don't #include
  // "director.h" because director.h needs to include this file
  // (output.h) in order to get the definitions for the templated
  // Oxs_Output child classes Oxs_Scalar/ScalarField/VectorFieldOutput
  // (which it uses to provide total energy, total field and energy calc
  // counts).  Moreover, we can't move this constructor definition to
  // output.cc because of the template instantiation model being used
  // (what gcc calls the "Borland model"), which requires the inclusion
  // of full template definitions in each module using the template.
  // OTOH, the base class Oxs_Output is *not* templated, so
  // Oxs_Output::GetMifOption() *can* be located in output.cc, which
  // *can* #include "director.h", and so *can* call
  // director->GetMifOption().

  virtual int SetOutputFormat(const String& format);
  virtual String GetOutputFormat() const;

  int SetOutputWriteheaders(OC_BOOL writeheaders) {
    output_format_writeheaders = writeheaders;
    return 0;
  }
  OC_BOOL GetOutputWriteheaders() const {
    return output_format_writeheaders;
  }

  int SetOutputMeshtype(const String& meshtype);
  String GetOutputMeshtype() const;

  virtual OC_BOOL UpdateCache(const Oxs_SimState*);
  /// Returns 1 on success, 0 if caching not supported.
  /// Throws an exception if an error occurs.

  virtual int Output(const Oxs_SimState* state,Tcl_Interp* interp,
                     int argc,CONST84 char** argv);
};


////////////////////////////////////////////////////////////////////////
// Include definitions below.  This is required by compiler/linkers
// using what gcc calls the "Borland model," as opposed to the "Cfront
// model."  The latter is distinguished by the use of template
// repositories.  With the Cfront model, template definitions can be
// put in a file (*.cc) separate from the header file (*.h).  Putting
// the definitions in the header file along with declarations slows down
// compilation, because the definitions are re-compiled in each including
// file, but appears to be more portable.

//////////////////////////////////////////////////////////////
// Oxs_ScalarOutput<T> definitions
template<class T>
int Oxs_ScalarOutput<T>::SetOutputFormat
(const String& format)
{
  if(format.length()==0) return 1;
  output_format = format;
  return 0;
}

template<class T>
int Oxs_ScalarOutput<T>::Output
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

  if(!IsCacheSupported() || cache.state_id!=state->Id()) {
    // Fill cache with data
    ForceCacheIncrement(1);
    cache.state_id=0; // Safety
    (owner->*getscalar)(*state);
    ForceCacheIncrement(-1);

    // Check that cache got filled
    if(cache.state_id!=state->Id()) {
      OXS_THROW(Oxs_ProgramLogicError,
               "Output cache not filled by registered getscalar"
               " call (as required).");
    }
  }

  char buf[100];
  Oc_Snprintf(buf,sizeof(buf),output_format.c_str(),
              static_cast<double>(cache.value));

  Tcl_ResetResult(interp); // Protection against badly behaved Oxs_Ext's.
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}


//////////////////////////////////////////////////////////////
// Oxs_VectorFieldOutput<T> definitions
template<class T>
int Oxs_VectorFieldOutput<T>::SetOutputFormat
(const String& format)
{
  if(format.length()==0) return 1;
  Nb_SplitList arglist;
  arglist.Split(format.c_str());
  if(arglist.Count()!=2) {
    OXS_THROW(Oxs_BadParameter,
              "Wrong number of arguments in format string import to "
              "Oxs_VectorFieldOutput<T>::SetOutputFormat(const char*)");
  }
  output_format_datatype = arglist[0];
  output_format_precision = arglist[1];
  return 0;
}

template<class T>
String Oxs_VectorFieldOutput<T>::GetOutputFormat() const
{
  vector<String> sarr;
  sarr.push_back(output_format_datatype);
  sarr.push_back(output_format_precision);
  return Nb_MergeList(&sarr);
}

template<class T>
int Oxs_VectorFieldOutput<T>::SetOutputMeshtype
(const String& meshtype)
{
  if(meshtype.length()==0) return 1;
  output_format_meshtype = meshtype;
  return 0;
}

template<class T>
String Oxs_VectorFieldOutput<T>::GetOutputMeshtype() const
{
  return output_format_meshtype;
}

template<class T>
int Oxs_VectorFieldOutput<T>::UpdateCache
(const Oxs_SimState* state)
{ // Returns 1 on success, 0 if caching not supported or not enabled.
  // Throws an exception if an error occurs.

  if(!IsCacheSupported() || GetCacheRequestCount()<1) return 0;

  if(cache.state_id!=state->Id()) {
    ForceCacheIncrement(1);
    cache.state_id=0; // Safety
    cache.value.AdjustSize(state->GetMeshNodesPtr());
    (owner->*getvectorfield)(*state);
    ForceCacheIncrement(-1);

    // Check that cache got filled
    if(cache.state_id!=state->Id()) {
      OXS_THROW(Oxs_ProgramLogicError,
               "Output cache not filled by registered getvectorfield"
               " call (as required).");
    }
  }

  return 1;
}

template<class T>
void Oxs_VectorFieldOutput<T>::FillBuffer
(const Oxs_SimState* state,
 Oxs_MeshValue<ThreeVector>& buffer)
{ // Throws an exception if an error occurs.  Import "buffer"
  // is automatically sized as necessary.

    // Swap supplied buffer in for cache
    OC_UINT4m save_state_id = cache.state_id;
    cache.value.Swap(buffer);

    // Call usual cache update routine to fill buffer
    ForceCacheIncrement(1);
    try {
      cache.state_id=0;
      cache.value.AdjustSize(state->GetMeshNodesPtr());
      (owner->*getvectorfield)(*state);
    } catch(...) {
      ForceCacheIncrement(-1);
      cache.value.Swap(buffer);        // Swap back
      cache.state_id = save_state_id;
      throw;
    }
    ForceCacheIncrement(-1);

    // Check that cache got filled
    if(cache.state_id!=state->Id()) {
      cache.value.Swap(buffer);  // Swap back
      cache.state_id = save_state_id;
      OXS_THROW(Oxs_ProgramLogicError,
                "Output buffer not filled by registered getvectorfield"
                " call (as required).");
    }

    // Swap back
    cache.value.Swap(buffer);
    cache.state_id = save_state_id;
}


template<class T>
int Oxs_VectorFieldOutput<T>::Output
(const Oxs_SimState* state, // State to get output for
 Tcl_Interp* interp,        // Tcl interpreter
 int argc,CONST84 char** argv)      // Args
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Tcl_AppendResult(interp, "wrong # of args: should be 1: filename",
                     (char *) NULL);
    return TCL_ERROR;
  }
  if(state==NULL || state->Id()==0) {
    Tcl_AppendResult(interp,"Invalid state passed to output object",
                     (char *)NULL);
    return TCL_ERROR;
  }

  const char* filename = argv[0];

  if(!IsCacheSupported() || cache.state_id!=state->Id()) {
    // Fill cache with vector field
    ForceCacheIncrement(1);
    cache.state_id=0; // Safety
    cache.value.AdjustSize(state->GetMeshNodesPtr());
    (owner->*getvectorfield)(*state);
    ForceCacheIncrement(-1);

    // Check that cache got filled
    if(cache.state_id!=state->Id()) {
      OXS_THROW(Oxs_ProgramLogicError,
               "Output cache not filled by registered getvectorfield"
               " call (as required).");
    }
  }

  // Write cache to file
  char description[4096];
  Oc_Snprintf(description,sizeof(description),
              "Oxs %.80s output\n"
              " MIF source file: %.256s\n"
              " Iteration: %u, State id: %u\n"
              " Stage: %u, Stage iteration: %u\n"
              " Stage simulation time: %g s\n"
              " Total simulation time: %g s\n",
              OutputType().c_str(),
              GetProblemName(),
              state->iteration_count,state->Id(),
              state->stage_number,state->stage_iteration_count,
              static_cast<double>(state->stage_elapsed_time),
              static_cast<double>(state->stage_start_time
                                  + state->stage_elapsed_time));
  int errorcode = TCL_OK;
  try {
    vector<String> valuelabels;
    valuelabels.push_back(OutputName()+String("_x"));
    valuelabels.push_back(OutputName()+String("_y"));
    valuelabels.push_back(OutputName()+String("_z"));

    vector<String> valueunits;
    valueunits.push_back(OutputUnits());
    valueunits.push_back(OutputUnits());
    valueunits.push_back(OutputUnits());

    Vf_Ovf20_MeshType meshtype;
    if(output_format_meshtype.compare("irregular")==0) {
      meshtype = vf_ovf20mesh_irregular;
    } else if(output_format_meshtype.compare("rectangular")==0) {
      meshtype = vf_ovf20mesh_rectangular ;
    } else {
      String errmsg =
        String("Error in Oxs_VectorFieldOutput<T>::Output():")
        + String(" Bad meshtype: \"")
        + String(output_format_meshtype)
        + String("\"  Should be either \"irregular\" or \"rectangular\"");
      OXS_THROW(Oxs_BadParameter,errmsg);
    }

    Vf_OvfDataStyle datastyle;
    String textfmt;
    if(output_format_datatype.compare("text")==0) {
      datastyle = vf_oascii;
      textfmt = output_format_precision;
    } else if(output_format_datatype.compare("binary")==0){
      if(output_format_precision.compare("4")==0) {
        datastyle = vf_obin4;
      } else if(output_format_precision.compare("8")==0) {
        datastyle = vf_obin8;
      } else {
        String errmsg =
          String("Error in Oxs_VectorFieldOutput<T>::Output():")
          + String(" Bad binary precision: \"")
          + output_format_precision
          + String("\"  Should be either \"4\" or \"8\"");
        OXS_THROW(Oxs_BadParameter,errmsg);
      }
    } else {
      String errmsg =
        String("Error in Oxs_VectorFieldOutput<T>::Output():")
        + String(" Bad datatype: \"")
        + String(output_format_datatype)
        + String("\"  Should be either \"binary\" or \"text\"");
      OXS_THROW(Oxs_BadParameter,errmsg);
    }

    Vf_OvfFileVersion ovf_version = vf_ovf_20; // Default

    Oxs_MeshValueOutputField<ThreeVector>
      (filename,GetOutputWriteheaders(),LongName().c_str(),
       description,valuelabels,valueunits,
       meshtype,   // Either rectangular or irregular
       datastyle,   // vf_oascii, vf_obin4, or vf_obin8
       textfmt.c_str(),state->GetMeshNodesPtr(),&cache.value,
       ovf_version);
  } catch (Oxs_ExtError& err) {
    // Include filename in error message
    char buf[1500];
    Oc_Snprintf(buf,sizeof(buf),
                "Error writing Ovf file \"%.1000s\", state id %u: ",
                filename,state->Id());
    Tcl_ResetResult(interp); // Protection against badly behaved Oxs_Ext's.
    Tcl_AppendResult(interp,buf,err.c_str(),(char *)NULL);
    errorcode = TCL_ERROR;
  } catch (Oxs_Exception& err) {
    // Include filename in error message
    char buf[1500];
    Oc_Snprintf(buf,sizeof(buf),
                "Error writing Ovf file \"%.1000s\", state id %u: ",
                filename,state->Id());
    err.Prepend(buf);

    // Use directory of filename as error message subtype
    Oc_AutoBuf fullname,dirname;
    Oc_DirectPathname(filename,fullname);
    Oc_FileDirname(fullname,dirname);
    err.SetSubtype(dirname.GetStr());

    // Rethrow
    if(GetCacheRequestCount()<1) {
      cache.state_id=0;
      cache.value.Release();
    }
    throw;
  } catch (...) {
    // Unexpected exception; Clean-up and rethrow.
    if(GetCacheRequestCount()<1) {
      cache.state_id=0;
      cache.value.Release();
    }
    throw;
  }

  // Release buffer if caching is disabled
  if(GetCacheRequestCount()<1) {
    cache.state_id=0;
    cache.value.Release();
  }

  // On success, return value is the filename.
  if(errorcode==TCL_OK) {
    Tcl_ResetResult(interp); // Protection against badly behaved Oxs_Ext's.
    Tcl_AppendResult(interp,filename,(char *)NULL);
  }
  return errorcode;
}

//////////////////////////////////////////////////////////////
// Oxs_ScalarFieldOutput<T> implementation
template<class T>
int Oxs_ScalarFieldOutput<T>::SetOutputFormat
(const String& format)
{
  if(format.length()==0) return 1;
  Nb_SplitList arglist;
  arglist.Split(format.c_str());
  if(arglist.Count()!=2) {
    OXS_THROW(Oxs_BadParameter,
              "Wrong number of arguments in format string import to "
              "Oxs_ScalarFieldOutput<T>::SetOutputFormat(const char*)");
  }
  output_format_datatype = arglist[0];
  output_format_precision = arglist[1];
  return 0;
}

template<class T>
String Oxs_ScalarFieldOutput<T>::GetOutputFormat() const
{
  vector<String> sarr;
  sarr.push_back(output_format_datatype);
  sarr.push_back(output_format_precision);
  return Nb_MergeList(&sarr);
}

template<class T>
int Oxs_ScalarFieldOutput<T>::SetOutputMeshtype
(const String& meshtype)
{
  if(meshtype.length()==0) return 1;
  output_format_meshtype = meshtype;
  return 0;
}

template<class T>
String Oxs_ScalarFieldOutput<T>::GetOutputMeshtype() const
{
  return output_format_meshtype;
}

template<class T>
int Oxs_ScalarFieldOutput<T>::UpdateCache
(const Oxs_SimState* state)
{ // Returns 1 on success, 0 if caching not supported or not enabled.
  // Throws an exception if an error occurs.

  if(!IsCacheSupported() || GetCacheRequestCount()<1) return 0;

  if(cache.state_id!=state->Id()) {
    ForceCacheIncrement(1);
    cache.state_id=0; // Safety
    cache.value.AdjustSize(state->GetMeshNodesPtr());
    (owner->*getscalarfield)(*state);
    ForceCacheIncrement(-1);

    // Check that cache got filled
    if(cache.state_id!=state->Id()) {
      OXS_THROW(Oxs_ProgramLogicError,
               "Output cache not filled by registered getscalarfield"
               " call (as required).");
    }
  }

  return 1;
}


template<class T>
int Oxs_ScalarFieldOutput<T>::Output
(const Oxs_SimState* state, // State to get output for
 Tcl_Interp* interp,        // Tcl interpreter
 int argc,CONST84 char** argv)      // Args
{
  Tcl_ResetResult(interp);
  if(argc!=1) {
    Tcl_AppendResult(interp, "wrong # of args: should be 1: filename",
                     (char *) NULL);
    return TCL_ERROR;
  }
  if(state==NULL || state->Id()==0) {
    Tcl_AppendResult(interp,"Invalid state passed to output object",
                     (char *)NULL);
    return TCL_ERROR;
  }

  const char* filename = argv[0];

  if(!IsCacheSupported() || cache.state_id!=state->Id()) {
    // Fill cache with scalar field
    ForceCacheIncrement(1);
    cache.state_id=0; // Safety
    cache.value.AdjustSize(state->GetMeshNodesPtr());
    (owner->*getscalarfield)(*state);
    ForceCacheIncrement(-1);

    // Check that cache got filled
    if(cache.state_id!=state->Id()) {
      OXS_THROW(Oxs_ProgramLogicError,
               "Output cache not filled by registered getscalarfield"
               " call (as required).");
    }
  }

  // Write cache to file
  char description[4096];
  Oc_Snprintf(description,sizeof(description),
              "Oxs %.80s output\n"
              " MIF source file: %.256s\n"
              " Iteration: %u, State id: %u\n"
              " Stage: %u, Stage iteration: %u\n"
              " Stage simulation time: %g s\n"
              " Total simulation time: %g s\n",
              OutputType().c_str(),
              GetProblemName(),
              state->iteration_count,state->Id(),
              state->stage_number,state->stage_iteration_count,
              static_cast<double>(state->stage_elapsed_time),
              static_cast<double>(state->stage_start_time
                                  + state->stage_elapsed_time));
  int errorcode = TCL_OK;
  try {
    vector<String> valuelabels;
    valuelabels.push_back(OutputName());

    vector<String> valueunits;
    valueunits.push_back(OutputUnits());

    Vf_Ovf20_MeshType meshtype;
    if(output_format_meshtype.compare("irregular")==0) {
      meshtype = vf_ovf20mesh_irregular;
    } else if(output_format_meshtype.compare("rectangular")==0) {
      meshtype = vf_ovf20mesh_rectangular ;
    } else {
      String errmsg =
        String("Error in Oxs_ScalarFieldOutput<T>::Output():")
        + String(" Bad meshtype: \"")
        + String(output_format_meshtype)
        + String("\"  Should be either \"irregular\" or \"rectangular\"");
      OXS_THROW(Oxs_BadParameter,errmsg);
    }

    Vf_OvfDataStyle datastyle;
    String textfmt;
    if(output_format_datatype.compare("text")==0) {
      datastyle = vf_oascii;
      textfmt = output_format_precision;
    } else if(output_format_datatype.compare("binary")==0){
      if(output_format_precision.compare("4")==0) {
        datastyle = vf_obin4;
      } else if(output_format_precision.compare("8")==0) {
        datastyle = vf_obin8;
      } else {
        String errmsg =
          String("Error in Oxs_ScalarFieldOutput<T>::Output():")
          + String(" Bad binary precision: \"")
          + output_format_precision
          + String("\"  Should be either \"4\" or \"8\"");
        OXS_THROW(Oxs_BadParameter,errmsg);
      }
    } else {
      String errmsg =
        String("Error in Oxs_ScalarFieldOutput<T>::Output():")
        + String(" Bad datatype: \"")
        + String(output_format_datatype)
        + String("\"  Should be either \"binary\" or \"text\"");
      OXS_THROW(Oxs_BadParameter,errmsg);
    }

    Vf_OvfFileVersion ovf_version = vf_ovf_20; // Default

    Oxs_MeshValueOutputField<OC_REAL8m>
      (filename,GetOutputWriteheaders(),LongName().c_str(),
       description,valuelabels,valueunits,
       meshtype,   // Either rectangular or irregular
       datastyle,   // vf_oascii, vf_obin4, or vf_obin8
       textfmt.c_str(),state->GetMeshNodesPtr(),&cache.value,
       ovf_version);
  } catch (Oxs_ExtError& err) {
    char buf[1500];
    Oc_Snprintf(buf,sizeof(buf),
                "Error writing Ovf file \"%.1000s\", state id %u: ",
                filename,state->Id());
    Tcl_ResetResult(interp); // Protection against badly behaved Oxs_Ext's.
    Tcl_AppendResult(interp,buf,err.c_str(),(char *)NULL);
    errorcode = TCL_ERROR;
  } catch (...) {
    // Unexpected exception; Clean-up and rethrow.
    if(GetCacheRequestCount()<1) {
      cache.state_id=0;
      cache.value.Release();
    }
    throw;
  }

  // Release buffer if caching is disabled
  if(GetCacheRequestCount()<1) {
    cache.state_id=0;
    cache.value.Release();
  }

  // On success, return value is the filename.
  if(errorcode==TCL_OK) {
    Tcl_ResetResult(interp); // Protection against badly behaved Oxs_Ext's.
    Tcl_AppendResult(interp,filename,(char *)NULL);
  }
  return errorcode;
}


#endif // _OXS_OUTPUTDERIV
