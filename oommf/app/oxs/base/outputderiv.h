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

#include <cstdlib>
#include <string>
#include <utility>

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

// Support for deprecated cache_support option to Oxs_Output::Setup().
// Set to 1 to enable for third-party contributed extensions.
#define SUPPORT_DEPRECATED_CACHE_SUPPORT 1

class Oxs_Director;   // Forward declarations
class Oxs_Ext;
class Oxs_SimState;

/////////////////////////////////////////////////////////////////////
////////////////////////// SCALAR OUTPUT ////////////////////////////

//////////////////////////////////////////////////////////////////
// Templated cache class, used by templated output classes.
template<class T> struct Oxs_OutputCache {
public:
  Oxs_OutputCache() : state_id(0) {}
  T value;
  OC_UINT4m state_id;
};

//////////////////////////////////////////////////////////////////
// Templated output classes.  The templated class T should be the
// embedding class.

////////////////////////
// Scalar output - non-templated base part.  This is accessible
// via downcast from Oxs_Output (unlike the templated part, which
// can only be downcast to if the template type is known).
class Oxs_BaseScalarOutput : public Oxs_Output {
protected:
  String output_format; // Default is "%.17g"

private:
  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_BaseScalarOutput(const Oxs_BaseScalarOutput &);
  Oxs_BaseScalarOutput& operator=(const Oxs_BaseScalarOutput&);

public:
  Oxs_BaseScalarOutput() : Oxs_Output(),
    output_format("%.17g")
    {
      cache.value=0; // Safety
      CacheRequestIncrement(1);  // Temporary measure: until /**/
      /// cache management is implemented in Tcl interface,  /**/
      /// enable caching on all scalar outputs.              /**/
    }

  ~Oxs_BaseScalarOutput() {
    // Deregister while the base pointer can still be downcast.
    Deregister();
  }

  // Public data member for result drop-off/storage.
  // The owner should test GetCacheRequestCount() inside the output
  // calculation code, and adjust the cache as needed.
  Oxs_OutputCache<OC_REAL8m> cache;

  virtual void GetNames(vector<String>& label_value_names) const {
    GetBaseNames(label_value_names);
  }

  int SetOutputFormat(const String& format);
  String GetOutputFormat() const { return output_format; }

  virtual OC_REAL8m GetValue(const Oxs_SimState* state) =0;
};

////////////////////////
// Scalar output, non-chunked - templated part:

template<class T> class Oxs_ScalarOutput:public Oxs_BaseScalarOutput {
  using GetScalar = void (T::*)(const Oxs_SimState&);
private:
  T* owner;
  GetScalar getscalar;
  /// Without the alias, this would be
  ///    void (T::* getscalar)(const Oxs_SimState&)

  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_ScalarOutput(const Oxs_ScalarOutput<T> &);
  Oxs_ScalarOutput<T>& operator=(const Oxs_ScalarOutput<T>&);

public:
  Oxs_ScalarOutput() : Oxs_BaseScalarOutput(), owner(NULL), getscalar(NULL) {}

  void Setup(T* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             const GetScalar member_function)
  {
    owner = owner_;
    getscalar = member_function;
    Oxs_Output::Setup(owner_name_,output_name_,"scalar",output_units_);
  }
#if SUPPORT_DEPRECATED_CACHE_SUPPORT
  void Setup(T* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             OC_BOOL /* cache_support_ */,
             const GetScalar member_function)
  { // Interface for backward compatibility; the cache_support option
    // is ignored and deprecated.
    Setup(owner_,owner_name_,output_name_,output_units_,member_function);
  }
#endif // SUPPORT_DEPRECATED_CACHE_SUPPORT

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
  // We can't directly access member functions of director here because
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

  OC_REAL8m GetValue(const Oxs_SimState* state);

  virtual int Output(const Oxs_SimState* state,Tcl_Interp* interp,
                     int argc,const char** argv);
  // The owner getscalar() function can assume on entry that the
  // cache value stored here is invalid, but it may become valid
  // during the lifetime of the getscalar() call (if getscalar()
  // calls some other function that sets the cache).
};

