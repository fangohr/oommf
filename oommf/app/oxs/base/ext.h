/* FILE: ext.h                 -*-Mode: c++-*-
 *
 * Base OXS extension class.  This is an abstract class.
 *
 */

#ifndef _OXS_EXT
#define _OXS_EXT

#include <map>
#include <string>
#include <vector>

#include "oc.h"
#include "outputderiv.h"
#include "oxsexcept.h"
#include "exterror.h"
#include "lock.h"
#include "threevector.h"
#include "util.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported
OC_USE_STRING;

/* End includes */

/* Optional #ifdef's:
 *   #define REPORT_TIME 1/0
 *     This enables/disables the reporting of non-zero calculation
 *     time information.  If enabled, then at run end a report is
 *     written to stderr detailing the cpu and wall time spent
 *     computing each energy term, as well as the aggregate time spent
 *     in the step routine of the driver object.
 */
#ifndef REPORT_TIME
# define REPORT_TIME 0
#endif

////////////////////////////////////////////////////////////////////////
// Macro that builds a generic child constructor, and provides a static
// Oxs_ExtRegistrationBlock variable to force automatic class
// registration.  This macro should be invoked at the top of each
// Oxs_Ext child class *.cc class definition file that requires
// registration, i.e., any class for which object instantiation is
// desired.
//   The child constructor produced by this macro has the name
//
//          OxsExtNewChild##classname
//
// where "classname" is the macro argument, and the name of the Oxs_Ext
// child class being registered.
//   This macro also provides definitions for the class public member
// function ClassName().  This is used instead of an RTTI call, because
// we want to be able to associate this name reported by the class with
// entries in the input MIF file.  The RTTI "typeid().name()" function
// returns an implementation dependent string, which is not what we want.
#define OXS_EXT_REGISTER(classname)                               \
static Oxs_Ext*                                                   \
OxsExtNewChild##classname(const char* instancename,               \
                          Oxs_Director* newdtr,                   \
                          const char* argstr)                     \
{ return static_cast<Oxs_Ext*>(                                   \
     new classname(instancename,newdtr,argstr)); }                \
