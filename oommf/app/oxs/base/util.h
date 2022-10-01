/* FILE: util.h                 -*-Mode: c++-*-
 *
 * Small utility classes.
 *
 * Classes and support routines declared in this file:
 *
 *   void Oxs_ToUpper(String& str);
 *   void Oxs_ToLower(String& str);
 *   String Oxs_QuoteListElement(const char* import);
 *   class Oxs_TclInterpState;
 *   struct Oxs_FilePointer;
 *   class Oxs_Box;
 *   template<class T> class Oxs_WriteOnceObject;
 *   template<class T> class Oxs_OwnedPointer;
 *   class Oxs_MultiType
 */

#ifndef _OXS_UTIL
#define _OXS_UTIL

#include <cstdio>
#include <string>
#include <vector>
#include "oc.h"
#include "nb.h" // This is needed in util.cc.  We include it here
/// to get it into the dependency tree for overaggressive compilers
/// that when loading util.h automatically go looking into util.cc
/// too, presumably for template expansion.  Which compilers?  I'm
/// not naming names, but their initials are s, g and i.

#include "threevector.h"
#include "oxsexcept.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported
OC_USE_STRING;

/* End includes */


////////////////////////////////////////////////////////////////////////
// Couple of routines to change case of characters inside an STL string.
// Useful for case insensitive comparisons.
void Oxs_ToUpper(String& str);
void Oxs_ToLower(String& str);

////////////////////////////////////////////////////////////////////////
// Wrapper for Tcl_ConvertElement().
String Oxs_QuoteListElement(const char* import);

////////////////////////////////////////////////////////////////////////
// Class to save and restore Tcl interpreter state,
// including error information.  Saved results can be either
// restored or discarded, once and only once.  If a state
// is being held at the time of object destruction, Discard()
// is automatically called.
class Oxs_TclInterpState {
private:
  Tcl_Interp* interp; // ==NULL iff no state is currently held
#if (TCL_MAJOR_VERSION > 8) || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION>4)
  Tcl_InterpState state;
#else
  Tcl_SavedResult result;
  OC_BOOL error_info_set;
  String error_info;
  OC_BOOL error_code_set;
  String error_code;
#endif
public:
  Oxs_TclInterpState()
    : interp(NULL)
#if (TCL_MAJOR_VERSION > 8) || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION>4)
#else
, error_info_set(0), error_code_set(0)
#endif
  {}
  Oxs_TclInterpState(Tcl_Interp* import_interp)
    : interp(NULL)
#if (TCL_MAJOR_VERSION > 8) || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION>4)
#else
, error_info_set(0), error_code_set(0)
#endif
  {
    Save(import_interp);
  }
  ~Oxs_TclInterpState();
  void Save(Tcl_Interp* import_interp);
  void Restore();
  void Discard();
  OC_BOOL IsSaved() { return (interp ? 1 : 0); }
};


////////////////////////////////////////////////////////////////////////
// Wrapper for standard C stream file pointer.  Constructor opens
// file, destructor closes it.
struct Oxs_FilePointer
{
public:
  Oxs_FilePointer(const char* filename,const char* filemode);
  ~Oxs_FilePointer();
  FILE* fptr;
};

////////////////////////////////////////////////////////////////////////
// Simple box class.  Used for defining axes parallel rectangular
// parallelepipeds.
class Oxs_Box {
  OC_REAL8m x0,x1,y0,y1,z0,z1;  // Box corner coordinates.
public:
  Oxs_Box() : x0(0.),x1(-1.),y0(0.),y1(-1.),z0(0.),z1(-1.) {}

  void Set(OC_REAL8m x0p,OC_REAL8m x1p,OC_REAL8m y0p,OC_REAL8m y1p,
	   OC_REAL8m z0p,OC_REAL8m z1p);
  /// This Set function insures that x0<=x1, y0<=y1, z0<=z1.

  void Set(const String& sx0,const String& sx1,
	   const String& sy0,const String& sy1,
	   const String& sz0,const String& sz1);
  /// This Set function insures that x0<=x1, y0<=y1, z0<=z1.

  OC_BOOL CheckOrderSet(OC_REAL8m xmin,OC_REAL8m xmax,
		     OC_REAL8m ymin,OC_REAL8m ymax,
		     OC_REAL8m zmin,OC_REAL8m zmax);
  /// Checks that [xyz]min <= [xyz]max.  Returns 0 if not,
  /// otherwise sets [xyz][01] and returns 1.

  // An empty box is identified by the condition x0>x1.
  void MakeEmpty() {
    x0 = y0 = z0 =  0.0;
    x1 = y1 = z1 = -1.0;
  }

  OC_BOOL IsEmpty() const { return (x0>x1); }

