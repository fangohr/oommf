/* FILE: tclobjarray.h          -*-Mode: c++-*-
 *
 * Wrapper for Tcl_Obj objects.  Inert if building against Tcl 8.0
 * or earlier.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011/10/07 23:23:08 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_TCLOBJARRAY
#define _NB_TCLOBJARRAY

#include <string>
#include "oc.h"

OC_USE_STRING;         // Map String --> std::string

/* End includes */

#ifndef NB_TCLOBJARRAY_AVAILABLE
# if (TCL_MAJOR_VERSION > 8 || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION>0))
#  define NB_TCLOBJARRAY_AVAILABLE 1
# else
#  define NB_TCLOBJARRAY_AVAILABLE 0
# endif
#endif

#if NB_TCLOBJARRAY_AVAILABLE
////////////////////////////////////////////////////////////////////////
// Wrapper for Tcl_Obj**
typedef Tcl_Obj* NbTclObjPtr;
class Nb_TclObjArray
{
private:
  int size;
  Tcl_Obj** arr;
  void Alloc(int arrsize);
  void Free();
public:
  Nb_TclObjArray() : size(0), arr(NULL) {}
  Nb_TclObjArray(int arrsize) : size(0), arr(NULL) { Alloc(arrsize); }
  ~Nb_TclObjArray() { Free(); }

  void Resize(int newsize) {
    if(newsize != size) {
      Free(); Alloc(newsize);
    }
  }

  NbTclObjPtr& operator[](int index) {
#ifndef NDEBUG
    if(index>=size || index<0) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_TclObjArray",
                            "operator[]",1024,
                            "Array out-of-bounds; index=%d, size=%d",
                            index,size));
    }
#endif
    return arr[index];
  }

  const NbTclObjPtr& operator[](int index) const {
#ifndef NDEBUG
    if(index>=size || index<0) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_TclObjArray",
                            "operator[] const",1024,
                            "Array out-of-bounds; index=%d, size=%d",
                            index,size));
    }
#endif
    return arr[index];
  }

  int Size() const { return size; }
  Tcl_Obj** Array() { return arr; }
  Tcl_Obj* const * Array() const { return arr; }

  // Write interface routines.  These handle the 
  // copy-on-write semantics of the Tcl referencing
  // system.
  void WriteString(int index,const char*);
  void WriteString(int index,const String& str);
  void WriteDouble(int index,double val);
  void WriteInt(int index,int val);
  void WriteLong(int index,long val);
  void WriteList(int index,const Nb_TclObjArray& val);
  /// Add more as needed

  // Read interface routines.
  String GetString(int index) const;

};
#endif // NB_TCLOBJARRAY_AVAILABLE


#endif // _NB_TCLOBJARRAY