////////////////////////
// Chunked scalar output - non-templated base part.  These output
// objects support computing their output chunk-wise, in parallel.
class Oxs_BaseChunkScalarOutput : public Oxs_BaseScalarOutput {
private:
  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_BaseChunkScalarOutput(const Oxs_BaseChunkScalarOutput&);
  Oxs_BaseChunkScalarOutput& operator=(const Oxs_BaseChunkScalarOutput&);

public:
  Oxs_BaseChunkScalarOutput() : Oxs_BaseScalarOutput() {}
  virtual ~Oxs_BaseChunkScalarOutput() {}

  virtual void ChunkScalarOutputInitialize
  (const Oxs_SimState*,int number_of_threads) =0;

  virtual void ChunkScalarOutput(const Oxs_SimState*,
                          OC_INDEX node_start,OC_INDEX node_stop,
                          int threadnumber) =0;

  virtual void ChunkScalarOutputFinalize
  (const Oxs_SimState*,int number_of_threads) =0;

  virtual void ChunkSharedScalarOutputs
  (std::vector<Oxs_BaseChunkScalarOutput*>& buddy_list) =0;
  // Returns a list, including *this, of Oxs_BaseChunkScalarOutput objects
  // produced by ChunkScalarOutputInitialize. ChunkScalarOutput, and
  // ChunkScalarOutputFinalize.  WRT the Oxs_Output::Output interface,
  // those are called serially, so if multiple objects are set by one
  // then subsequent calls should be automatically disabled by cache
  // flags.

  // Implementations for (non-parallel, sequential) Oxs_Output::Output()
  // and Oxs_BaseScalarOutput::GetValue() are provided by this class, in
  // terms of the ChunkScalarOutput*() virtual functions --- see the
  // outputderiv.cc file.
  int Output(const Oxs_SimState*,Tcl_Interp*,int argc,const char** argv);
  OC_REAL8m GetValue(const Oxs_SimState* state);
};


////////////////////////
// Scalar output, chunked - templated part:

template<class T> class Oxs_ChunkScalarOutput
  : public Oxs_BaseChunkScalarOutput {

  typedef void (T::* GetScalarChunkInitialize)(const Oxs_SimState&,int);
  typedef void (T::* GetScalarChunk)(const Oxs_SimState&,
                                    OC_INDEX,OC_INDEX,int);
  typedef void (T::* GetScalarChunkFinalize)(const Oxs_SimState&,int);

  typedef void (T::* GetScalarChunkShareList)
  (std::vector<Oxs_BaseChunkScalarOutput*>& buddy_list);

private:
  GetScalarChunkInitialize getscalarchunkinit;
  GetScalarChunk           getscalarchunk;
  GetScalarChunkFinalize   getscalarchunkfini;
  GetScalarChunkShareList  getscalarchunkshares;

  T* owner;  // 32-bit Visual C++ 2010 complains about data alignment if
  /// this pointer is placed before the getscalarchunk* member function
  /// calls.

  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_ChunkScalarOutput(const Oxs_ChunkScalarOutput<T> &);
  Oxs_ChunkScalarOutput<T>& operator=(const Oxs_ChunkScalarOutput<T>&);

public:
  Oxs_ChunkScalarOutput() : Oxs_BaseChunkScalarOutput(),
                            getscalarchunkinit(0),getscalarchunk(0),
                            getscalarchunkfini(0),getscalarchunkshares(0),
                            owner(0) {}

  void Setup(T* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             const GetScalarChunkInitialize member_function_init_,
             const GetScalarChunk member_function_,
             const GetScalarChunkFinalize member_function_fini_,
             const GetScalarChunkShareList member_function_shares_)
  {
    owner = owner_;
    getscalarchunkinit   = member_function_init_;
    getscalarchunk       = member_function_;
    getscalarchunkfini   = member_function_fini_;
    getscalarchunkshares = member_function_shares_;
    Oxs_Output::Setup(owner_name_,output_name_,"scalar",output_units_);
  }
#if 0 // SUPPORT_DEPRECATED_CACHE_SUPPORT (not needed?)
  void Setup(T* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             OC_BOOL /* cache_support_ */,
             const GetScalarChunkInitialize member_function_init_,
             const GetScalarChunk member_function_,
             const GetScalarChunkFinalize member_function_fini_,
             const GetScalarChunkShareList member_function_shares_)
  { // Interface for backward compatibility; the cache_support option
    // is ignored and deprecated.
    Setup(owner_,owner_name_,output_name_,output_units_,
          member_function_init_,member_function_,
          member_function_fini_,member_function_shares_);
  }
#endif // SUPPORT_DEPRECATED_CACHE_SUPPORT

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

  void ChunkScalarOutputInitialize(const Oxs_SimState* state,
                                   int number_of_threads);
  void ChunkScalarOutput(const Oxs_SimState* state,
                         OC_INDEX node_start,OC_INDEX node_stop,
                         int threadnumber);
  void ChunkScalarOutputFinalize(const Oxs_SimState* state,
                                 int number_of_threads);

  void ChunkSharedScalarOutputs
  (std::vector<Oxs_BaseChunkScalarOutput*>& buddy_list) {
      (owner->*getscalarchunkshares)(buddy_list);
  }
};

