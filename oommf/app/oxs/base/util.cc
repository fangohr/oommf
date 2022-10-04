/* FILE: util.cc                 -*-Mode: c++-*-
 *
 * Small utility classes
 *
 */

#include <cstring>
#include <cctype>

#include "oc.h"
#include "nb.h"
#include "threevector.h"
#include "util.h"

/* End includes */

////////////////////////////////////////////////////////////////////////
// Couple of routines to change case of characters inside an STL string.
// Useful for case insensitive comparisons.
void Oxs_ToUpper(String& str)
{
  String::iterator p = str.begin();
  while(p!=str.end()) {
    *p = static_cast<char>(toupper(*p));
    ++p;
  }
}

void Oxs_ToLower(String& str)
{
  String::iterator p = str.begin();
  while(p!=str.end()) {
    *p = static_cast<char>(tolower(*p));
    ++p;
  }
}

////////////////////////////////////////////////////////////////////////
// Wrapper for Tcl_ConvertElement().
String Oxs_QuoteListElement(const char* import)
{
  String result;
  if(import==NULL || import[0]=='\0') {
    return result; // Return empty string
  }
  int flags,worksize;
  Oc_AutoBuf inbuf,outbuf;
  inbuf.Dup(import);
  worksize = Tcl_ScanElement(inbuf,&flags);
  outbuf.SetLength(worksize);
  Tcl_ConvertElement(inbuf,outbuf,flags);
  result = outbuf.GetStr();
  return result;
}

////////////////////////////////////////////////////////////////////////
// Class to save and restore Tcl interpreter state,
// including error information
void Oxs_TclInterpState::Discard()
{
  if(interp==NULL) {
    OXS_THROW(Oxs_BadResourceDealloc,
	      "Oxs_TclInterpState::Discard(): No state to discard");
  }
#if (TCL_MAJOR_VERSION > 8) || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION>4)
  Tcl_DiscardInterpState(state);
#else
  Tcl_DiscardResult(&result);
  error_info_set = 0; error_info.erase();
  error_code_set = 0; error_code.erase();
#endif
  interp=NULL;
}

void Oxs_TclInterpState::Restore()
{
  if(interp==NULL) {
    OXS_THROW(Oxs_BadResourceDealloc,
	      "Oxs_TclInterpState::Restore(): No state to restore");
  }
#if (TCL_MAJOR_VERSION > 8) || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION>4)
  Tcl_RestoreInterpState(interp, state);
#else
  Tcl_RestoreResult(interp,&result);
  if(error_info_set) {
    const char *p = Tcl_GetStringResult(interp);

    if (error_info.find(p, 0) == 0) {
      String tail = error_info.substr(strlen(p));

      Tcl_AddErrorInfo(interp, OC_CONST84_CHAR( tail.c_str() ));
    } else {
      Tcl_AddErrorInfo(interp, OC_CONST84_CHAR(error_info.c_str()));
    }
  }
  if(error_code_set) {
    Tcl_SetErrorCode(interp, OC_CONST84_CHAR(error_code.c_str()), NULL);
  }
  error_info_set = 0; error_info.erase();
  error_code_set = 0; error_code.erase();
#endif
  interp=NULL;
}

Oxs_TclInterpState::~Oxs_TclInterpState()
{
  if(interp!=NULL) Discard();
}

void Oxs_TclInterpState::Save(Tcl_Interp* import_interp)
{
  if(interp!=NULL) Discard();
  interp = import_interp;
#if (TCL_MAJOR_VERSION > 8) || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION>4)
  state = Tcl_SaveInterpState(interp, TCL_OK);
#else
  Tcl_SaveResult(interp,&result);
  const char* ei = Tcl_GetVar(interp,OC_CONST84_CHAR("errorInfo"),
                              TCL_GLOBAL_ONLY);
  if(ei!=NULL) {
    error_info_set = 1;
    error_info = String(ei);
  } else { // Safety
    error_info_set = 0;
    error_info.erase();
  }
  const char* ec = Tcl_GetVar(interp,OC_CONST84_CHAR("errorCode"),
                              TCL_GLOBAL_ONLY);
  if(ec!=NULL) {
    error_code_set = 1;
    error_code = String(ec);
  } else { // Safety
    error_code_set = 0;
    error_code.erase();
  }
#endif
}

////////////////////////////////////////////////////////////////////////
// Oxs_FilePointer
Oxs_FilePointer::Oxs_FilePointer
(const char* filename,const char* filemode)
{
  fptr = Nb_FOpen(filename,filemode);
  // Throw exception here if fptr == NULL ?
}

Oxs_FilePointer::~Oxs_FilePointer()
{
  if(fptr!=NULL) fclose(fptr);
}

////////////////////////////////////////////////////////////////////////
// Oxs_Box
void Oxs_Box::Set(OC_REAL8m x0p,OC_REAL8m x1p,
		  OC_REAL8m y0p,OC_REAL8m y1p,
		  OC_REAL8m z0p,OC_REAL8m z1p)
{ // This Set function insures that x0<=x1, y0<=y1, z0<=z1.
  if(x0p<=x1p) { x0=x0p; x1=x1p; }
  else         { x0=x1p; x1=x0p; }
  if(y0p<=y1p) { y0=y0p; y1=y1p; }
  else         { y0=y1p; y1=y0p; }
  if(z0p<=z1p) { z0=z0p; z1=z1p; }
  else         { z0=z1p; z1=z0p; }
}

