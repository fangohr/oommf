/* FILE: tclobjarray.cc          -*-Mode: c++-*-
 *
 * Wrapper for Tcl_Obj objects.  Inert if building against Tcl 8.0
 * or earlier.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011/07/07 05:04:59 $
 * Last modified by: $Author: donahue $
 */


#include <cstring>
#include "tclobjarray.h"

/* End includes */

#if NB_TCLOBJARRAY_AVAILABLE
////////////////////////////////////////////////////////////////////////
// Wrapper for Tcl_Obj**

#define NB_TCLOBJARRAY_ERRBUFSIZE 1024

void Nb_TclObjArray::Free()
{
  if(size>0) {
    for(int i=0;i<size;i++) {
      Tcl_DecrRefCount(arr[i]);
    }
    delete[] arr;
  }
  size=0;
  arr=NULL;
}

void Nb_TclObjArray::Alloc(int arrsize)
{
  if(arrsize<0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_TclObjArray","Alloc",
                          NB_TCLOBJARRAY_ERRBUFSIZE,
                          "Invalid arrsize=%d (must be non-negative)",
                          arrsize));
  }
  if(size>0) Free();
  if(arrsize==0) {
    size=0;
    arr=NULL;
  } else {
    arr = new Tcl_Obj*[arrsize];
    size=arrsize;
    for(int i=0;i<size;i++) {
      arr[i] = Tcl_NewObj();
      Tcl_IncrRefCount(arr[i]);
    }
  }
}

// Write interface routines.  These handle the 
// copy-on-write semantics of the Tcl referencing
// system.  We can included additional routines
// when needed.
void
Nb_TclObjArray::WriteString(int index,const char* cptr)
{
#ifndef NDEBUG
  if(index>=size || index<0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_TclObjArray",
                          "WriteString",NB_TCLOBJARRAY_ERRBUFSIZE+2000,
                          "Array out-of-bounds; index=%d, string=%.2000s",
                          index,cptr));
  }
#endif
  Tcl_Obj* obj = arr[index];
  if (Tcl_IsShared(obj)) {
    Tcl_Obj* tmp = obj;
    arr[index] = obj = Tcl_DuplicateObj(obj); // Create copy to write on
    Tcl_IncrRefCount(obj);
    Tcl_DecrRefCount(tmp);
  }
  Tcl_SetStringObj(obj,OC_CONST84_CHAR(cptr),
		   static_cast<int>(strlen(cptr)));
}

void
Nb_TclObjArray::WriteString(int index,const String& str)
{
  WriteString(index,str.c_str());
}

void
Nb_TclObjArray::WriteDouble(int index,double val)
{
#ifndef NDEBUG
  if(index>=size || index<0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_TclObjArray",
                          "WriteDouble",NB_TCLOBJARRAY_ERRBUFSIZE,
                          "Array out-of-bounds; index=%d, double=%g",
                          index,val));
  }
#endif
  Tcl_Obj* obj = arr[index];
  if (Tcl_IsShared(obj)) {
    Tcl_Obj* tmp = obj;
    arr[index] = obj = Tcl_DuplicateObj(obj); // Create copy to write on
    Tcl_IncrRefCount(obj);
    Tcl_DecrRefCount(tmp);
  }
  Tcl_SetDoubleObj(obj,val);
}

void
Nb_TclObjArray::WriteInt(int index,int val)
{
#ifndef NDEBUG
  if(index>=size || index<0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_TclObjArray",
                          "WriteInt",NB_TCLOBJARRAY_ERRBUFSIZE,
                          "Array out-of-bounds; index=%d, int=%d",
                          index,val));
  }
#endif
  Tcl_Obj* obj = arr[index];
  if (Tcl_IsShared(obj)) {
    Tcl_Obj* tmp = obj;
    arr[index] = obj = Tcl_DuplicateObj(obj); // Create copy to write on
    Tcl_IncrRefCount(obj);
    Tcl_DecrRefCount(tmp);
  }
  Tcl_SetIntObj(obj,val);
}

void
Nb_TclObjArray::WriteLong(int index,long val)
{
#ifndef NDEBUG
  if(index>=size || index<0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_TclObjArray",
                          "WriteLong",NB_TCLOBJARRAY_ERRBUFSIZE,
                          "Array out-of-bounds; index=%d, long=%ld",
                          index,val));
  }
#endif
  Tcl_Obj* obj = arr[index];
  if (Tcl_IsShared(obj)) {
    Tcl_Obj* tmp = obj;
    arr[index] = obj = Tcl_DuplicateObj(obj); // Create copy to write on
    Tcl_IncrRefCount(obj);
    Tcl_DecrRefCount(tmp);
  }
  Tcl_SetLongObj(obj,val);
}

void
Nb_TclObjArray::WriteList(int index,const Nb_TclObjArray& val)
{
#ifndef NDEBUG
  if(index>=size || index<0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_TclObjArray",
                          "WriteInt",NB_TCLOBJARRAY_ERRBUFSIZE,
                          "Array out-of-bounds; index=%d, <list val>",
                          index));
  }
#endif
  Tcl_Obj* obj = arr[index];
  if (Tcl_IsShared(obj)) {
    Tcl_Obj* tmp = obj;
    arr[index] = obj = Tcl_DuplicateObj(obj); // Create copy to write on
    Tcl_IncrRefCount(obj);
    Tcl_DecrRefCount(tmp);
  }
  Tcl_SetListObj(obj,val.size,val.arr);
}

String
Nb_TclObjArray::GetString(int index) const
{
#ifndef NDEBUG
  if(index>=size || index<0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_TclObjArray",
                          "GetString",NB_TCLOBJARRAY_ERRBUFSIZE,
                          "Array out-of-bounds; index=%d",
                          index));
  }
#endif
  return String(Tcl_GetString(arr[index]));
}
#endif // NB_TCLOBJARRAY_AVAILABLE