//////////////////////////////////////////////////////////////
// Oxs_BaseScalarOutput implementation
inline int Oxs_BaseScalarOutput::SetOutputFormat
(const String& format)
{
  if(format.length()==0) return 1;
  output_format = format;
  return 0;
}

//////////////////////////////////////////////////////////////
// Oxs_ScalarOutput<U> implementation
template<class U>
OC_REAL8m Oxs_ScalarOutput<U>::GetValue
(const Oxs_SimState* state)   // State to get output for
{
  if(state==NULL || state->Id()==0) {
    OXS_THROW(Oxs_BadParameter,
              "Invalid state passed to output object");
  }
  if(cache.state_id!=state->Id()) {
    // Fill cache with data
    CacheRequestIncrement(1);
    cache.state_id=0; // Safety
    (owner->*getscalar)(*state);
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

template<class U>
int Oxs_ScalarOutput<U>::Output
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



//////////////////////////////////////////////////////////////
// Oxs_ChunkScalarOutput<U> implementations
template<class U>
void Oxs_ChunkScalarOutput<U>::ChunkScalarOutputInitialize
(const Oxs_SimState* state,int number_of_threads)
{
  if(state==NULL || state->Id()==0) {
    OXS_THROW(Oxs_BadParameter,
              "Invalid state passed to output object");
  }
  Oxs_Output::CacheRequestIncrement(1);  // Fill cache with data
  cache.state_id = 0; // Safety
  (owner->*getscalarchunkinit)(*state,number_of_threads);
}

template<class U>
void Oxs_ChunkScalarOutput<U>::ChunkScalarOutput
(const Oxs_SimState* state,
 OC_INDEX node_start,
 OC_INDEX node_stop,
 int threadnumber)
{
  (owner->*getscalarchunk)(*state,node_start,node_stop,threadnumber);
}

template<class U>
void Oxs_ChunkScalarOutput<U>::ChunkScalarOutputFinalize
(const Oxs_SimState* state,int number_of_threads)
{
  (owner->*getscalarchunkfini)(*state,number_of_threads);
  CacheRequestIncrement(-1);

  // Check that cache got filled
  if(cache.state_id!=state->Id()) {
    OXS_THROW(Oxs_ProgramLogicError,
              "Output cache not filled by registered getscalarchunk"
              " call (as required).");
  }
}

/////////////////////////////////////////////////////////////////////
/////////////////////////// FIELD OUTPUT ////////////////////////////

// Define three field output base classes, two cache control classes and
// one generic initialization and output class, w/o an owner template
// parameter so clients can access values and functionality uniformly
// w/o knowing the object owner. The template type T is assumed to be
// either OC_REAL8m (for scalar fields) or ThreeVector (for vector
// fields), but additional types could be supported with additional
// template member function specializations. The first cache class
// supports the old-style Oxs_{Scalar,ThreeVector}FieldOutput classes,
// the latter the newer Oxs_SimState-based field caching scheme.  The
// generic output class defines all the higher-level non-owner specific
// methods. These are mostly T-type invariant, with specializations
// handling the anomalies.  The concrete owner-specific child classes
// inherit from the generic output class and one of the cache classes
// (multiple-inheritance).
//
// Migration note: Some Oxs_VectorFieldOutput instances share their
// cache buffers with main-path compute code in their owners. When
// migrating from Oxs_VectorFieldOutput to Oxs_SimStateVectorFieldOutput
// be sure to reestablish this connection, otherwise output requests may
// instigate unnecessary calls into the energy computation code. (This
// might be detectable as an increase in the "Energy calc count"
// DataTable output.)

// Generic owner-agnostic output base class.
template<class T>
class Oxs_FieldOutput : public Oxs_Output {
private:
  OC_BOOL output_format_writeheaders; // Default is 1

  String output_format_meshtype;   // Default is "rectangular"
  String output_format_datatype;   // Default is "binary"
  String output_format_precision;  // Default is "8"

protected:
  // Cache access routines needed by Oxs_FieldOutput::Output()
  // member. UpdateCache ensures the cached data is current
  // for the specified Oxs_SimState. GetCacheBuffer returns
  // a pointer to the cache buffer.
  // NB: Make sure that cache_request_count is bigger than zero
  //     before calling UpdateCache(), because even though
  //     UpdateCache() will update the cache regardless, cache
  //     control may release it if cache_request_count is
  //     smaller than 1.
  virtual void UpdateCache(const Oxs_SimState* state)=0;
  virtual const Oxs_MeshValue<T>*
     GetCacheBuffer(const Oxs_SimState* state) const=0;

  String output_filename_script; // Empty string to use default
  /// file naming method, otherwise script taking a "params"
  /// dictionary that is run inside the MIF interp.

  // Oxs_FieldOutput::UpdateCache() is used by ::Output() and
  // Oxs_Ext::Fill__user_outputs_init().
  friend class Oxs_Ext;

public:
  // Disable copy constructor and assignment operators.
  // These are public so attempted use generates a deleted
  // function message instead of a privacy violation.
  Oxs_FieldOutput
  (const Oxs_FieldOutput<T>&) = delete;
  Oxs_FieldOutput<T>& operator=
  (const Oxs_FieldOutput<T>&) = delete;

  Oxs_FieldOutput() : Oxs_Output(),
    output_format_writeheaders(1),
    output_format_meshtype("rectangular"),
    output_format_datatype("binary"),
    output_format_precision("8")
    {}

  void Register(Oxs_Director* director_,OC_INT4m priority_);

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

  virtual void GetNames(vector<String>& label_value_names) const {
    GetBaseNames(label_value_names);
    label_value_names.emplace_back("filename_script");
    label_value_names.emplace_back(output_filename_script);
  }

  virtual int SetOutputFormat(const String& format);
  virtual String GetOutputFormat() const;

  int SetOutputWriteheaders(OC_BOOL writeheaders) {
    output_format_writeheaders = writeheaders;
    return 0;
  }
  OC_BOOL GetOutputWriteheaders() const {
    return output_format_writeheaders;
  }

  int SetOutputMeshtype(const String& meshtype) {
    if(meshtype.length()==0) return 1;
    output_format_meshtype = meshtype;
    return 0;
  }
  String GetOutputMeshtype() const {
    return output_format_meshtype;
  }

  void FillValueLabelsAndUnits(vector<String>& valuelabels,
                               vector<String>& valueunits) const;

  int Output(const Oxs_SimState* state,     // State to output
             Tcl_Interp* interp,            // Tcl interpreter
             int argc,const char** argv); // Args
};

// Declare explicit member specializations so the compiler knows to not
// provide the generic version. The definitions are in the .cc file,
// since full specializations are not templates.
template<>
void Oxs_FieldOutput<OC_REAL8m>::Register
(Oxs_Director* director_,OC_INT4m priority_);
template<>
void Oxs_FieldOutput<OC_REAL8m>::FillValueLabelsAndUnits
(vector<String>& valuelabels,vector<String>& valueunits) const;

template<>
void Oxs_FieldOutput<ThreeVector>::Register
(Oxs_Director* director_,OC_INT4m priority_);
template<>
void Oxs_FieldOutput<ThreeVector>::FillValueLabelsAndUnits
(vector<String>& valuelabels,vector<String>& valueunits) const;

//////////////////////////////////////////
// Implementations for Oxs_FieldOutput
template<class T>
int Oxs_FieldOutput<T>::SetOutputFormat
(const String& format)
{
  if(format.length()==0) return 1;
  Nb_SplitList arglist;
  arglist.Split(format.c_str());
  if(arglist.Count()!=2) {
    OXS_THROW(Oxs_BadParameter,
              "Wrong number of arguments in format string import to "
              "Oxs_FieldOutput<T>::SetOutputFormat(const char*)");
  }
  output_format_datatype = arglist[0];
  output_format_precision = arglist[1];
  return 0;
}

template<class T>
String Oxs_FieldOutput<T>::GetOutputFormat() const
{
  vector<String> sarr;
  sarr.push_back(output_format_datatype);
  sarr.push_back(output_format_precision);
  return Nb_MergeList(&sarr);
}

template<class T>
int Oxs_FieldOutput<T>::Output
(const Oxs_SimState* state, // State to get output for
 Tcl_Interp* interp,        // Tcl interpreter
 int argc,const char** argv)      // Args
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
  CacheRequestIncrement(1);
  UpdateCache(state);

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
    vector<String> valueunits;
    FillValueLabelsAndUnits(valuelabels,valueunits);
    Vf_Ovf20_MeshType meshtype;
    if(output_format_meshtype.compare("irregular")==0) {
      meshtype = vf_ovf20mesh_irregular;
    } else if(output_format_meshtype.compare("rectangular")==0) {
      meshtype = vf_ovf20mesh_rectangular ;
    } else {
      String errmsg =
        String("Error in Oxs_VectorFieldOutput<U>::Output():")
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
          String("Error in Oxs_FieldOutput<U>::Output():")
          + String(" Bad binary precision: \"")
          + output_format_precision
          + String("\"  Should be either \"4\" or \"8\"");
        OXS_THROW(Oxs_BadParameter,errmsg);
      }
    } else {
      String errmsg =
        String("Error in Oxs_FieldOutput<U>::Output():")
        + String(" Bad datatype: \"")
        + String(output_format_datatype)
        + String("\"  Should be either \"binary\" or \"text\"");
      OXS_THROW(Oxs_BadParameter,errmsg);
    }

    Vf_OvfFileVersion ovf_version = vf_ovf_20; // Default

    Oxs_MeshValueOutputField<T>
      (filename,GetOutputWriteheaders(),Oxs_Output::LongName().c_str(),
       description,valuelabels,valueunits,
       meshtype,   // Either rectangular or irregular
       datastyle,   // vf_oascii, vf_obin4, or vf_obin8
       textfmt.c_str(),state->GetMeshNodesPtr(),
       GetCacheBuffer(state),
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
    Oc_TclFileCmd("dirname",fullname,dirname);
    err.SetSubtype(dirname.GetStr());

    // Rethrow
    CacheRequestIncrement(-1);
    throw;
  } catch (...) {
    // Unexpected exception; Clean-up and rethrow.
    CacheRequestIncrement(-1);
    throw;
  }

  CacheRequestIncrement(-1); // We're done with the cache

  // On success, return value is the filename.
  if(errorcode==TCL_OK) {
    Tcl_ResetResult(interp); // Protection against badly behaved Oxs_Ext's.
    Tcl_AppendResult(interp,filename,(char *)NULL);
  }
  return errorcode;
}