void Oxs_Box::Set(const String& sx0,const String& sx1,
		  const String& sy0,const String& sy1,
		  const String& sz0,const String& sz1)
{ // This Set function insures that x0<=x1, y0<=y1, z0<=z1.
  OC_REAL8m x0p,x1p,y0p,y1p,z0p,z1p;
  x0p=Nb_Atof(sx0.c_str());
  x1p=Nb_Atof(sx1.c_str());
  y0p=Nb_Atof(sy0.c_str());
  y1p=Nb_Atof(sy1.c_str());
  z0p=Nb_Atof(sz0.c_str());
  z1p=Nb_Atof(sz1.c_str());
  Set(x0p,x1p,y0p,y1p,z0p,z1p);
}

OC_BOOL
Oxs_Box::CheckOrderSet(OC_REAL8m xmin,OC_REAL8m xmax,
		       OC_REAL8m ymin,OC_REAL8m ymax,
		       OC_REAL8m zmin,OC_REAL8m zmax)
{ // Checks that [xyz]min <= [xyz]max.  Returns 0 if not,
  // otherwise sets [xyz][01] and returns 1.
  if(xmax<xmin || ymax<ymin || zmax<zmin) return 0;
  x0=xmin;   x1=xmax;
  y0=ymin;   y1=ymax;
  z0=zmin;   z1=zmax;
  return 1;
}

OC_BOOL Oxs_Box::Expand(const Oxs_Box& other)
{ // Expands *this as necessary to contain region specified by "other".
  // Returns 1 if *this was enlarged, 0 if other was already contained in
  // *this.
  if(other.IsEmpty()) return 0; // "other" is an empty box
  if(IsEmpty()) {
    // *this is an empty box
    *this = other;
    return 1; // We only get to this clause if other
    /// is not empty.
  }
  OC_BOOL expanded=0;
  if(other.x0<x0) { x0 = other.x0; expanded=1; }
  if(other.y0<y0) { y0 = other.y0; expanded=1; }
  if(other.z0<z0) { z0 = other.z0; expanded=1; }
  if(other.x1>x1) { x1 = other.x1; expanded=1; }
  if(other.y1>y1) { y1 = other.y1; expanded=1; }
  if(other.z1>z1) { z1 = other.z1; expanded=1; }
  return expanded;
}

OC_BOOL Oxs_Box::Expand(OC_REAL8m x,OC_REAL8m y,OC_REAL8m z)
{ // Similar to Expand(const Oxs_Box&), but with a single point import
  // instead of a box.  Returns 1 if *this was enlarged, 0 if (x,y,z)
  // was already contained in *this.
  if(IsEmpty()) {
    // *this is an empty box
    x0=x1=x;
    y0=y1=y;
    z0=z1=z;
    return 1;
  }
  OC_BOOL expanded=0;
  if(x<x0) { x0 = x; expanded=1; }
  if(y<y0) { y0 = y; expanded=1; }
  if(z<z0) { z0 = z; expanded=1; }
  if(x>x1) { x1 = x; expanded=1; }
  if(y>y1) { y1 = y; expanded=1; }
  if(z>z1) { z1 = z; expanded=1; }
  return expanded;
}

OC_BOOL Oxs_Box::Intersect(const Oxs_Box& other)
{ // Shrinks *this as necessary so that the resulting box
  // is the intersection of the original *this with other.
  // Returns 1 if *this is shrunk, 0 if *this is already
  // contained inside other.
  if(IsEmpty()) return 0; // *this is already empty
  if(other.IsEmpty()) { // "other" is an empty box
    MakeEmpty();
    return 1; // We only get to this clause if *this
    /// was not empty coming in.
  }
  OC_BOOL shrunk=0;
  if(other.x0>x0) { x0 = other.x0; shrunk=1; }
  if(other.y0>y0) { y0 = other.y0; shrunk=1; }
  if(other.z0>z0) { z0 = other.z0; shrunk=1; }
  if(other.x1<x1) { x1 = other.x1; shrunk=1; }
  if(other.y1<y1) { y1 = other.y1; shrunk=1; }
  if(other.z1<z1) { z1 = other.z1; shrunk=1; }
  return shrunk;
}

OC_BOOL Oxs_Box::IsIn(const ThreeVector& point) const
{
  if(point.x<x0 || point.x>x1 ||
     point.y<y0 || point.y>y1 ||
     point.z<z0 || point.z>z1) return 0;
  return 1;
}

OC_BOOL Oxs_Box::IsContained(const Oxs_Box& other) const
{
  if(other.x0>other.x1) return 1; // "other" is an empty box
  if(x0>x1) return 0; // *this is an empty box
  if(other.x0<x0 || other.x1>x1 ||
     other.y0<y0 || other.y1>y1 ||
     other.z0<z0 || other.z1>z1) return 0;
  return 1;
}