  OC_BOOL Expand(const Oxs_Box& other); // Expands *this as
  /// necessary to contain region specified by "other".
  /// Returns 1 if *this was enlarged, 0 if other was
  /// already contained in *this.

  OC_BOOL Expand(OC_REAL8m x,OC_REAL8m y,OC_REAL8m z); // Similar
  /// to Expand(const Oxs_Box&), but with a single
  /// point import instead of a box.  Returns 1 if *this
  /// was enlarged, 0 if (x,y,z) was already contained
  /// in *this.

  OC_BOOL Intersect(const Oxs_Box& other); // Shrinks *this
  /// as necessary so that the resulting box is the
  /// intersection of the original *this with other.

  OC_BOOL IsIn(const ThreeVector& point) const;
  OC_BOOL IsContained(const Oxs_Box& other) const;

  OC_REAL8m GetMinX() const { return x0; }
  OC_REAL8m GetMaxX() const { return x1; }
  OC_REAL8m GetMinY() const { return y0; }
  OC_REAL8m GetMaxY() const { return y1; }
  OC_REAL8m GetMinZ() const { return z0; }
  OC_REAL8m GetMaxZ() const { return z1; }

  OC_REAL8m GetVolume() const {
    if(IsEmpty()) return 0.0;
    return (x1-x0)*(y1-y0)*(z1-z0);
  }
};
// The next 3 operators are defined so MSVC++ 5.0 will accept
// vector<Oxs_Box>, but are left undefined because they aren't
// needed, and in general make no sense.
OC_BOOL operator<(const Oxs_Box&,const Oxs_Box&);
OC_BOOL operator>(const Oxs_Box&,const Oxs_Box&);
OC_BOOL operator==(const Oxs_Box&,const Oxs_Box&);

////////////////////////////////////////////////////////////////////////
// Write Once Object (WOO) template.  Can be used with any type that
// supports operator=().  Throws an exception if a read occurs on an
// unset object, or if a write occurs onto an already set object.
// These type is not intended for use in high traffic paths.  If such
// a need arises, then perhaps the access checks should be #ifdef'd out
// (perhaps with NDEBUG?) for production builds.
//  UPDATE 5-Apr-2002: New class idea ---
//    Problems: I want to be able to store arbitrary types, with fast
//  lookup and memory reuse.
//    Answer: Use a 3-class solution.  First, a WOO class holding
//  an auto_ptr to a generic wrapper class.  Children of the wrapper
//  class are templated.  The director then holds an STL vector of WOO's.
//  Clients register with the director each WOO they want stored in
//  the state.  When a state is created, the WOO vector will be filled
//  out with the indicated objects, with NULL auto_ptr's.  So every state
//  will hold slots for each WOO, and in the same order.  The return from
//  the registration call is an index to the corresponding position in
//  the WOO vector.
//     Each WOO also holds the "name" of the object being held, which is
//  the same across all states.  Clients can do a lookup on the name,
//  and get back the index for future fast access.
//     A const WOO object which is not "set" will allow clients to grab
//  its auto_ptr, and will accept a new auto_ptr.  But once a new auto_ptr
//  is accepted, any future attempt to grab that auto_ptr or write a new
//  one will result in an exception.  The non-const interface allows this
//  without exception (or exceptions :^).
//     When a state is reused, all the WOO objects are marked as unset.
//  Memory is not released, though, so by grabbing the auto_ptr the
//  associated memory may be reused.  Hmm, the wrapper class should
//  have a virtual "reset" method that re-initializes the object.
//     Note 1: "grabbing" an auto_ptr, i.e., getting a non-const ptr,
//  is different from "reading" the auto_ptr, i.e., getting a const ptr.
//  The former raises an error if attempted on a set, const WOO object.
//  The latter raises an error if attempted on an unset object.  Also,
// "grabbing" unsets the WOO object.
//     Note 2: I've had problems using auto_ptr in the past due to
//  busted STL auto_ptr implementations.  Since auto_ptr is a simple
//  enough class, perhaps we should just go ahead and implement our
//  own stand-alone Oc_AutoPtr class?
//     Note 3: Maybe all state-based output should be moved into the
//  state?  Perhaps at some level of the 3-class WOO should include
//  all the machinery currently in the Oxs_Output objects, such as
//  update function pointer and output registration?  It might also
//  be useful to store output scheduling information here too, so that
//  when an output update function is called all other outputs that
//  will be requested for that state are calculated and cached(?) too?
//  Or maybe rather than caching, we store where the destinations of the
//  data, and send it straight out?
template<class T> class Oxs_WriteOnceObject
{
private:
  OC_BOOL set;
  T value;
public:
  Oxs_WriteOnceObject() : set(0) {}
  OC_BOOL IsSet() const { return set; }
  void Set(const T& newvalue) {
    if(set) {
      OXS_THROW(Oxs_ProgramLogicError,
		"WriteOnceObject<T>::Set(): Object already set");
    }
    value = newvalue;
    set = 1;
  }
  const T& Get() const {
    if(!set) {
      OXS_THROW(Oxs_ProgramLogicError,
		"WriteOnceObject<T>::Get(): Object not set");
    }
    return value;
  }
  const Oxs_WriteOnceObject<T>& operator=(const T& newvalue) {
    Set(newvalue);
    return *this;
  }
  operator const T&() const { return Get(); }
};