const char* classname::ClassName() const { return #classname; }   \
static Oxs_ExtRegistrationBlock oxsExt##classname##RegBlock(      \
#classname,OxsExtNewChild##classname)

class Oxs_Ext;       // Forward declarations
class Oxs_Director;

typedef Oxs_Ext* Oxs_ExtChildConstructFunc(
   const char* instancename, // Child instance id string
   Oxs_Director* dtr,        // App director
   const char* argstr);      // MIF input block parameters

// Child class registration block
struct Oxs_ExtRegistrationBlock {
public:
  String classid;  // Class name
  Oxs_ExtChildConstructFunc * construct;  // Constructor

  // Standard constructor.  Performs automatic registration
  // by calling Oxs_Ext::Register();
  Oxs_ExtRegistrationBlock(const char* name,
                           Oxs_ExtChildConstructFunc* ctr);

  // Default constructor, for use by STL "map" container.
  Oxs_ExtRegistrationBlock():classid(""),construct(NULL) {}
};

////////////////////////////////////////////////////////////////////////
// Macros to construct and downcast Oxs_Ext references.
// These macros can only be used from inside a child of Oxs_Ext.
// NOTE: We use Oc_Snprintf instead of string templates in the
// error handling because at least some versions of gcc (egcs 2.91.66
// on Linux/AXP) slurp up a lot of memory processing string template
// operations.
#define OXS_GET_EXT_OBJECT(params,subtype,obj)                        \
   { Oxs_OwnedPointer<Oxs_Ext> _goeo_tmp;                             \
     FindOrCreateObject(params,_goeo_tmp,#subtype);                   \
     if(_goeo_tmp.IsOwner()) {                                        \
        obj.SetAsOwner(dynamic_cast<subtype *>(_goeo_tmp.GetPtr()));  \
        _goeo_tmp.ReleaseOwnership();                                 \
     } else {                                                         \
        obj.SetAsNonOwner(dynamic_cast<subtype *>(_goeo_tmp.GetPtr())); \
     }                                                                \
     if(obj.GetPtr()==NULL) {                                         \
        char buf[4000];                                               \
        Oc_Snprintf(buf,sizeof(buf),                                  \
                    "Failed downcast: %.1000s is not of type %.1000s" \
                    " in source code file %.1000s, line %.100s.",     \
                    _goeo_tmp->InstanceName(),#subtype,               \
                    OC_STRINGIFY(__FILE__),                           \
                    OC_STRINGIFY(__LINE__));                          \
        throw Oxs_ExtError(this,buf);                                 \
     }                                                                \
   }

#define OXS_GET_INIT_EXT_OBJECT(label,subtype,obj)                    \
   { Oxs_OwnedPointer<Oxs_Ext> _goeo_tmp;                             \
     FindOrCreateInitObject(label,#subtype,_goeo_tmp);                \
     if(_goeo_tmp.IsOwner()) {                                        \
        obj.SetAsOwner(dynamic_cast<subtype *>(_goeo_tmp.GetPtr()));  \
        _goeo_tmp.ReleaseOwnership();                                 \
     } else {                                                         \
        obj.SetAsNonOwner(dynamic_cast<subtype *>(_goeo_tmp.GetPtr())); \
     }                                                                \
     if(obj.GetPtr()==NULL) {                                         \
        char buf[5000];                                               \
        Oc_Snprintf(buf,sizeof(buf),                                  \
                    "Error processing label \"%.1000s\""              \
                    " --- Failed downcast: %.1000s is not of type"    \
                    " %.1000s in source code file %.1000s,"           \
                    " line %.100s.",                                  \
                    label,_goeo_tmp->InstanceName(),#subtype,         \
                    OC_STRINGIFY(__FILE__),                           \
                    OC_STRINGIFY(__LINE__));                          \
        throw Oxs_ExtError(this,buf);                                 \
     }                                                                \
     DeleteInitValue(label);                                          \
   }


////////////////////////////////////////////////////////////////////////
// Oxs_Ext user outputs.  These are intended solely for use by the
// Oxs_Ext class.
struct OxsExtUserOutput {
public:
  Oxs_Director* director;
  Oxs_ChunkScalarOutput<Oxs_Ext> output;
  std::vector<OC_REAL8m> chunk_storage;
  String source_field_name;
  Oxs_VectorFieldOutputCache* source_field;
  vector<String> selector_init;
  Oxs_MeshValue<ThreeVector> selector;
  Oxs_MeshValue<ThreeVector> source_buffer; // Only used if source
  /// is not cacheable.  Note that if used, then memory space
  /// allocated in source_buffer remains reserved through lifetime
  /// of OxsExtUserOutput object.
  OC_BOOL source_cacheable;
  OC_BOOL normalize;
  OC_BOOL exclude_0_Ms;
  OC_REAL8m scaling; // For regular meshes (includes user_scaling)
  OC_REAL8m user_scaling;
  String name;
  String units;
  OC_BOOL units_specified;
  OxsExtUserOutput()
    : director(0), source_field(0), source_cacheable(0),
      normalize(1), exclude_0_Ms(0),
      scaling(0.), user_scaling(1.), units_specified(0) {}
  ~OxsExtUserOutput();
  void LookupSource(const char* instance_name); // Note:
  /// The director field must be set before calling this
  /// function.
private:
  // Disable copy constructor and assignment operators by
  // declaring them w/o providing definition.
  OxsExtUserOutput(const OxsExtUserOutput&);
  OxsExtUserOutput& operator=(const OxsExtUserOutput&);
};

////////////////////////////////////////////////////////////////////////
// Oxs_Ext base class
class Oxs_Ext : public Oxs_Lock {
  // Perhaps the inheritance from Oxs_Lock should be private,
  // but then exposing the Oxs_Lock member functions is awkward.
  friend struct Oxs_ExtRegistrationBlock; // Makes ::Register() calls.
private:
  // List (actually, "map")  of all registered classes.  The actual
  // map is stored as a function static variable of RegClasses().
  // This insures the map is initialized before it is used, which
  // is difficult to insure otherwise, because Oxs_Ext child classes
  // call Oxs_Ext::Register during static initialization.  (See
  // Section 9.4.1 of "The C++ Programming Lanuage," Bjarne Stroustrup,
  // 3rd edition for a discussion of nonlocal variable initialization
  // order.)
  static map<String,Oxs_ExtRegistrationBlock>& RegClasses();

  static void Register(const Oxs_ExtRegistrationBlock & regblk);

  const String instanceId;     // Object instance name

  // Disable copy constructor and assignment operators.  Don't
  // provide definitions for these!
  Oxs_Ext(const Oxs_Ext&);
  Oxs_Ext& operator=(const Oxs_Ext&);

  // Helper function for extended constructor (see below).
  // Throws exception if 'key' is already in init_strings;
  // otherwise adds key+value to init_strings.
  void AddNewInitStringPair(const String& key,const String& value);

  // User defined outputs
  Nb_ArrayWrapper<OxsExtUserOutput> user_output;
  void Fill__user_outputs_init(const Oxs_SimState&,int);
  void Fill__user_outputs(const Oxs_SimState&,OC_INDEX,OC_INDEX,int);
  void Fill__user_outputs_fini(const Oxs_SimState&,int);
  void Fill__user_outputs_shares(std::vector<Oxs_BaseChunkScalarOutput*>&);

protected:
  Oxs_Director* director;  // Controlling director object

  // Base Oxs_Ext constructor.  Call this from a child class if
  // the child wants to parse the MIF argstr on its own. The
  // extended Oxs_Ext constructor farther below is available to
  // provide automatic MIF argstr parsing.
  Oxs_Ext(const char* idname,Oxs_Director* dtr);

  // Helper routine to extract className from an instanceName
  static String ExtractClassName(const String& name);

  // Helper routine to extract instance tail from
  // instanceName = className + instanceTail.
  static String ExtractInstanceTail(const String& name);

  // Routine to check that instanceId and classId are consistent.
  // Throws an error if they aren't.  This is called inside
  // Oxs_Ext::MakeNew().
  OC_BOOL ValidateIdStrings() const;

  // Support code for child construction.  This code is activated
  // by the use of the constructor that has the extended
  // parameter list including 'const char* argstr'.  This
  // constructor requires that argstring be a Tcl list of pairs,
  // where the first element in each pair is an id string, and
  // the second is a value string.  The id string is mapped to
  // the key entry in init_strings, and the value string is
  // stored as the value corresponding to that key.  If the child
  // MIF class argstr does not fit this format, then use the
  // base, non-extended Oxs_Ext constructor.
  Oxs_Ext(const char* idname,Oxs_Director* dtr,const char* argstr);

  map<String,String> init_strings; // Initialization strings.

  void CheckInitValueParamCount(const String& key,OC_UINT4m count) const;
  /// Throws an exception if key is not in init_strings, or if the
  /// associated value is not a valid Tcl list, or if the number
  /// of parameters in the value is not exactly count.

  OC_BOOL HasInitValue(const String& key) const;
  /// Returns 1 if key is in init_strings, 0 otherwise.

  OC_BOOL FindInitValue(const String& key,String& value) const;
  OC_BOOL FindInitValue(const String& key,vector<String>& params) const;
  /// Returns the value associated with key, as either the original
  /// raw string or as a parameter list resulting from sending
  /// the value string through Tcl_SplitList.  The return value is
  /// 1 if key is in init_strings, 0 otherwise.  The params version
  /// will throw an exception if the value is not a valid Tcl list.
  ///   NB: Unlike the Get*InitValue routines, the Find*InitValue
  /// routines do not remove key+value from init_strings.  The
  /// caller must make an explicit call to DeleteInitValue to
  /// do this.

  void FindRequiredInitValue(const String& key,String& value) const;
  void FindRequiredInitValue(const String& key,
                             vector<String>& params) const;
  /// Same as FindInitValue routines, but throws an exception if key is
  /// not in init_strings.  The params version will throw an exception
  /// if the value is not a valid Tcl list.

  OC_BOOL DeleteInitValue(const String& key); // Removes the specified
  /// key+value pair from init_strings.  Returns 1 on success, 0
  /// if key was not found in init_strings.

  OC_UINT4m GetInitValueCount() {
    return static_cast<OC_UINT4m>(init_strings.size());
  }

  void VerifyAllInitArgsUsed(); // Throws an exception if
  /// GetInitValueCount() != 0.  Use this in combination with
  /// DeleteInitValue() to check MIF input block for superfluous
  /// parameters.  This should be called only in the actual
  /// instantiating class (not a parenting class).

  // The following functions are provided as a convenience to child
  // classes, because they occur frequently in practice.  They perform
  // a FindInitValue() call on key, and convert the result to the
  // appropriate type.  An exception is thrown if the lookup fails.
  // On success, the key+value is removed from init_strings (i.e.,
  // DeleteInitValue(key) is built into these functions).  The return
  // is by value, so no clean-up is required.
  OC_REAL8m GetRealInitValue(const String& key);
  ThreeVector GetThreeVectorInitValue(const String& key);
  OC_INT4m GetIntInitValue(const String& key);
  OC_UINT4m GetUIntInitValue(const String& key);
  String GetStringInitValue(const String& key);

  // The next group are duplicates of the above, except return
  // default_value instead of throwing an error if key is not
  // in init_strings.
  OC_REAL8m GetRealInitValue(const String& key,OC_REAL8m default_value);
  ThreeVector GetThreeVectorInitValue(const String& key,
                                      const ThreeVector& default_value);
  OC_INT4m GetIntInitValue(const String& key,OC_INT4m default_value);
  OC_UINT4m GetUIntInitValue(const String& key,OC_UINT4m default_value);
  String GetStringInitValue(const String& key,
                            const String& default_value);

  // Support for reading fixed-length lists (vectors)
  void GetRealVectorInitValue(const String& key,OC_UINT4m length,
                              vector<OC_REAL8m>& values);
  /// The value associated with key should be a list of "length"
  /// real values.  This function converts the value into a vector
  /// of OC_REAL8m's, stores the results in export values, and
  /// removes the key-value pair from init_strings.  Throws an
  /// exception if key in not in init_strings, of if any of the
  /// values cannot be completed converted to a OC_REAL8m, or if the
  /// parameter list associated with key does not have "length"
  /// entries.

  // Support functions for reading nested lists
  void GetGroupedStringListInitValue(const String& key,
                                     vector<String>& expanded_list);
  /// The value associated with key should be a grouped list of strings.
  /// This function loads the strings into the export vector<String>
  /// expanded_list, and removes the key-value pair from init_strings.
  /// An exception is thrown if key in not in init_strings, if the
  /// associated value does not contain at least one element, of if there
  /// is a structural problem with the grouped list.  If the associated
  /// value is a list of at least 2 elements, and if the last element is
  /// ":expand:", then that last element is pitched and the remainder is
  /// expanded as a grouped sublist.  Grouped sublists have the form
  /// {x1 x2 ... xn R} where x1 through xn are strings, and R is an
  /// integer repeat count.  The sublists are expanded recursively, so,
  /// i.e., xi can also be either an individual value or a grouped
  /// sublist of the same form.  As an example, the value "fred {barney
  /// {wilma betty 3} 2} :expand:" would be expanded into "fred barney
  /// wilma betty wilma betty wilma betty barney wilma betty wilma betty
  /// wilma betty".  Note that there is no group count expected or
  /// allowed at the top level, i.e., to get the expanded list "fred
  /// fred fred" an extra level of grouping is necessary, i.e.,
  /// "{fred 3} :expand:" is correct, not "fred 3 :expand:".
  ///   If a sublist element has embedded spaces, and the last bit can be
  /// interpreted as an integer, then the algorithm outlined above will
  /// cause the sublist to be improperly expanded.  For example, consider
  /// the flat list "{fred 3} {fred 3}".  This is *not* equivalent to the
  /// grouped list "{{fred 3} 2} :expand:", because as a result of
  /// recursive expansion, the latter expands into "fred fred fred fred
  /// fred fred".  To disable recursive expansion use the ":noexpand:"
  /// keyword in the repeat count location.  The proper grouped list
  /// representation of "{fred 3} {fred 3}" is
  ///             "{{{fred 3} :noexpand:} 2} :expand:"
  /// The ":noexpand:" keyword may also be used at the top-level to force
  /// a flat, i.e., unexpanded list.  As explained above, no-expansion is
  /// the default, but ":noexpand:" can be used to handle the situation
  /// where a flat list by chance has ":expand:" as the last element.  The
  /// ":noexpand:" keyword may be safely added to any flat list, which also
  /// protects against accidental eating of a chance ":noexpand:" last
  /// element in a flat list.  This is mainly useful in lists generated
  /// by code from unknown input.

  void GetGroupedRealListInitValue(const String& key,
                                   vector<OC_REAL8m>& expanded_list);
  void GetGroupedIntListInitValue(const String& key,
                                  vector<OC_INT4m>& expanded_list);
  void GetGroupedUIntListInitValue(const String& key,
                                   vector<OC_UINT4m>& expanded_list);
  /// Analogous to GetGroupedStringListInitValue(), these routines
  /// return vectors of OC_REAL8m's, OC_INT4m's and OC_UINT4m's, respectively.
  /// Exceptions are thrown for the same type of grouped list structural
  /// problems as GetGroupedStringListInitValue(), but in addition an
  /// exception is thrown if any element in the expanded list cannot be
  /// completely converted to the specified target type.

  void FindOrCreateObject(const vector<String>& param,
                          Oxs_OwnedPointer<Oxs_Ext>& obj,
                          const char* subtype=NULL) const;
  /// If param.size()==1, then queries the director for an already
  /// existing, registered Oxs_Ext object by the name param[0].
  /// In this case, the returned obj value points to the pre-existing
  /// object, and ownership is set false.  Otherwise, uses param to
  /// generate a new Oxs_Ext object, and obj points to that with
  /// ownership set true.  In this case it is the responsibility of the
  /// calling routine to see to the new object's eventual destruction.
  ///    If param.size()==1, but no pre-existing object is found, and
  /// if subtype == "Oxs_ScalarField", then an Oxs_UniformScalarField
  /// is created with param[0] as the initialization string.
  ///    If param.size()==2 then a new object is created of type
  /// param[0] with init string param[1].
  ///    If param.size()==3 and subtype == "Oxs_VectorField", then an
  /// Oxs_UniformVectorField is created with param's as the
  /// initialization string.
  ///    On failure, this routine will throw an exception.  In no case
  /// is the return obj value a NULL pointer.

  void FindOrCreateInitObject(const String& label,
                              const char* subtype,
                              Oxs_OwnedPointer<Oxs_Ext>& obj) const;
  /// A wrapper for FindRequiredInitValue + FindOrCreateObject, which
  /// includes some additional error handling.

public:

  // The Error, UnregisteredType, and ConstructFailure structs are
  // wrappers around Oxs_ExtError structs.  These wrappers provide
  // backwards compatibility for extension writers.  Their use is
  // deprecated; all new code should use the Oxs_ExtError structures
  // directly.
  struct Error : public Oxs_ExtError {
  public:
    Error(const char* errmsg) : Oxs_ExtError(errmsg) {}
    Error(const String& errmsg) : Oxs_ExtError(errmsg) {}
    Error(const Oxs_Ext* obj,const char* errmsg)
      : Oxs_ExtError(obj,errmsg) {}
    Error(const Oxs_Ext* obj,const String& errmsg)
      : Oxs_ExtError(obj,errmsg) {}
    virtual ~Error() {}
  };

  // A couple specializations for MakeNew().  We don't need Error(const
  // Oxs_Ext*, const char*) constructor support, because there is no
  // *this available inside static MakeNew().
  struct UnregisteredType : public Oxs_ExtErrorUnregisteredType {
    UnregisteredType(const char* errmsg)
      : Oxs_ExtErrorUnregisteredType(errmsg) {}
  };
  struct ConstructFailure : public Oxs_ExtErrorConstructFailure
  {
    ConstructFailure(const char* errmsg) :
      Oxs_ExtErrorConstructFailure(errmsg) {}
  };

  const char* InstanceName() const { return instanceId.c_str(); }
  virtual const char* ClassName() const =0;    // Child class name
  String DataName(const char* item) const {
    // Wrapper for use with calls to Oxs_SimState::Add/GetDerivedData()
    // and Oxs_SimState::Add/GetAuxData().
    return instanceId + String(":") + String(item);
  }
  
  virtual OC_BOOL Init();  // This can be called at any time to reset
  /// an Ext object to its initial, post constructor state.  The
  /// constructor should set all static object state information, and
  /// then call this function to initialize any state information that
  /// might change during the lifetime of the object.  It is incumbent
  /// upon the constructor to save whatever information is necessary for
  /// Init() to completely reset itself.  Return value of 1 indicates
  /// success, 0 indicates error.  Perhaps this should be changed to
  /// void return, with an exception thrown on error?
  ///
  /// NB: Any child that overrides the default Oxs_Ext::Init()
  ///     function should embed a call to Oxs_Ext::Init() for
  ///     proper initialization of general Oxs_Ext members (in
  ///     particular, user outputs).

  static Oxs_Ext* MakeNew(const char* idstring, // Child object name
                          Oxs_Director* dtr,   // App director
                          const char* argstr); // MIF input block params
  /// NOTE 1: The safe interpreter is guaranteed valid throughout the
  /// life of the Oxs_Ext object.  This is the same interpreter used
  /// by the Oxs_Mif object to source the input MIF file.  MakeNew()
  /// throws an Oxs_Ext::UnregisteredType exception if idstring
  /// requests an unknown class type, or an Oxs_Ext::ConstructFailure
  /// exception if object construction failed.  The latter is most
  /// likely due to bad argstr.  Otherwise, the return value is
  /// guaranteed to be non-NULL, and to point to a object of the
  /// specified type.  It is the responsibility of the client to delete
  /// the object when finished with it.

  // The following static function is wrapped by member function
  // FindOrCreateObject.  The basic functionality is pulled into this
  // separate routine to make it available outside of Oxs_Ext objects.
  //   On success, fills import obj.  On error, throws a String
  // containing an error message.
  static void
  FindOrCreateObjectHelper(Oxs_Director* dtr,
                           const vector<String>& params,
                           Oxs_OwnedPointer<Oxs_Ext>& obj,
                           const char* subtype);

  static String ListRegisteredChildClasses(); // This is intended
  /// for debugging purposes only.

  virtual void StageRequestCount(unsigned int& min,
                                 unsigned int& max) const;
  // Number of stages object wants.  UINT_MAX indicates infinity.
  // If min is larger than max, then an error may be raised by the
  // driver, depending on the value of its parameter stage_count_check.
  // If checking is disabled, then the number of stages run will
  // be the largest "min" value returned by all StageRequestCount
  // calls, so in the case of non-fatal stage count inconsistencies
  // inside an Oxs_Ext count, the min value should be set to the best
  // stage count guess.  In this case, "max" doesn't really matter,
  // as long as it is less than min.
  //  The default implementation returns (0,UINT_MAX), which means
  // no restriction.

  virtual ~Oxs_Ext();
};


#endif // _OXS_EXT