// Implementations for Oxs_FieldOutput
//////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// Old-style cache class. The member-function cache-control interface
// supersedes direct cache member access (which is deprecated).
template<class T>
class Oxs_FieldOutputCacheControl
  : public Oxs_FieldOutput<T> {
protected:
  friend class Oxs_Ext;
  virtual void UpdateCache(const Oxs_SimState* state) override {
    // Throws an exception if an error occurs.
    assert(Oxs_Output::GetCacheRequestCount()>0);
    /// NOP if cache_request_count == 0.
    if(IsCacheValid(state)) { return; }
    ResetCache(state); // Safety;
    OwnerCacheFill(*state); // calls owner->*getvectorfield)
    assert(IsCacheValid(state));
  }

  virtual void OwnerCacheFill(const Oxs_SimState& state)=0;
  /// This is used in UpdateCache(). Concrete child classes implement
  /// this using their getvectorfield member.

public:
  // Disable copy constructor and assignment operators.
  // These are public so attempted use generates a deleted
  // function message instead of a privacy violation.
  Oxs_FieldOutputCacheControl
   (const Oxs_FieldOutputCacheControl&) = delete;
  Oxs_FieldOutputCacheControl& operator=
   (const Oxs_FieldOutputCacheControl&) = delete;

  // Default constructor
  Oxs_FieldOutputCacheControl() {}

  // Public data member for result drop-off/storage.  The owner should
  // test GetCacheRequestCount() inside the output calculation code, and
  // adjust the cache as needed.
  struct Oxs_OutputCache {
  public:
    Oxs_OutputCache() : state_id(0) {}
    Oxs_MeshValue<T> value;
    OC_UINT4m state_id;
  } cache;

  // Augment default CacheRequestIncrement() base member
  virtual void CacheRequestIncrement(OC_INT4m incr) override {
    Oxs_Output::CacheRequestIncrement(incr);
    if(Oxs_Output::GetCacheRequestCount()<1) {
      cache.state_id=0;
      cache.value.Release();
    }
  }

  // Use the following member routines in preference to direct cache
  // access. These routines provide built-in support for Oxs_MeshValue<>
  // buffer locking and sharing.
  void ResetCache() {
    cache.state_id = 0;
    cache.value.Reset();
  }
  void ResetCache(const Oxs_SimState* state) {
    if(state == nullptr) ResetCache();
    const Vf_Ovf20_MeshNodes* mesh = state->GetMeshNodesPtr();
    if(cache.value.CheckMesh(mesh)) {
      // Current size matches requested size
      ResetCache();
    } else {
      // Otherwise, release current buffer and replace with one of the
      // proper size.
      cache.value.AdjustSize(mesh);
      cache.state_id = 0;
    }
  }

  OC_BOOL IsCacheValid(const Oxs_SimState* state) {
    return (state->Id() == cache.state_id);
  }
  OC_UINT4m GetCacheStateId() const {
    return cache.state_id;
  }
  void SetCacheStateId(OC_UINT4m newid) {
    // To protect against data corruption, the idiom for changing the
    // cache is to first reset the state_id to 0, then fill the buffer
    // with new data, and finally set the state_id to the new
    // value. This routine throws an exception if cache.state_id on
    // entry is not zero, or is a non-zero value different from newid.
    if(cache.state_id == 0) {
      cache.state_id = newid;   return;
    }
    if(cache.state_id == newid) return;
    OXS_THROW(Oxs_ProgramLogicError,
              "Output cache changed without reset.");
  }
  Oxs_MeshValue<T>* GetCacheBuffer(const Oxs_SimState* state) {
    if(cache.state_id != state->Id()) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Oxs_FieldOutputCacheControl::GetCacheBuffer()"
                " state mismatch.");
    }
    return &(cache.value);
  }
  virtual const Oxs_MeshValue<T>*
  GetCacheBuffer(const Oxs_SimState* state) const override {
    if(cache.state_id != state->Id()) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Oxs_FieldOutputCacheControl::GetCacheBuffer() const"
                " state mismatch.");
    }
    return &(cache.value);
  }
};