////////////////////////////////////////////////////////////////////////
// Owned pointer class.  A smart pointer with mediocre intelligence.
// New work should use instead one of the C++11 smart pointer types.
template<class T> class Oxs_OwnedPointer
{
private:
  OC_BOOL owner;
  T* ptr;

  // Disable all copy operators by declaring them but not providing
  // a definition.  This makes OwnedPointers more awkward to use,
  // but less likely to be abused.
  Oxs_OwnedPointer(const Oxs_OwnedPointer<T>&);
  Oxs_OwnedPointer<T>& operator=(const Oxs_OwnedPointer<T>&);
public:
  Oxs_OwnedPointer() : owner(0), ptr(0) {}
  ~Oxs_OwnedPointer() { if(owner) delete ptr; }

  // Initialization
  void SetAsOwner(T* nptr) {
    if(owner) delete ptr;
    if((ptr=nptr)!=0) owner=1;
    else              owner=0; // Don't own NULL pointer
  }
  void SetAsNonOwner(T* nptr) {
    if(owner) delete ptr;
    owner=0;
    ptr=nptr;
  }
  void Free() { SetAsNonOwner(NULL); }

  // Dereferencing ops
  T* GetPtr() const { return ptr; }
  T* operator->() const { return ptr; }
  T& operator*() const { return *ptr; }

  // Ownership managment
  OC_BOOL IsOwner() const { return owner; }

  void ReleaseOwnership() { owner=0; }
  /// Note difference with Free(): ReleaseOwnership()
  /// does *not* delete to pointed to object, or even
  /// stop pointing to it.

  void ClaimOwnership() {
    if(ptr==NULL) {
      OXS_THROW(Oxs_BadPointer,
		"Oxs_OwnedPointer<T>::ClaimOwnership():"
		" Can't take ownership of a NULL pointer.");
    }
    owner=1;
  }

};

////////////////////////////////////////////////////////////////////////
// Class wrapper around a union type that can be used for arrays
// of heterogeneous objects.  Extend types as needed.
class Oxs_MultiType
{
public:
  enum VarType { TYPE_UNSET, TYPE_INTEGER, TYPE_UNSIGNED, TYPE_REAL };
  Oxs_MultiType() : var_type(TYPE_UNSET) {}
  Oxs_MultiType(OC_INT4 ival)
    : var_type(TYPE_INTEGER), value(static_cast<OC_INT8>(ival)) {}
  Oxs_MultiType(OC_INT8 ival)
    : var_type(TYPE_INTEGER), value(ival) {}
  Oxs_MultiType(OC_UINT4 ival)
    : var_type(TYPE_UNSIGNED), value(static_cast<OC_UINT8>(ival)) {}
  Oxs_MultiType(OC_UINT8 ival)
    : var_type(TYPE_UNSIGNED), value(ival) {}
  Oxs_MultiType(OC_REAL4 ival)
    : var_type(TYPE_REAL), value(static_cast<OC_REALWIDE>(ival)) {}
  Oxs_MultiType(OC_REAL8 ival)
    : var_type(TYPE_REAL), value(static_cast<OC_REALWIDE>(ival)) {}
#if !OC_REALWIDE_IS_OC_REAL4 && !OC_REALWIDE_IS_OC_REAL8
  Oxs_MultiType(OC_REALWIDE ival)
    : var_type(TYPE_REAL), value(ival) {}
#endif
  
  // Type query
  VarType GetType() const { return var_type; }
  
  // Set value operators
  const OC_INT8& operator=(const OC_INT4& other) {
    *this = static_cast<OC_INT8>(other);
    return value.integer;
  }
  const OC_INT8& operator=(const OC_INT8& other) {
    var_type = TYPE_INTEGER;
    value.integer = other;
    return value.integer;
  }
  const OC_UINT8& operator=(const OC_UINT4& other) {
    *this = static_cast<OC_UINT8>(other);
    return value.unsigned_integer;
  }
  const OC_UINT8& operator=(const OC_UINT8& other) {
    var_type = TYPE_UNSIGNED;
    value.unsigned_integer = other;
    return value.unsigned_integer;
  }
  const OC_REALWIDE& operator=(const OC_REALWIDE& other) {
    var_type = TYPE_REAL;
    value.real = other;
    return value.real;
  }