//////////////////////////////////////////////////////////////////////
// Oxs_SimState-based cache class
template<class T>
class Oxs_SimStateFieldOutputCacheControl
  : public Oxs_FieldOutput<T> {
protected:
  virtual void UpdateCache(const Oxs_SimState* state) override {
    assert(Oxs_Output::GetCacheRequestCount()>0);
    /// NOP if cache_request_count == 0.
    if(IsCacheValid(state)) { return; }
    OwnerCacheFill(*state); // calls owner->*getvectorfield)
    assert(IsCacheValid(state));
  }

  virtual void OwnerCacheFill(const Oxs_SimState& state)=0;
  /// This is used in UpdateCache(). Concrete child classes implement
  /// this using their getvectorfield member.

public:
  // Disable copy constructor and assignment operators.
  // These are public so attempted use generates a deleted
  // function message instead of a privacy violation.
  Oxs_SimStateFieldOutputCacheControl
   (const Oxs_SimStateFieldOutputCacheControl&) = delete;
  Oxs_SimStateFieldOutputCacheControl& operator=
   (const Oxs_SimStateFieldOutputCacheControl&) = delete;

  // Default constructor
  Oxs_SimStateFieldOutputCacheControl() {}


  OC_BOOL IsCacheValid(const Oxs_SimState* state) {
    const Oxs_MeshValue<T>* valueptr;
    return state->GetDerivedData(Oxs_Output::LongName(),valueptr);
  }

  // GetCacheBuffer is functionally the same as IsCacheValid, except that
  // on success the return value is a pointer to the cache; on error a
  // nullptr is returned.
  virtual const Oxs_MeshValue<T>*
  GetCacheBuffer(const Oxs_SimState* state) const override {
    const Oxs_MeshValue<T>* valueptr;
    if(state->GetDerivedData(Oxs_Output::LongName(),valueptr)) {
      return valueptr;
    }
    return nullptr;
  }

  // To fill the cache a client should create an Oxs_MeshValue sized to
  // match state->mesh, and fill it as appropriate for the state. Then
  // call MoveToCache to *move* the Oxs_MeshValue into the state's
  // DerivedData area with this output's reference name. If the return
  // value is true, then the move was successful and the Oxs_MeshValue
  // *will be empty* on the client side.  If the return value is false
  // then the move was not successful (probably because state already
  // has this output set in its DerivedData area), and the Oxs_MeshValue
  // will be unchanged on the client side.
  //   The ShareToCache() function is an alternative to MoveToCache()
  // which creates a shared copy of value and moves that into the state
  // DerivedData area. With this routine the import value is still
  // accessible from the client side, but if the share was successful
  // (as indicated by the OC_BOOL return value) then the read_only_lock
  // will be set on both copies of value (i.e., in the client and in the
  // state DerivedData area). Actually, the read_only_lock might be set
  // even if the return value is false, if the state->AddDerivedData()
  // call fails for some reason even after the state->HavedDerivedData()
  // call returns false.
  OC_BOOL MoveToCache(const Oxs_SimState* state,
                      Oxs_MeshValue<T>& value) {
    return state->AddDerivedData(Oxs_Output::LongName(),std::move(value));
  }
  OC_BOOL ShareToCache(const Oxs_SimState* state,
                      Oxs_MeshValue<T>& value) {
    // If value is already attached to state, do nothing
    if(state->HaveDerivedData(Oxs_Output::LongName(),&value)) {
      return 0;
    }
    // Otherwise, move a shared copy into state
    return state->AddDerivedData(Oxs_Output::LongName(),
                                 std::move(value.SharedCopy()));
  }

  // IDEA: Perhaps the class interface should have an init(state)
  //       member that sets up an fresh Oxs_MeshValue object buffer, and
  //       returns a writable pointer to it. The client fills
  //       the buffer, and then calls a set or close member that
  //       moves the buffer into the state derived area.
};


//////////////////////////////////////////////////////////////////////
// Concrete child classes

// Old-style scalar field output class. The template U parameter is the
// output "owner" class.
template<class U> class Oxs_ScalarFieldOutput
  : public Oxs_FieldOutputCacheControl<OC_REAL8m>
{
  using GetScalarField = void (U::*)(const Oxs_SimState&);
private:
  U* owner;
  GetScalarField getscalarfield;
  /// Without the alias, this would be
  ///    void (U::* getvectorfield)(const Oxs_SimState&)

  virtual void OwnerCacheFill(const Oxs_SimState& state) {
    (owner->*getscalarfield)(state);
  }

public:
  // Disable copy constructor and assignment operators.
  // These are public so attempted use generates a deleted
  // function message instead of a privacy violation.
  Oxs_ScalarFieldOutput(const Oxs_ScalarFieldOutput<U>&) = delete;
  Oxs_ScalarFieldOutput<U>&
  operator=(const Oxs_ScalarFieldOutput<U>&) = delete;

  Oxs_ScalarFieldOutput()
    : Oxs_FieldOutputCacheControl<OC_REAL8m>(),
      owner(nullptr), getscalarfield(nullptr)
  {}

  void Setup(U* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             const GetScalarField member_function)
    {
      owner = owner_;
      getscalarfield = member_function;
      output_filename_script.clear();
      owner->GetExtMifOption("scalar_field_output_filename_script",
                             output_filename_script);
      Oxs_Output::Setup(owner_name_,output_name_,"scalar field",
                        output_units_);
    }
#if SUPPORT_DEPRECATED_CACHE_SUPPORT
  void Setup(U* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             OC_BOOL /* cache_support_ */,
             const GetScalarField member_function)
  { // Interface for backward compatibility; the cache_support option
    // is ignored and deprecated.
    Setup(owner_,owner_name_,output_name_,output_units_,member_function);
  }
#endif // SUPPORT_DEPRECATED_CACHE_SUPPORT

};