  // Fill operators
  void Fill(OC_INT4& other) {
    // NOTE: No overflow check
    OC_INT8 tmp;
    Fill(tmp);
    other = static_cast<OC_INT4>(tmp);
  }
  void Fill(OC_INT8& other) {
    if(var_type != TYPE_INTEGER) {
      String msg = "Type mismatch in Oxs_MultiType::Fill(OC_INT8);"
        " Oxs_MultiType value is type ";
      switch(var_type) {
      case TYPE_INTEGER:  msg += " TYPE_INTEGER";  break;
      case TYPE_UNSIGNED: msg += " TYPE_UNSIGNED"; break;
      case TYPE_REAL:     msg += " TYPE_REAL";     break;
      default:            msg += " TYPE_UNSET";    break;
      }
      OXS_THROW(Oxs_BadParameter,msg);
    }
    other = value.integer;
  }

  void Fill(OC_UINT4& other) {
    // NOTE: No overflow check
    OC_UINT8 tmp;
    Fill(tmp);
    other = static_cast<OC_UINT4>(tmp);
  }
  void Fill(OC_UINT8& other) {
    if(var_type != TYPE_UNSIGNED) {
      String msg = "Type mismatch in Oxs_MultiType::Fill(OC_UINT8);"
        " Oxs_MultiType value is type ";
      switch(var_type) {
      case TYPE_INTEGER:  msg += " TYPE_INTEGER";  break;
      case TYPE_UNSIGNED: msg += " TYPE_UNSIGNED"; break;
      case TYPE_REAL:     msg += " TYPE_REAL";     break;
      default:            msg += " TYPE_UNSET";    break;
      }
      OXS_THROW(Oxs_BadParameter,msg);
    }
    other = value.unsigned_integer;
  }

  void Fill(OC_REAL4& other) {
    // NOTE: No overflow check
    OC_REALWIDE tmp;
    Fill(tmp);
    other = static_cast<OC_REAL4>(tmp);
  }
#if !OC_REALWIDE_IS_OC_REAL8
  void Fill(OC_REAL8& other) {
    // NOTE: No overflow check
    OC_REALWIDE tmp;
    Fill(tmp);
    other = static_cast<OC_REAL8>(tmp);
  }
#endif
  void Fill(OC_REALWIDE& other) {
    if(var_type != TYPE_REAL) {
      String msg = "Type mismatch in Oxs_MultiType::Fill(OC_REALWIDE);"
        " Oxs_MultiType value is type ";
      switch(var_type) {
      case TYPE_INTEGER:  msg += " TYPE_INTEGER";  break;
      case TYPE_UNSIGNED: msg += " TYPE_UNSIGNED"; break;
      case TYPE_REAL:     msg += " TYPE_REAL";     break;
      default:            msg += " TYPE_UNSET";    break;
      }
      OXS_THROW(Oxs_BadParameter,msg);
    }
    other = value.real;
  }

  // Fill operation that converts value to a string.
  // NB: The std::to_string(float/double/long double) function converts
  //     using %f rather than %g.  Unclear who thinks that is a good
  //     idea.  (Although clearly floating point is a poor stepchild.)
  //     Regardless and even so, we want to write with full precision,
  //     so use Nb_FloatToString().
  void Fill(String& other) {
    switch(var_type) {
    case TYPE_INTEGER:   other = std::to_string(value.integer); break;
    case TYPE_UNSIGNED:  other = std::to_string(value.unsigned_integer); break;
    case TYPE_REAL:
#if OC_REALWIDE_WIDTH == 8
      // Nb_FloatToString will append an "L" suffix to long double
      // output.  If OC_REALWIDE is a long double, but has the same
      // width as a double, then that L is unnecessary.  (Double and
      // long double are treated as distinct types by C++, even if they
      // are same at the binary level.)  This is the situation with
      // Visual C++ on Windows, for example.  This #if branch will
      // drop the L suffix if REAL8 is type double.
      other = Nb_FloatToString(static_cast<OC_REAL8>(value.real));
#else
      other = Nb_FloatToString(value.real);
#endif
      break;
    default:             other=""; break;
    }
  }

private:
  VarType var_type;
  union VALUE {
    OC_INT8 integer;
    OC_UINT8 unsigned_integer;
    OC_REALWIDE real;
    VALUE() {}
    VALUE(OC_INT8 ival)     : integer(ival) {}
    VALUE(OC_UINT8 ival)    : unsigned_integer(ival) {}
    VALUE(OC_REALWIDE ival) : real(ival) {}
  } value;

};

#endif // _OXS_UTIL