// Old-style vector field output class. The template U parameter is the
// output "owner" class.
template<class U> class Oxs_VectorFieldOutput
  : public Oxs_FieldOutputCacheControl<ThreeVector>
{
  using GetVectorField = void (U::*)(const Oxs_SimState&);
private:
  U* owner;
  GetVectorField getvectorfield;
  /// Without the alias, this would be
  ///    void (U::* getvectorfield)(const Oxs_SimState&)

  virtual void OwnerCacheFill(const Oxs_SimState& state) {
    (owner->*getvectorfield)(state);
  }

public:
  // Disable copy constructor and assignment operators.
  // These are public so attempted use generates a deleted
  // function message instead of a privacy violation.
  Oxs_VectorFieldOutput(const Oxs_VectorFieldOutput<U>&) = delete;
  Oxs_VectorFieldOutput<U>&
     operator=(const Oxs_VectorFieldOutput<U>&) = delete;

  Oxs_VectorFieldOutput()
    : Oxs_FieldOutputCacheControl<ThreeVector>(),
      owner(nullptr), getvectorfield(nullptr)
  {}

  void Setup(U* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             const GetVectorField member_function)
    {
      owner = owner_;
      getvectorfield = member_function;
      output_filename_script.clear();
      owner->GetExtMifOption("vector_field_output_filename_script",
                             output_filename_script);
      Oxs_Output::Setup(owner_name_,output_name_,
                        "vector field",output_units_);
    }
#if SUPPORT_DEPRECATED_CACHE_SUPPORT
  void Setup(U* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             OC_BOOL /* cache_support_ */,
             const GetVectorField member_function)
  { // Interface for backward compatibility; the cache_support option
    // is ignored and deprecated.
    Setup(owner_,owner_name_,output_name_,output_units_,member_function);
  }
#endif // SUPPORT_DEPRECATED_CACHE_SUPPORT

};

// Oxs_SimState based scalar and vector field output template. T is
// either OC_REAL8m or ThreeVector, template U parameter is the output
// "owner" class.  Aliases with built-in T follow the declaration.
template<class T,class U> class Oxs_SimStateFieldOutput
  : public Oxs_SimStateFieldOutputCacheControl<T>
{
  using GetField = void (U::*)(const Oxs_SimState&);
private:
  U* owner;
  GetField getfield;
  /// Without the alias, this would be
  ///    void (U::* getfield)(const Oxs_SimState&)

  virtual void OwnerCacheFill(const Oxs_SimState& state) {
    (owner->*getfield)(state);
  }

  // Shim code for output labels, etc. Add additional types as needed.
  const char* ProtocolName(OC_REAL8m*) const noexcept {
    return "scalar field";
  }
  const char* ProtocolName(ThreeVector*) const noexcept {
    return "vector field";
  }
  const char* OutputFilenameScriptName(OC_REAL8m*) const noexcept {
    return "scalar_field_output_filename_script";
  }
  const char* OutputFilenameScriptName(ThreeVector*) const noexcept {
    return "vector_field_output_filename_script";
  }

public:
  // Disable copy constructor and assignment operators.
  // These are public so attempted use generates a deleted
  // function message instead of a privacy violation.
  Oxs_SimStateFieldOutput(const Oxs_SimStateFieldOutput<T,U>&) = delete;
  Oxs_SimStateFieldOutput<T,U>&
     operator=(const Oxs_SimStateFieldOutput<T,U>&) = delete;

  Oxs_SimStateFieldOutput()
    : Oxs_SimStateFieldOutputCacheControl<T>(),
      owner(nullptr), getfield(nullptr)
  {}

  void Setup(U* owner_,
             const char* owner_name_,const char* output_name_,
             const char* output_units_,
             const GetField member_function)
    {
      owner = owner_;
      getfield = member_function;
      Oxs_FieldOutput<T>::output_filename_script.clear();
      owner->GetExtMifOption(OutputFilenameScriptName(static_cast<T*>(nullptr)),
                             Oxs_FieldOutput<T>::output_filename_script);
      Oxs_Output::Setup(owner_name_,output_name_,
                        ProtocolName(static_cast<T*>(nullptr)),
                        output_units_);
    }
};

// Aliases for use with the preceding
template<class U> using Oxs_SimStateScalarFieldOutput
    = Oxs_SimStateFieldOutput<OC_REAL8m,U>;
template<class U> using Oxs_SimStateVectorFieldOutput
    = Oxs_SimStateFieldOutput<ThreeVector,U>;

#endif // _OXS_OUTPUTDERIV
