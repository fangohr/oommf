/* FILE: mesh.cc                -*-Mode: c++-*-
 *
 * Abstract mesh class and children
 *
 * Last modified on: $Date: 2016/02/24 20:34:19 $
 * Last modified by: $Author: donahue $
 */

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "oc.h"
#include "nb.h"

#include "fileio.h"
#include "mesh.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported.
/// For some compilers this is needed to get "long double"
/// versions of the basic math library functions.

/* End includes */     // This is an optional directive to build.tcl,
                       // that excludes the remainder of the file from
                       // '#include ".*"' dependency building


#define VF_MESH_ERRBUFSIZE 1024

//////////////////////////////////////////////////////////////////////////
// Abstract Mesh class
void Vf_Mesh::SetFilename(const char* _Filename) {
  Filename.Dup(_Filename);
}
void Vf_Mesh::SetTitle(const char* _Title) {
  Title.Dup(_Title);
}
void Vf_Mesh::SetDescription(const char* _Description) {
  Description.Dup(_Description);
}
void Vf_Mesh::SetMeshUnit(const char* _MeshUnit) {
  MeshUnit.Dup(_MeshUnit);
}
void Vf_Mesh::SetValueUnit(const char* _ValueUnit) {
  ValueUnit.Dup(_ValueUnit);
}
const char* Vf_Mesh::GetName() const {
  return Filename.GetStr();
}
const char* Vf_Mesh::GetTitle() const {
  return Title.GetStr();
}
const char* Vf_Mesh::GetDescription() const {
  return Description.GetStr();
}
const char* Vf_Mesh::GetMeshUnit() const {
  return MeshUnit.GetStr();
}
const char* Vf_Mesh::GetValueUnit() const {
  return ValueUnit.GetStr();
}

void Vf_Mesh::GetRange(Nb_BoundingBox<OC_REAL4> &range) const
{
  Nb_BoundingBox<OC_REAL8> wrange;
  GetPreciseRange(wrange);
  Convert(wrange,range);
}

void Vf_Mesh::GetDataRange(Nb_BoundingBox<OC_REAL4> &range) const
{
  Nb_BoundingBox<OC_REAL8> wrange;
  GetPreciseDataRange(wrange);
  Convert(wrange,range);
}

void
Vf_Mesh::GetBoundaryList(Nb_List< Nb_Vec3<OC_REAL4> > &boundary_list) const
{
  Nb_List< Nb_Vec3<OC_REAL8> > precise_boundary;
  GetPreciseBoundaryList(precise_boundary);
  Nb_Vec3<OC_REAL4> newpt;
  const Nb_Vec3<OC_REAL8> *vptr;
  Nb_List_Index< Nb_Vec3<OC_REAL8> > key;
  boundary_list.Clear();
  for(vptr=precise_boundary.GetFirst(key);
      vptr!=NULL;
      vptr=precise_boundary.GetNext(key)) {
    newpt.Set(static_cast<OC_REAL4>(vptr->x),
              static_cast<OC_REAL4>(vptr->y),
              static_cast<OC_REAL4>(vptr->z));
    boundary_list.Append(newpt);
  }
}

void
Vf_Mesh::FillDataBoundaryList
(Nb_List< Nb_Vec3<OC_REAL8> > &bdry) const
{ // Uses GetPreciseRange to produce a rectangular boundary
  // about the data.
  Nb_BoundingBox<OC_REAL8> range;
  GetPreciseRange(range);
  Nb_Vec3<OC_REAL8> minpt,maxpt;
  range.GetExtremes(minpt,maxpt);
  OC_REAL8 zave=(minpt.z+maxpt.z)/2.0;
  minpt.z=maxpt.z=zave;
  bdry.SetFirst(minpt);
  bdry.Append(Nb_Vec3<OC_REAL8>(minpt.x,maxpt.y,zave));
  bdry.Append(maxpt);
  bdry.Append(Nb_Vec3<OC_REAL8>(maxpt.x,minpt.y,zave));
  bdry.Append(minpt);
}

void Vf_Mesh::GetZslice(OC_REAL8m zlow,OC_REAL8m zhigh,
                        OC_REAL4m& zslicelow,OC_REAL4m& zslicehigh)
{ // Calls (virtual) islice-version of GetZslice, but converts
  // from integer back to mesh coordinates.  Useful for emulating
  // slice-based display in non-slice-based code.

  OC_INDEX islicelow,islicehigh,islicecount;
  GetZslice(zlow,zhigh,islicelow,islicehigh);
  islicecount=GetZsliceCount();

  OC_REAL8m zstart,zstep;
  GetZsliceParameters(zstart,zstep);  // NB: zstep may be negative

  OC_REAL8m zend = zstart + zstep*islicecount;

  if(islicecount<=1) {
    OC_REAL8m slop = (fabs(zstart)+fabs(zend))*OC_REAL4_EPSILON;
    if(slop==0.0) slop=OC_REAL4_EPSILON;
    if(zstep>0.0) {
      zslicelow  = static_cast<OC_REAL4m>(zstart - slop);
      zslicehigh = static_cast<OC_REAL4m>(zend + slop);
    } else {
      zslicelow  = static_cast<OC_REAL4m>(zend - slop);
      zslicehigh = static_cast<OC_REAL4m>(zstart + slop);
    }
    return;
  }

  // Compute limits, shifted "down" by one half step
  OC_REAL8m za = zstart + (islicelow-0.5)*zstep;
  OC_REAL8m zb = zstart + (islicehigh-0.5)*zstep;

  // Shift range a little to guarantee inclusion of lower limit and
  // exclude upper limit (the islice interface return includes islicelow
  // but excludes islicehigh), and cast to OC_REAL4m.  Strictly speaking,
  // the shifting shouldn't really be necessary because the preceding
  // slice limit code sets the edges midway between the actual cell
  // sample levels.
  if(zstep>0.0) {
    zslicelow  = static_cast<OC_REAL4m>(za - 1*OC_REAL4_EPSILON*zstep);
    zslicehigh = static_cast<OC_REAL4m>(zb - 1*OC_REAL4_EPSILON*zstep);
  } else {
    zslicelow  = static_cast<OC_REAL4m>(zb - 1*OC_REAL4_EPSILON*zstep);
    zslicehigh = static_cast<OC_REAL4m>(za - 1*OC_REAL4_EPSILON*zstep);
  }
}


OC_BOOL Vf_Mesh::FindClosest(const Nb_Vec3<OC_REAL4> &pos,
                             Nb_LocatedVector<OC_REAL4> &lv)
{ // Low-precision version.
  Nb_Vec3<OC_REAL8> spos;
  Nb_LocatedVector<OC_REAL8> slv;
  Convert(pos,spos);
  OC_BOOL code=FindPreciseClosest(spos,slv);
  Convert(slv.location,lv.location);
  Convert(slv.value,lv.value);
  return code;
}

// Determine smallest and largest magnitude across all vectors in mesh.
// This does *not* include DisplayValueScale scaling, but does include
// ValueMultiplier.
void
Vf_Mesh::GetValueMagSpan(Nb_LocatedVector<OC_REAL8>& min_export,
                         Nb_LocatedVector<OC_REAL8>& max_export) const
{
  Vf_Mesh_Index *key=NULL;
  Nb_LocatedVector<OC_REAL8> vec,min_vec,max_vec;
  const OC_REAL8m sqrt3 = sqrt(OC_REAL8m(3.0));
  const OC_REAL8m sqrt1_3 = 1.0/sqrt3;
  OC_REAL8m max_a = 0.0, max_b = 0.0;  // Scaling to protect against
  OC_REAL8m min_a = 0.0, min_b = 0.0;  // over- and underflow
  // A min or max value is represented by two values, a scale value "a"
  // and an adjustment "b", computed as follows:
  //
  //    Given (x,y,z) != 0.0, let a = max(|x|,|y|,|z|).  Then set
  //    b = ((x/a)^2 + (y/a)^2 + (z/a)^2).  Note that
  //
  //    1 <= b <= 3
  //    |(x,y,z)| = a*sqrt(b)
  //    a <= |(x,y,z)| <= a*sqrt(3)
  //
  // Now, to test if say |(u,v,w)| is larger than |(x,y,z)| we compute
  // ta = max(|u|,|v|,|w|) and tb = ((u/ta)^2 + (v/ta)^2 + (w/ta)^2),
  // and compare:
  //
  //    if sqrt(3)*ta < a  then |(u,v,w)| is smaller than |(x,y,z)|
  //    if sqrt(3)*a  < ta then |(u,v,w)| is bigger than |(x,y,z)|
  //    else compare (a/ta)^2*b < tb iff |(u,v,w)| is bigger than |(x,y,z)|
  //
  // To find minimums, just run the tests the other way around.

  if(GetFirstPt(key,vec)) {
    OC_REAL8m tx=abs(vec.value.x);
    OC_REAL8m ty=abs(vec.value.y);
    OC_REAL8m tz=abs(vec.value.z);
    if(tx<ty) { OC_REAL8m tmp=tx; tx=ty; ty=tmp; }
    if(tx<tz) { OC_REAL8m tmp=tx; tx=tz; tz=tmp; }
    max_a = min_a = tx;
    if(tx>0.0) {
      ty /= tx;  tz /= tx;
      max_b = min_b = 1 + ty*ty + tz*tz;
    }
    min_vec = max_vec = vec;
    while(GetNextPt(key,vec)) {
      tx=abs(vec.value.x); ty=abs(vec.value.y); tz=abs(vec.value.z);
      if(tx<ty) { OC_REAL8m tmp=tx; tx=ty; ty=tmp; }
      if(tx<tz) { OC_REAL8m tmp=tx; tx=tz; tz=tmp; }
      if(tx > sqrt1_3 * max_a) { // vec may be new max
        ty /= tx;  tz /= tx;
        OC_REAL8m tb = 1 + ty*ty + tz*tz;
        if(tx*sqrt1_3 >= max_a) {
          max_a = tx;  max_b = tb;
          max_vec = vec;
        } else {
          OC_REAL8m tr = max_a/tx;
          if(tr*tr*max_b < tb) {
            max_a = tx;  max_b = tb;
            max_vec = vec;
          }
        }
      }
      if(tx*sqrt1_3 < max_a) { // vec may be new min
        if(tx == 0.0) {
          min_a = min_b = 0.0;
        } else {
          ty /= tx;  tz /= tx;
          OC_REAL8m tb = 1 + ty*ty + tz*tz;
          if(tx <= sqrt1_3*min_a) {
            min_a = tx;  min_b = tb;
            min_vec = vec;
          } else {
            OC_REAL8m tr = min_a/tx;
            if(tr*tr*min_b > tb) {
              min_a = tx;  min_b = tb;
              min_vec = vec;
            }
          }
        }
      }
    }
  }
  delete key;

  // If min and max values are very nearly equal, then rounding error
  // can result in |min_vec| being ever so slightly larger than
  // |max_vec|.  This can cause problems in downstream code which
  // assumes |max_vec| >= |min_vec|.  So check and swap if necessary.
  // (Of course, rounding error also means that there may be vecs with
  // |vec| outside of the min/max range, i.e. |vec|<|min_vec| or
  // |max_vec|<|vec|, but this is less easily discoverable and so less
  // likely to cause problems.

  // Scaling hack, assumes IEEE 8-byte double.  Should work in
  // practice.  We want powers of two to avoid rounding errors.
  // Note that 3*(testsize*testsize) < OC_REAL8m_MAX, and
  // 3*(OC_REAL8_MAX*testscale)^2 < OC_REAL8m_MAX.
  const OC_REAL8 testsize  = 6.703903964971299e+153; // 2^511
  const OC_REAL8 testscale = 3.729170365600103e-155; // 1/2^513

  OC_REAL8m scale = ( max_a>testsize ? testscale : 1.0 );

  OC_REAL8m sx = min_vec.value.x*scale;
  OC_REAL8m sy = min_vec.value.y*scale;
  OC_REAL8m sz = min_vec.value.z*scale;
  OC_REAL8m smagsq = sx*sx + sy*sy + sz*sz;

  OC_REAL8m tx = max_vec.value.x*scale;
  OC_REAL8m ty = max_vec.value.y*scale;
  OC_REAL8m tz = max_vec.value.z*scale;
  OC_REAL8m tmagsq = tx*tx + ty*ty + tz*tz;

  if(smagsq > tmagsq) { // Swap
    vec = min_vec;  min_vec = max_vec;  max_vec = vec;
  }

  min_vec.value *= ValueMultiplier;  min_export = min_vec;
  max_vec.value *= ValueMultiplier;  max_export = max_vec;
}

void
Vf_Mesh::GetValueMagSpan(OC_REAL8m& min_export,OC_REAL8m& max_export) const
{
  Nb_LocatedVector<OC_REAL8> min_vec,max_vec;
  GetValueMagSpan(min_vec,max_vec);
  min_export = min_vec.value.Mag();
  max_export = max_vec.value.Mag();
}

// Determine smallest and largest magnitude across all vectors in mesh,
// *excluding* zero vectors. This does *not* include DisplayValueScale
// scaling, but does include ValueMultiplier.
void
Vf_Mesh::GetNonZeroValueMagSpan(Nb_LocatedVector<OC_REAL8>& min_export,
                                Nb_LocatedVector<OC_REAL8>& max_export) const
{
  Vf_Mesh_Index *key=NULL;
  Nb_LocatedVector<OC_REAL8> vec,min_vec,max_vec;
  const OC_REAL8m sqrt3 = sqrt(3.0);
  const OC_REAL8m sqrt1_3 = 1.0/sqrt(3.0);
  OC_REAL8m max_a = -1.0, max_b = 0.0;  // Scaling to protect against
  OC_REAL8m min_a = -1.0, min_b = 0.0;  // over- and underflow
  // A min or max value is represented by two values, a scale value "a"
  // and an adjustment "b", computed as follows:
  //
  //    Given (x,y,z) != 0.0, let a = max(|x|,|y|,|z|).  Then set
  //    b = ((x/a)^2 + (y/a)^2 + (z/a)^2).  Note that
  //
  //    1 <= b <= 3
  //    |(x,y,z)| = a*sqrt(b)
  //    a <= |(x,y,z) <= a*sqrt(3)
  //
  // Now, to test if say |(u,v,w)| is larger than |(x,y,z)| we compute
  // ta = max(|u|,|v|,|w|) and tb = ((u/ta)^2 + (v/ta)^2 + (w/ta)^2),
  // and compare:
  //
  //    if sqrt(3)*ta < a  then |(u,v,w)| is smaller than |(x,y,z)|
  //    if sqrt(3)*a  < ta then |(u,v,w)| is bigger than |(x,y,z)|
  //    else compare (a/ta)^2*b < tb iff |(u,v,w)| is bigger than |(x,y,z)|
  //
  // To find minimums, just run the tests the other way around.

  if(GetFirstPt(key,vec)) {
    OC_REAL8m tx=abs(vec.value.x);
    OC_REAL8m ty=abs(vec.value.y);
    OC_REAL8m tz=abs(vec.value.z);
    if(tx<ty) { OC_REAL8m tmp=tx; tx=ty; ty=tmp; }
    if(tx<tz) { OC_REAL8m tmp=tx; tx=tz; tz=tmp; }
    if(tx>0.0) {
      max_a = min_a = tx;
      ty /= tx;  tz /= tx;
      max_b = min_b = 1 + ty*ty + tz*tz;
    }
    min_vec = max_vec = vec;
    while(max_a<0.0 && GetNextPt(key,vec)) {
      tx=abs(vec.value.x); ty=abs(vec.value.y); tz=abs(vec.value.z);
      if(tx<ty) { OC_REAL8m tmp=tx; tx=ty; ty=tmp; }
      if(tx<tz) { OC_REAL8m tmp=tx; tx=tz; tz=tmp; }
      if(tx>0.0) {
        max_a = min_a = tx;
        ty /= tx;  tz /= tx;
        max_b = min_b = 1 + ty*ty + tz*tz;
        min_vec = max_vec = vec;
      }
    }
    while(GetNextPt(key,vec)) {
      tx=abs(vec.value.x); ty=abs(vec.value.y); tz=abs(vec.value.z);
      if(tx<ty) { OC_REAL8m tmp=tx; tx=ty; ty=tmp; }
      if(tx<tz) { OC_REAL8m tmp=tx; tx=tz; tz=tmp; }
      if(tx == 0.0) continue;
      if(tx > sqrt1_3 * max_a) { // vec may be new max
        ty /= tx;  tz /= tx;
        OC_REAL8m tb = 1 + ty*ty + tz*tz;
        if(tx >= sqrt3*max_a) {
          max_a = tx;  max_b = tb;
          max_vec = vec;
        } else {
          OC_REAL8m tr = max_a/tx;
          if(tr*tr*max_b < tb) {
            max_a = tx;  max_b = tb;
            max_vec = vec;
          }
        }
      }
      if(tx < sqrt3 * max_a) { // vec may be new min
        ty /= tx;  tz /= tx;
        OC_REAL8m tb = 1 + ty*ty + tz*tz;
        if(tx <= sqrt1_3*min_a) {
          min_a = tx;  min_b = tb;
          min_vec = vec;
        } else {
          OC_REAL8m tr = min_a/tx;
          if(tr*tr*min_b > tb) {
            min_a = tx;  min_b = tb;
            min_vec = vec;
          }
        }
      }
    }
  }
  delete key;
  min_vec.value *= ValueMultiplier;  min_export = min_vec;
  max_vec.value *= ValueMultiplier;  max_export = max_vec;
}

void
Vf_Mesh::GetNonZeroValueMagSpan(OC_REAL8m& min_export,OC_REAL8m& max_export) const
{
  Nb_LocatedVector<OC_REAL8> min_vec,max_vec;
  GetNonZeroValueMagSpan(min_vec,max_vec);
  min_export = min_vec.value.Mag();
  max_export = max_vec.value.Mag();
}

// Compute vector mean value.  Each node is weighted equally.  The
// value does *not* include DisplayValueScale scaling, but does
// include ValueMultiplier.
void
Vf_Mesh::GetValueMean(Nb_Vec3<OC_REAL8>& meanvalue) const
{
  meanvalue.Set(0,0,0);
  OC_REAL8m count = static_cast<OC_REAL8m>(GetSize());
  if(count>0) {
    Vf_Mesh_Index *key=NULL;
    Nb_LocatedVector<OC_REAL8> vec;
    if(GetFirstPt(key,vec)) {
      meanvalue += vec.value;
      while(GetNextPt(key,vec)) {
        meanvalue += vec.value;
      }
    }
    delete key;
    OC_REAL8m mult = ValueMultiplier/count;
    meanvalue.x *= mult;
    meanvalue.y *= mult;
    meanvalue.z *= mult;
  }
}

// Compute the Root mean square (RMS) of the norm of the node vectors.
// Each node is weighted equally.  The value does *not* include
// DisplayValueScale scaling, but does include ValueMultiplier.
OC_REAL8m
Vf_Mesh::GetValueRMS() const
{
  OC_REAL8m result = 0.0;
  OC_REAL8m count = static_cast<OC_REAL8m>(GetSize());
  if(count>0) {
    Vf_Mesh_Index *key=NULL;
    Nb_LocatedVector<OC_REAL8> vec;
    OC_REAL8m sumsq=0.0;
    if(GetFirstPt(key,vec)) {
      sumsq += vec.value.MagSq();
      while(GetNextPt(key,vec)) {
        sumsq += vec.value.MagSq();
      }
    }
    delete key;
    result = fabs(ValueMultiplier)*sqrt(sumsq/count);
  }
  return result;
}

// Computes \sum_i (|x_i| + |y_i| + |z_i|)/N, where i=1..N.
// Each node is weighted equally.  The value does *not* include
// DisplayValueScale scaling, but does include ValueMultiplier.
OC_REAL8m
Vf_Mesh::GetValueL1() const
{
  OC_REAL8m result = 0.0;
  OC_REAL8m count = static_cast<OC_REAL8m>(GetSize());
  if(count>0) {
    Vf_Mesh_Index *key=NULL;
    Nb_LocatedVector<OC_REAL8> vec;
    OC_REAL8m sum=0.0;
    if(GetFirstPt(key,vec)) {
      sum += fabs(vec.value.x) + fabs(vec.value.y) + fabs(vec.value.z);
      while(GetNextPt(key,vec)) {
        sum += fabs(vec.value.x) + fabs(vec.value.y) + fabs(vec.value.z);
      }
    }
    delete key;
    sum /= count;
    result = fabs(ValueMultiplier)*sum;
  }
  return result;
}



// Subtract values of one mesh from the other.
OC_BOOL Vf_Mesh::SubtractMesh(const Vf_Mesh& other)
{
  if(GetSize() != other.GetSize()) {
    return 1; // Meshes aren't the same size
  }

  const OC_REAL8 scale = other.GetValueMultiplier()/GetValueMultiplier();

  Vf_Mesh_Index *key1=NULL,*key2=NULL;
  Nb_LocatedVector<OC_REAL8> vec1,vec2;

  if( GetFirstPt(key1,vec1) && other.GetFirstPt(key2,vec2) ) {
    do {
      vec1.value -= (scale*vec2.value);
      SetNodeValue(key1,vec1.value);
    } while( GetNextPt(key1,vec1) && other.GetNextPt(key2,vec2) );
  }

  delete key1;
  delete key2;

  return 0;
}

// Compute pointwise vector (cross) product of two meshes,
// storing the results in the first mesh, i.e.,
//  *this ^= other
OC_BOOL Vf_Mesh::CrossProductMesh(const Vf_Mesh& other)
{
  if(GetSize() != other.GetSize()) {
    return 1; // Meshes aren't the same size
  }

  const OC_REAL8 scale = other.GetValueMultiplier()/GetValueMultiplier();

  Vf_Mesh_Index *key1=NULL,*key2=NULL;
  Nb_LocatedVector<OC_REAL8> vec1,vec2;

  if( GetFirstPt(key1,vec1) && other.GetFirstPt(key2,vec2) ) {
    do {
      vec1.value ^= (scale*vec2.value);
      SetNodeValue(key1,vec1.value);
    } while( GetNextPt(key1,vec1) && other.GetNextPt(key2,vec2) );
  }

  delete key1;
  delete key2;

  return 0;
}

//////////////////////////////////////////////////////////////////////////
// Helper function for Vf_Mesh child ::ColorQuantityTransforms(), which
// meets the needs for the standard color quantities.
static OC_BOOL ColorQuantityTransformHelper
(const Nb_DString flipstr,
 const Nb_DString& quantity_in,OC_REAL8m phase_in,OC_BOOL invert_in,
 Nb_DString& quantity_out,OC_REAL8m& phase_out,OC_BOOL& invert_out)
{
  // Break flipstr into components
  const char* parsestr=" \t\n:";
  char* flipbuf = new char[size_t(flipstr.Length())+1];
  strcpy(flipbuf,flipstr.GetStr());
  char* nexttoken=flipbuf;
  char *token1 = Nb_StrSep(&nexttoken,parsestr);
  char *token2 = Nb_StrSep(&nexttoken,parsestr);
  char *token3 = Nb_StrSep(&nexttoken,parsestr);
  if(token1[0]=='\0' || strlen(token1)>2 ||
     token2[0]=='\0' || strlen(token2)>2 ||
     token3[0]=='\0' || strlen(token3)>2) { // Partial check
    delete[] flipbuf;
    OC_THROW(Oc_Exception(__FILE__,__LINE__,NULL,
                          "ColorQuantityTransformHelper",
                          VF_MESH_ERRBUFSIZE+200,
                          "Bad flipstr parameter: %.200s",
                          flipstr.GetStr()));
  }

  // Transform
  quantity_out = quantity_in;
  phase_out = phase_in;
  invert_out = invert_in;
  OC_BOOL change = 1;
  if(strcmp(quantity_in.GetStr(),"x")==0) {
    quantity_out[0]=token1[strlen(token1)-1];
    if(token1[0]=='-') invert_out = !invert_out;
  } else if(strcmp(quantity_in.GetStr(),"y")==0) {
    quantity_out[0]=token2[strlen(token2)-1];
    if(token2[0]=='-') invert_out = !invert_out;
  } else if(strcmp(quantity_in.GetStr(),"z")==0) {
    quantity_out[0]=token3[strlen(token3)-1];
    if(token3[0]=='-') invert_out = !invert_out;
  } else if(strcmp(quantity_in.GetStr(),"xy-angle")==0) {
    char a1=token1[strlen(token1)-1];
    char a2=token2[strlen(token2)-1];
    if(a2=='x' || a1=='z') {
      char t=a1; a1=a2; a2=t;
      phase_out-=0.25;
      invert_out = !invert_out;
    }
    quantity_out[0]=a1;  // Make sure coords are in
    quantity_out[1]=a2;  // canonical order.
    if(token1[0]=='-') phase_out += 0.5;
    if(token2[0]=='-') invert_out = !invert_out;
    if(token3[0]=='-') invert_out = !invert_out;
  } else if(strcmp(quantity_in.GetStr(),"xz-angle")==0) {
    char a1=token1[strlen(token1)-1];
    char a2=token3[strlen(token3)-1];
    if(a2=='x' || a1=='z') {
      char t=a1; a1=a2; a2=t;
      phase_out-=0.25;
      invert_out = !invert_out;
    }
    quantity_out[0]=a1;  // Make sure coords are in
    quantity_out[1]=a2;  // canonical order.
    if(token1[0]=='-') phase_out += 0.5;
    if(token3[0]=='-') invert_out = !invert_out;
    if(token2[0]=='-') invert_out = !invert_out;
  } else if(strcmp(quantity_in.GetStr(),"yz-angle")==0) {
    char a1=token2[strlen(token2)-1];
    char a2=token3[strlen(token3)-1];
    if(a2=='x' || a1=='z') {
      char t=a1; a1=a2; a2=t;
      phase_out-=0.25;
      invert_out = !invert_out;
    }
    quantity_out[0]=a1;  // Make sure coords are in
    quantity_out[1]=a2;  // canonical order.
    if(token2[0]=='-') phase_out += 0.5;
    if(token3[0]=='-') invert_out = !invert_out;
    if(token1[0]=='-') invert_out = !invert_out;
  } else {
    // Otherwise, no change
    change = 0;
  }
  delete[] flipbuf;
  return change;
}

//////////////////////////////////////////////////////////////////////////
// Regular 3D grid of Nb_Vec3<OC_REAL8>'s
const ClassDoc Vf_GridVec3f::class_doc("Vf_GridVec3f",
                    "Michael J. Donahue (michael.donahue@nist.gov)",
                    "1.0.0","11-Dec-1996");

// The following constructor should be called from file input functions.
Vf_GridVec3f::Vf_GridVec3f(const char* filename,
                           const char* title,const char* desc,
                           const char* meshunit,
                           const char* valueunit,OC_REAL8 valuemultiplier,
                           OC_INDEX newi,OC_INDEX newj,OC_INDEX newk,
                           const Nb_Vec3<OC_REAL8> &basept,
                           const Nb_Vec3<OC_REAL8> &gridstep,
                           const Nb_BoundingBox<OC_REAL8> &newdatarange,
                           const Nb_BoundingBox<OC_REAL8> &newrange,
                           const Nb_List< Nb_Vec3<OC_REAL8> > *newboundary)
  :Vf_Mesh(filename,title,desc,meshunit,valueunit,valuemultiplier)
{
  SetSize(newi,newj,newk);
  coords_base=basept;
  coords_step=gridstep;
  if(coords_step.x==0.) coords_step.x=1.;
  if(coords_step.y==0.) coords_step.y=1.;
  if(coords_step.z==0.) coords_step.z=1.;
  data_range=newdatarange;
  range=newrange;
  if(newboundary!=NULL) {
    boundary_from_data=0;
    boundary = *newboundary;
  } else {
    boundary_from_data=1;
    FillDataBoundaryList(boundary);
  }
}

// Problem <-> Nodal Coordinates Conversion Routines
// The conversion from nodes to problem coordinates is
//    (i,j,k) -> base+(i*step.x,j*step.y,k*step.z).
// NOTE 1: The grid vector access functions, e.g., operator(), access
//   via the integral "nodal" coordinates, *not* in terms of the
//   (real valued) problem coordinates.
// NOTE 2: The values in coords_step are guaranteed to be !=0,
//   but may be negative!  However, GetStdStep() (below), returns
//   a value >0.
// NOTE 3: The coords conversion routines don't preserve order
//   if step.? is negative, e.g., say (min.x,min.y) < (max.x,max.y)
//   going in, but step.x is negative.  Then after conversion,
//   min.y~ will still be < max.y~, but now min.x~>max.x~.

Nb_Vec3<OC_REAL8>
Vf_GridVec3f::NodalToProblemCoords(Nb_Vec3<OC_REAL8> const &w_nodal)
  const
{
  Nb_Vec3<OC_REAL8> v(w_nodal);
  v.x*=coords_step.x;
  v.y*=coords_step.y;
  v.z*=coords_step.z;
  v+=coords_base;
  return v;
}

Nb_Vec3<OC_REAL8>
Vf_GridVec3f::ProblemToNodalCoords(Nb_Vec3<OC_REAL8> const &v_problem)
  const
{
  Nb_Vec3<OC_REAL8> w(v_problem);
  w-=coords_base;
  w.x/=coords_step.x;
  w.y/=coords_step.y;
  w.z/=coords_step.z;
  return w;
}

void
Vf_GridVec3f::GetPreciseRange(Nb_BoundingBox<OC_REAL8> &myrange) const
{
  myrange=range;
}

void
Vf_GridVec3f::GetPreciseDataRange(Nb_BoundingBox<OC_REAL8> &myrange) const
{
  myrange=data_range;
}

OC_REAL8m Vf_GridVec3f::GetStdStep() const
{ // Returns smallest step among those coordinate directions
  // with dimensions of at least 2.
  OC_INDEX isize,jsize,ksize;
  GetDimens(isize,jsize,ksize);
  OC_REAL8 minstep = DBL_MAX;
  if(isize>1) minstep = fabs(coords_step.x);
  if(jsize>1 && minstep>fabs(coords_step.y)) {
    minstep=fabs(coords_step.y);
  }
  if(ksize>1 && minstep>fabs(coords_step.z)) {
    minstep=fabs(coords_step.z);
  }
  if(isize<2 && jsize<2 && ksize<2) {
    // Special handling for 1 cell meshes
    minstep = OC_MIN(fabs(coords_step.x),
                     OC_MIN(fabs(coords_step.y),fabs(coords_step.z)));
  }
  // Although this routine returns a OC_REAL8m, the result is frequently
  // cast to a OC_REAL4m, so protect in that case
  if(minstep<=8*FLT_MIN || minstep>=FLT_MAX/8) {
    minstep = 1.0; // Safety
  }
  return static_cast<OC_REAL8m>(minstep);
}

OC_INDEX Vf_GridVec3f::ColorQuantityTypes(Nb_List<Nb_DString> &types) const
{
  types.Clear();
  types.Append(Nb_DString("x"));
  types.Append(Nb_DString("y"));
  types.Append(Nb_DString("z"));
  types.Append(Nb_DString("slice")); // Note: For historical reasons
  /// this is quantity is called ZSLICE in the code, but the name
  /// exported to the user interface is now just "slice", to more
  /// accurately describe the quantity in the case were the user has
  /// applied an axis transformation.
  types.Append(Nb_DString("mag"));
  types.Append(Nb_DString("xy-angle"));
  types.Append(Nb_DString("xz-angle"));
  types.Append(Nb_DString("yz-angle"));
  types.Append(Nb_DString("div"));
  types.Append(Nb_DString("none"));
  return types.GetSize();
}

OC_BOOL Vf_GridVec3f::ColorQuantityTransform
(const Nb_DString flipstr,
 const Nb_DString& quantity_in,OC_REAL8m phase_in,OC_BOOL invert_in,
 Nb_DString& quantity_out,OC_REAL8m& phase_out,OC_BOOL& invert_out) const
{
  return ColorQuantityTransformHelper(flipstr,
                                      quantity_in,phase_in,invert_in,
                                      quantity_out,phase_out,invert_out);
}


OC_REAL4m Vf_GridVec3f::GetVecShade(const char* colorquantity,
                                 OC_REAL8m phase,OC_BOOL invert,
                                 const Nb_Vec3<OC_REAL4>& v) const
{// NOTE: The input vector v is assumed to be pre-scaled, i.e., no
 //  DisplayValueScale or ValueMultiplier is applied inside this routine.
 //  MagHints are used, however.
  OC_REAL8m shade = 0.5;
  if(strcmp("xy-angle",colorquantity)==0) {
    if(v.x==0. && v.y==0.) {
      if(invert) shade = 1. - phase;
      else       shade = -phase;
    } else {
      shade = Oc_Atan2((double)v.y,(double)v.x)*(1./(2*PI));
      shade = shade - floor(shade+phase);  // Force into [0,1)
    }
  } else if(strcmp("xz-angle",colorquantity)==0) {
    if(v.x==0. && v.z==0.) {
      if(invert) shade = 1. - phase;
      else       shade = -phase;
    } else {
      shade = Oc_Atan2((double)v.z,(double)v.x)*(1./(2*PI));
      shade = shade - floor(shade+phase);  // Force into [0,1)
    }
  } else if(strcmp("yz-angle",colorquantity)==0) {
    if(v.y==0. && v.z==0.) {
      if(invert) shade = 1. - phase;
      else       shade = -phase;
    } else {
      shade = Oc_Atan2((double)v.z,(double)v.y)*(1./(2*PI));
      shade = shade - floor(shade+phase);  // Force into [0,1)
    }
  } else if(strcmp("slice",colorquantity)==0) {
    // Slice color doesn't depend on vector components,
    // but rather position.  Technically, we should
    // probably return 0.5 or -1 here, but as a convenience
    // to calls from the coords display, we'll treat v here
    // as referring to the position, and furthermore expand
    // to maximum range
    if(v.z<0)      shade=0.0;
    else if(v.z>0) shade=1.0;
    else           shade=0.5;
  } else {
    OC_REAL8m val=0.0;
    if(strcmp("x",colorquantity)==0)        val = v.x;
    else if(strcmp("y",colorquantity)==0)   val = v.y;
    else if(strcmp("z",colorquantity)==0)   val = v.z;
    else                                    return -1;

    OC_REAL8m minsize=0.0,maxsize=1.0;
    if(MaxMagHint<1.0 || GetDisplayValueScale()<DBL_MAX/MaxMagHint) {
      maxsize = GetDisplayValueScale()*MaxMagHint;
    }
    if(MaxMagHint>MinMagHint) {
      minsize = maxsize*MinMagHint/MaxMagHint;
    }

    if(minsize>0.0 && fabs(v.x)<minsize
       && fabs(v.y)<minsize && fabs(v.z)<minsize) {
      // Overflow-protected min size check
      OC_REAL8m tmp,sum=0.0;
      tmp = v.x/minsize; sum += tmp*tmp;
      tmp = v.y/minsize; sum += tmp*tmp;
      tmp = v.z/minsize; sum += tmp*tmp;
      if(sum<1.0) return -1;
    }

    if(fabs(val)<maxsize) shade = (1.+val/maxsize)/2.;
    else if(val>0.)   shade = 1.0;
    else              shade = 0.0;
  }

  shade+=phase;
  if(shade>1.0)      shade=1.0;
  else if(shade<0.0) shade=0.0;
  if(invert) shade = 1.0 - shade;

  return static_cast<OC_REAL4m>(shade);
}


OC_BOOL Vf_GridVec3f::FindPreciseClosest(const Nb_Vec3<OC_REAL8> &pos,
                                         Nb_LocatedVector<OC_REAL8> &lv)
{
  OC_BOOL inmesh = 1;
  Nb_Vec3<OC_REAL8> npos=ProblemToNodalCoords(pos);
  OC_INDEX i,isize,j,jsize,k,ksize;
  GetDimens(isize,jsize,ksize);
  i=OC_INDEX(OC_ROUND(npos.x));
  if(i<0) {
    i=0;       inmesh=0;
  } else if(i>=isize) {
    i=isize-1; inmesh=0;
  }
  j=OC_INDEX(OC_ROUND(npos.y));
  if(j<0) {
    j=0;       inmesh=0;
  } else if(j>=jsize) {
    j=jsize-1; inmesh=0;
  }
  k=OC_INDEX(OC_ROUND(npos.z));
  if(k<0) {
    k=0;       inmesh=0;
  } else if(k>=ksize) {
    k=ksize-1; inmesh=0;
  }

  lv.value=GridVec(i,j,k);
  lv.location=NodalToProblemCoords(Nb_Vec3<OC_REAL8>(OC_REAL8(i),
                                                  OC_REAL8(j),OC_REAL8(k)));
  return inmesh;
}

void Vf_GridVec3f::GetZsliceParameters(OC_REAL8m& zslicebase,
                                       OC_REAL8m& zstep)
{
  zslicebase = coords_base.z;
  zstep = coords_step.z;
}

void Vf_GridVec3f::GetZslice(OC_REAL8m zlow,OC_REAL8m zhigh,
                             OC_INDEX& islicelow,OC_INDEX& islicehigh)
{ // Coordinate this with the method used in GetDisplayList()
  // The exports should be interpreted as including islicelow,
  // and everything above up to but not including islicehigh.

  OC_INDEX maxslice = grid.GetDim1(); // NB: grid dim1 is z-axis
  OC_REAL8m zstepsize = fabs(coords_step.z);

  // The mmDisp interface displays 4 decimal digits; so, if
  // |zhigh-zlow| is within 0.00051 of zstepsize, then assume
  // the requester really wants exactly one slice.
  const OC_REAL8m interface_error = 0.00051;
  if(fabs(fabs(zhigh-zlow)-zstepsize) < interface_error * zstepsize) {
    // First handle some special rounding cases
    OC_REAL8m zbottom = coords_base.z;
    OC_REAL8m ztop = coords_base.z + (maxslice-1)*coords_step.z;
    if(coords_step.z<0) {
      if(fabs(zlow-zbottom) < interface_error * zstepsize){
        islicelow=0; islicehigh=1; return;
      }
      if(fabs(zhigh-ztop)< interface_error * zstepsize){
        islicelow=maxslice-1; islicehigh=maxslice; return;
      }
    } else {
      if(fabs(zhigh-zbottom) < interface_error * zstepsize){
        islicelow=0; islicehigh=1; return;
      }
      if(fabs(zlow-ztop)< interface_error * zstepsize){
        islicelow=maxslice-1; islicehigh=maxslice; return;
      }
    }

    // Otherwise, find the slice nearest the middle of the
    // requested range
    islicelow =
      static_cast<OC_INDEX>(OC_ROUND((zlow+zhigh-2*coords_base.z)
                                     /(2*coords_step.z)));
    if(islicelow<0) {
      islicelow=islicehigh=0;
    } else if(islicelow>=maxslice) {
      islicelow=islicehigh=maxslice;
    } else {
      islicehigh=islicelow+1;
    }

    return;
  }


  if(coords_step.z<0) {
    islicelow =
      static_cast<OC_INDEX>(ceil((zhigh-coords_base.z)/coords_step.z));
    islicehigh =
      static_cast<OC_INDEX>(floor((zlow-coords_base.z)/coords_step.z))+1;
  } else {
    islicelow =
      static_cast<OC_INDEX>(ceil((zlow-coords_base.z)/coords_step.z));
    islicehigh =
      static_cast<OC_INDEX>(floor((zhigh-coords_base.z)/coords_step.z))+1;
  }

  if(islicelow<0) islicelow=0;
  if(islicehigh>maxslice)  islicehigh=maxslice;
}

OC_INDEX Vf_GridVec3f::GetDisplayList(OC_REAL4m &xstep_request,
                                   OC_REAL4m &ystep_request,
                                   OC_REAL4m &zstep_request,
                                   const Nb_BoundingBox<OC_REAL4> &dsprange,
                                   const char *colorquantity,
                                   OC_REAL8m phase,
                                   OC_BOOL invert, OC_BOOL trimtiny,
                                   Nb_List<Vf_DisplayVector> &display_list)
{ // Fills display_list with points from mesh inside "dsprange", with
  // step size close to ?step_request, which is reset to value actually
  // used.
  //   Use ?step_request=0 to remove constraint on that axis, e.g.,
  // if xstep_request=ystep_request=zstep_request=0, then this
  // routine places all points inside "dsprange" into display_list.
  //   NOTE: Non-integral values for step_request lead to unattractive
  // banding in the output display list, and so are currently
  // overridden.
#define MEMBERNAME "GetDisplayList"
  if(xstep_request<0 || ystep_request<0 || zstep_request<0)
    FatalError(-1,STDDOC,"%s: xstep=%f, ystep=%f, zstep=%f",
               ErrBadParam,xstep_request,ystep_request,zstep_request);

  enum Gv3Color { GV3_NONE, GV3_X, GV3_Y, GV3_Z, GV3_ZSLICE,GV3_MAG,
                  GV3_XYANGLE, GV3_XZANGLE, GV3_YZANGLE,
                  GV3_DIV};
  Gv3Color cq=GV3_NONE;
  if(colorquantity!=NULL) {
    if(strcmp("x",colorquantity)==0)             cq=GV3_X;
    else if(strcmp("y",colorquantity)==0)        cq=GV3_Y;
    else if(strcmp("z",colorquantity)==0)        cq=GV3_Z;
    else if(strcmp("slice",colorquantity)==0)    cq=GV3_ZSLICE;
    else if(strcmp("mag",colorquantity)==0)      cq=GV3_MAG;
    else if(strcmp("xy-angle",colorquantity)==0) cq=GV3_XYANGLE;
    else if(strcmp("xz-angle",colorquantity)==0) cq=GV3_XZANGLE;
    else if(strcmp("yz-angle",colorquantity)==0) cq=GV3_YZANGLE;
    else if(strcmp("div",colorquantity)==0)      cq=GV3_DIV;
  }

  display_list.Clear();
  OC_INDEX isize,jsize,ksize;
  GetDimens(isize,jsize,ksize);
  if(isize<1 || jsize<1 || ksize<1) {
    return display_list.GetSize(); // Empty grid
  }
  Nb_Vec3<OC_REAL4> srangemin,srangemax;
  Nb_Vec3<OC_REAL8> rangemin,rangemax;
  dsprange.GetExtremes(srangemin,srangemax);
  Convert(srangemin,rangemin);
  Convert(srangemax,rangemax);

  // Convert dsprange & step requests from problem coords to nodal coords
  rangemin=ProblemToNodalCoords(rangemin);
  rangemax=ProblemToNodalCoords(rangemax);
  if(coords_step.x<0) Nb_Swap(rangemin.x,rangemax.x); // Adjust for ordering
  if(coords_step.y<0) Nb_Swap(rangemin.y,rangemax.y); // across coord.
  if(coords_step.z<0) Nb_Swap(rangemin.z,rangemax.z); // transform.

  OC_REAL8m nxstep=xstep_request/fabs(coords_step.x);
  OC_REAL8m nystep=ystep_request/fabs(coords_step.y);
  OC_REAL8m nzstep=zstep_request/fabs(coords_step.z);

  OC_INDEX ifirst,ilast,jfirst,jlast,kfirst,klast;
  ifirst=OC_INDEX(ceil(rangemin.x)); ilast=OC_INDEX(floor(rangemax.x))+1;
  jfirst=OC_INDEX(ceil(rangemin.y)); jlast=OC_INDEX(floor(rangemax.y))+1;
  kfirst=OC_INDEX(ceil(rangemin.z)); klast=OC_INDEX(floor(rangemax.z))+1;

  if(ifirst<0 || ilast>isize || jfirst<0 || jlast>jsize ||
     kfirst<0 || klast>ksize) {
#if DEBUGLVL > 99
    NonFatalError(STDDOC,"Range box out-of-bounds: "
        "(%" OC_INDEX_MOD "d,%" OC_INDEX_MOD "d,%" OC_INDEX_MOD "d) to "
        "(%" OC_INDEX_MOD "d,%" OC_INDEX_MOD "d,%" OC_INDEX_MOD "d), "
        "not in (0,0,0) to "
        "(%" OC_INDEX_MOD "d,%" OC_INDEX_MOD "d,%" OC_INDEX_MOD "d)",
                  ifirst,jfirst,kfirst,ilast,jlast,klast,
                  isize,jsize,ksize);
#endif
    if(ifirst<0) ifirst=0;
    if(ilast>isize) ilast=isize;
    if(jfirst<0) jfirst=0;
    if(jlast>jsize) jlast=jsize;
    if(kfirst<0) kfirst=0;
    if(klast>ksize) klast=ksize;
  }

  // Calculate step sizes
  // Note: Non-integer step size produce ugly banding in output sampling
  nxstep=OC_ROUND(nxstep);
  nystep=OC_ROUND(nystep);
  nzstep=OC_ROUND(nzstep);

  OC_REAL8m max_step=OC_REAL4m(OC_MAX(OC_MAX(isize,jsize),ksize));
  if(nxstep>max_step) nxstep=max_step;
  if(nystep>max_step) nystep=max_step;
  if(nzstep>max_step) nzstep=max_step;

  OC_INDEX istep=OC_INDEX(nxstep);
  OC_INDEX jstep=OC_INDEX(nystep);
  OC_INDEX kstep=OC_INDEX(nzstep);

  if(istep<1) istep=1;
  if(jstep<1) jstep=1;
  if(kstep<1) kstep=1;

  // Determine offsets
  OC_INDEX ioff=((ilast-ifirst-1)%istep)/2;
  OC_INDEX joff=((jlast-jfirst-1)%jstep)/2;
  OC_INDEX koff=((klast-kfirst-1)%kstep)/2;

  OC_INDEX i,j,k;
  OC_INDEX istart,jstart,kstart;
  Nb_Vec3<OC_REAL8m> position;  // Position, in "Problem" coords
  Nb_Vec3<OC_REAL4m> nposition;  // "Narrow" position
  OC_REAL8m xstart,ystart,zstart,xstep,ystep,zstep;

  istart=ifirst+ioff;
  xstart=coords_base.x+coords_step.x*OC_REAL8m(istart);
  xstep=coords_step.x*OC_REAL8m(istep);

  jstart=jfirst+joff;
  ystart=coords_base.y+coords_step.y*OC_REAL8m(jstart);
  ystep=coords_step.y*OC_REAL8m(jstep);

  kstart=kfirst+koff;
  zstart=coords_base.z+coords_step.z*OC_REAL8m(kstart);
  zstep=coords_step.z*OC_REAL8m(kstep);


  OC_REAL8m magsq,minmagsq; // NOTE: Properties are scaled so
  /// that effective "maxmagsq" is 1.0.
  if(MinMagHint>MaxMagHint) {
    NonFatalError(STDDOC,"MinMagHint (%g) > MaxMagHintValue (%g)",
                  static_cast<double>(MinMagHint),
                  static_cast<double>(MaxMagHint));
    return display_list.GetSize(); // Empty grid
  }
  if(!trimtiny) {
    minmagsq = 0.0;
  } else if(MaxMagHint>0.0) {
    // minmagsq is intended for display optimization, not display
    // selection.  Guarantee here some more-or-less arbitrary minimum
    // range between minmag cutoff and MaxMagHint.
    if(MinMagHint<MaxMagHint/1024.) {
      minmagsq=MinMagHint/MaxMagHint;
      minmagsq*=minmagsq;
      minmagsq*=0.999; // Round down 0.1% to protect against round-off
    } else {
      minmagsq = 1.0/(1024.*1024.);
    }
  } else {
    minmagsq=1e-16;  // Pretend MaxMagHint=1.0, and MinMagHint=1e-8
  }

  Nb_Vec3<OC_REAL8> wv;
  Nb_Vec3<OC_REAL4> v;

  const OC_REAL8m datascale = GetDisplayValueScale();
  OC_REAL8m wvscale=MaxMagHint;

  if(fabs(ValueMultiplier)<1.0 && wvscale>FLT_MAX*ValueMultiplier) {
    NonFatalError(STDDOC,"Value scale range error (too big): %g",
                  static_cast<double>(datascale));
    return display_list.GetSize(); // Empty grid
  }
  wvscale/=ValueMultiplier;

  // minmagsq is used for excluding small vectors.  Transform
  // this to raw data scale, so we can do this check before
  // scaling the data vectors.  NOTE: minmagsq scaling should
  // be unaffected by datascale.
  minmagsq *= wvscale; // NB: (wvscale*wvscale) might overflow,
  minmagsq *= wvscale; //  but (magsq*wvscale)*wvscale shouldn't.

  if(datascale>1.0 && wvscale>FLT_MAX/datascale) {
    NonFatalError(STDDOC,"Value scale range error (too big): %g",
                  static_cast<double>(datascale));
    return display_list.GetSize(); // Empty grid
  }
  wvscale*=datascale;

  if(wvscale==0.0) wvscale=1.0; // Safety
  OC_REAL8m wvmult=1.0/wvscale;

  // Determine scaling for divergence calculation
  OC_REAL8m xdivscale=0., ydivscale=0.,  zdivscale=0.;
  if(cq==GV3_DIV) {
    if(isize>1 && xstep!=0.) { xdivscale=1.0/xstep; }
    if(jsize>1 && ystep!=0.) { ydivscale=1.0/ystep; }
    if(ksize>1 && zstep!=0.) { zdivscale=1.0/zstep; }
    OC_REAL8m maxdiv=fabs(xdivscale)+fabs(ydivscale)+fabs(zdivscale);
    // We don't include in the scaling any dimension with only
    // 1 cell, because that dimension will not contribute to
    // the (discrete) divergence.  One could argue that if ?size
    // is only 2, then the max scaling factor should be divided by
    // 2, but in some sense such a change just highlights the
    // inadequacies of the discrete divergence.  So we leave it out
    // for now.
    if(maxdiv>0.0) {
      OC_REAL8m wvtest= ( fabs(wvscale)<1.0 ? 1.0 : wvscale );
      wvtest=1.0/wvtest;
      if(maxdiv>(FLT_MAX*wvtest/2.0)) {
        NonFatalError(STDDOC,
                      "Divergence scaling range error (too big): %g",
                      static_cast<double>(maxdiv));
        return display_list.GetSize(); // Empty grid
      } else {
        maxdiv*=wvscale; // Nominal max divergence
        OC_REAL8m temp=0.5/maxdiv;
        xdivscale*=temp;
        ydivscale*=temp;
        zdivscale*=temp;
      }
    }
  }

  OC_REAL8m shade;
  OC_REAL8m tempshade,xtempshade,ytempshade,ztempshade; // Used in div calc

  // Extract z-range for use by zslice coloring
  OC_REAL8m zrangemid=0.,zrangemult=1.0;
  if(cq==GV3_ZSLICE) {
    Nb_Vec3<OC_REAL8> tempmin,tempmax;
    data_range.GetExtremes(tempmin,tempmax);
    zrangemid = (tempmin.z + tempmax.z)/2.0;
    OC_REAL8m zrangespan = tempmax.z-tempmin.z;
    if(zrangespan==0.0) zrangemult=1.0;
    else                zrangemult=1.0/zrangespan;
    OC_REAL8m tempgrvs=GetDisplayValueScale();
    if(tempgrvs>1.0 && zrangemult > FLT_MAX / tempgrvs) {
      NonFatalError(STDDOC,"Value scale range error (too big): %g",
                    static_cast<double>(tempgrvs));
      return display_list.GetSize(); // Empty grid
    } else {
      zrangemult *= tempgrvs;
    }
    if(zrangemult==0.0) zrangemult=1.0; // Safety
  }

  for(k=kstart,position.z=zstart;k<klast;k+=kstep,position.z+=zstep) {
    for(j=jstart,position.y=ystart;j<jlast;j+=jstep,position.y+=ystep) {
      for(i=istart,position.x=xstart;i<ilast;i+=istep,position.x+=xstep) {
        wv=GridVec(i,j,k);
        magsq=wv.MagSq();
        if(magsq<=minmagsq) continue; // Skip tiny vectors
        /// Note that minmagsq is at raw data scale, unless trimtiny is
        /// false, in which case minmagsq=0.0. Note: There are cases,
        /// particularly for scalar fields, where one would like
        /// magsq==0 pixels displayed. But it is not obvious how to
        /// discriminate between those situations and the far more
        /// common case where magsq==0 indicates an Ms==0 empty space
        /// that is more usefully left unpainted.

        if(fabs(wvscale)<1.0 && magsq>(FLT_MAX*wvscale)*wvscale) {
          // Vector will overflow floating point range; resize
          wv*=sqrt(FLT_MAX/magsq);
          magsq=FLT_MAX;
        } else {
          magsq*=wvmult; // NB: (wvmult*wvmult) might overflow,
          magsq*=wvmult; //  but (magsq*wvmult)*wvmult shouldn't.
          wv*=wvmult; // User specified data value scaling
        }
        shade=0.5;
        switch(cq) {
        case GV3_X:
          shade=(1.+wv.x)/2.;
          break;
        case GV3_Y:
          shade=(1.+wv.y)/2.;
          break;
        case GV3_Z:
          shade=(1.+wv.z)/2.;
          break;
        case GV3_ZSLICE:
          shade = (position.z-zrangemid)*zrangemult + 0.5;
          break;
        case GV3_MAG:
          shade=sqrt(magsq);
          break;
        case GV3_XYANGLE:
          if(wv.x==0. && wv.y==0.) {
            if(invert) shade = 1.-phase;
            else       shade = -phase;
          } else {
            shade = Oc_Atan2(wv.y,wv.x)*(1./(2*PI));
            shade = shade - floor(shade+phase);  // Force into [0,1)
          }
          break;
        case GV3_XZANGLE:
          if(wv.z==0. && wv.x==0.) {
            if(invert) shade = 1.-phase;
            else       shade = -phase;
          } else {
            shade = Oc_Atan2(wv.z,wv.x)*(1./(2*PI));
            shade = shade - floor(shade+phase);  // Force into [0,1)
          }
          break;
        case GV3_YZANGLE:
          if(wv.z==0. && wv.y==0.) {
            if(invert) shade = 1.-phase;
            else       shade = -phase;
          } else {
            shade = Oc_Atan2(wv.z,wv.y)*(1./(2*PI));
            shade = shade - floor(shade+phase);  // Force into [0,1)
          }
          break;
        case GV3_DIV:
          xtempshade=ytempshade=ztempshade=0.0;
          if(i+istep<isize) xtempshade+=GridVec(i+istep,j,k).x;
          if(i-istep>=0)    xtempshade-=GridVec(i-istep,j,k).x;
          if(j+jstep<jsize) ytempshade+=GridVec(i,j+jstep,k).y;
          if(j-jstep>=0)    ytempshade-=GridVec(i,j-jstep,k).y;
          if(k+kstep<ksize) ztempshade+=GridVec(i,j,k+kstep).z;
          if(k-kstep>=0)    ztempshade-=GridVec(i,j,k-kstep).z;
          tempshade = xdivscale*xtempshade
            + ydivscale*ytempshade
            + zdivscale*ztempshade;
          shade=(1.+tempshade)/2.;
          break;
        default:
          break;
        }

        // Make phase and invert adjustments
        shade+=phase;
        if(shade>1.0)      shade=1.0;
        else if(shade<0.0) shade=0.0;
        if(invert) shade = 1.0 - shade;

        if(magsq>1.0) {
          // Cut back (trim) overly large vectors
          wv*=1.0/sqrt(magsq);
          magsq=1.0;  // Safety
        }
        Convert(wv,v);  // Narrow from OC_REAL8 to OC_REAL4
        Convert(position,nposition);
        display_list.Append(Vf_DisplayVector(nposition,v,
                                             static_cast<OC_REAL4m>(shade),
                                             k));
        /// Note: Nominal "full length" value vectors v should
        ///       have magnitude 1.
      }
    }
  }

  // Export actual step sizes, in problem units
  xstep_request=static_cast<OC_REAL4m>(nxstep*fabs(coords_step.x));
  ystep_request=static_cast<OC_REAL4m>(nystep*fabs(coords_step.y));
  zstep_request=static_cast<OC_REAL4m>(nzstep*fabs(coords_step.z));

  return display_list.GetSize();
#undef MEMBERNAME
}

OC_BOOL Vf_GridVec3f::GetFirstPt(Vf_Mesh_Index* &key_export,
                                 Nb_LocatedVector<OC_REAL8> &lv) const
{
  key_export=(Vf_Mesh_Index*)(new Vf_GridVec3f_Index);
  if(GetSize()<=0) return 0;

  lv.location=GetBasePoint();
  lv.value=GridVec(0,0,0);
  return 1;
}

OC_BOOL Vf_GridVec3f::GetNextPt(Vf_Mesh_Index* &key_import,
                                Nb_LocatedVector<OC_REAL8> &lv) const
{
  Vf_GridVec3f_Index *key = (Vf_GridVec3f_Index *)key_import;
  OC_INDEX isize,jsize,ksize;
  GetDimens(isize,jsize,ksize);
  OC_INDEX i=key->i,j=key->j,k=key->k;
  ++i;
  if(i>=isize) { i=0; ++j; }
  if(j>=jsize) { j=0; ++k; }
  if(k>=ksize) return 0;
  lv.location=GetBasePoint();
  Nb_Vec3<OC_REAL8> step=GetGridStep();
  step.x*=i;
  step.y*=j;
  step.z*=k;
  lv.location+=step;
  lv.value=GridVec(i,j,k);
  key->i=i; key->j=j; key->k=k;
  return 1;
}

OC_BOOL Vf_GridVec3f::SetNodeValue(const Vf_Mesh_Index* key_import,
                                   const Nb_Vec3<OC_REAL8>& value)
{ // For use by generic Vf_Mesh code for manipulation of
  // mesh node values (as opposed to node locations).
  // Returns 0 on success, 1 if the index is invalid
  // or out-of-bounds.

  Vf_GridVec3f_Index *key = (Vf_GridVec3f_Index *)key_import;
  /// This should be a dynamic cast, but we don't want to
  /// require RTTI in this code.  So we leave it up to the
  /// programmer to insure that keys imported here were created
  /// by the corresponding GetFirstPt or GetNextPt functions.

  // Check key validity
  OC_INDEX isize,jsize,ksize;
  GetDimens(isize,jsize,ksize);
  OC_INDEX i=key->i,j=key->j,k=key->k;
  if(i<0 || i>=isize || j<0 || j>=jsize || k<0 || k>=ksize)
    return 1; // Invalid or out-of-bounds key

  // Set value
  GridVec(i,j,k) = value;

  return 0;
}



Vf_GridVec3f::Vf_GridVec3f
(const Vf_GridVec3f& import_mesh,
 OC_REAL8m subsample,
 const char* flipstr,
 Nb_BoundingBox<OC_REAL8>& clipbox,
 OC_BOOL cliprange)
  : Vf_Mesh(import_mesh)
{ // Copy constructor with transform
#define MEMBERNAME "Constructor_copy_with_transform"
  const char* parsestr=" \t\n:";
  Nb_Vec3<OC_REAL8> vtemp;
  int xsign=0,ysign=0,zsign=0;
  OC_INDEX new_xdim=-1,new_ydim=-1,new_zdim=-1;
  OC_INDEX xstep=0,ystep=0,zstep=0;
  OC_INDEX xindex=0,yindex=0,zindex=0;
  OC_INDEX *ix=NULL,*iy=NULL,*iz=NULL;
  OC_INDEX *ixstart=NULL,*iystart=NULL,*izstart=NULL;
  OC_REAL8 *vx=NULL,*vy=NULL,*vz=NULL;
  Nb_Vec3<OC_REAL8> minpt,maxpt;

  // Parameter value check
  if(subsample<0.0 || floor(subsample) != subsample) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_GridVec3f","Vf_GridVec3f",
                          VF_MESH_ERRBUFSIZE+200,
                          "Requested subsample value (%g) not supported; "
                          "subsample must be a positive integer.",
                          subsample));
  }
  OC_INDEX isubsample = static_cast<OC_INDEX>(subsample);
  if(isubsample == 0) isubsample = 1;  // 0 means 1

  // Determine new base point and dimensions, modified as necessary
  // by clipbox request.
  OC_INDEX import_xdim,import_ydim,import_zdim;
  import_mesh.GetDimens(import_xdim,import_ydim,import_zdim);
  OC_INDEX clipped_import_xdim=import_xdim;
  OC_INDEX clipped_import_ydim=import_ydim;
  OC_INDEX clipped_import_zdim=import_zdim;
  OC_INDEX clipped_import_xstart=0;
  OC_INDEX clipped_import_ystart=0;
  OC_INDEX clipped_import_zstart=0;
  Nb_Vec3<OC_REAL8> clipped_import_base = import_mesh.coords_base;

  clipbox.GetExtremes(minpt,maxpt);
  OC_REAL8m stepx = import_mesh.coords_step.x;
  if(stepx>0.) { // Safety
    OC_REAL8m xmin = import_mesh.coords_base.x;
    // Determine start offset
    if(minpt.x>xmin) {
      clipped_import_xstart = OC_INDEX(ceil((minpt.x - xmin)/stepx));
    }
    // Determine dim
    clipped_import_xdim =
      OC_MIN(OC_INDEX(floor((maxpt.x - xmin)/stepx))+1,import_xdim)
      - clipped_import_xstart;
    if(clipped_import_xdim % isubsample == 0) {
      // A whole number of subsample groups fit into clipped region;
      // shift start point to center samples inside clipped region
      clipped_import_xstart += (isubsample-1)/2;
    }
    clipped_import_xdim = (clipped_import_xdim+isubsample-1)/isubsample;
    if(clipped_import_xdim<0) clipped_import_xdim=0;
  }
  OC_REAL8m stepy = import_mesh.coords_step.y;
  if(stepy>0.) { // Safety
    OC_REAL8m ymin = import_mesh.coords_base.y;
    // Determine start offset
    if(minpt.y>ymin) {
      clipped_import_ystart = OC_INDEX(ceil((minpt.y - ymin)/stepy));
    }
    // Determine dim
    clipped_import_ydim =
      OC_MIN(OC_INDEX(floor((maxpt.y - ymin)/stepy))+1,import_ydim)
      - clipped_import_ystart;
    if(clipped_import_ydim % isubsample == 0) {
      // A whole number of subsample groups fit into clipped region;
      // shift start point to center samples inside clipped region
      clipped_import_ystart += (isubsample-1)/2;
    }
    clipped_import_ydim = (clipped_import_ydim+isubsample-1)/isubsample;
    if(clipped_import_ydim<0) clipped_import_ydim=0;
  }
  OC_REAL8m stepz = import_mesh.coords_step.z;
  if(stepz>0.) { // Safety
    OC_REAL8m zmin = import_mesh.coords_base.z;
    // Determine start offset
    if(minpt.z>zmin) {
      clipped_import_zstart = OC_INDEX(ceil((minpt.z - zmin)/stepz));
    }
    // Determine dim
    clipped_import_zdim =
      OC_MIN(OC_INDEX(floor((maxpt.z - zmin)/stepz))+1,import_zdim)
      - clipped_import_zstart;
    if(clipped_import_zdim % isubsample == 0) {
      // A whole number of subsample groups fit into clipped region;
      // shift start point to center samples inside clipped region
      clipped_import_zstart += (isubsample-1)/2;
    }
    clipped_import_zdim = (clipped_import_zdim+isubsample-1)/isubsample;
    if(clipped_import_zdim<0) clipped_import_zdim=0;
  }
  clipped_import_base.Set(import_mesh.coords_base.x
                          + clipped_import_xstart*stepx,
                          import_mesh.coords_base.y
                          + clipped_import_ystart*stepy,
                          import_mesh.coords_base.z
                          + clipped_import_zstart*stepz);
  // Note: If any of the clipped_import_?dim are <1,
  // then the resulting mesh is empty.


  // Interpret flipstr.  The transform procedure is to read data one
  // point at a time from import_mesh, coping into vtemp
  // through pointers vx, vy, vz with sign multipliers xsign, ysign,
  // zsign on the first, second and third components from the source
  // point, respectively.  For example, if the flipstr is -z:y:x, then
  // vx will point to vtemp.z and xsign will be -1, so
  // *vx = xsign*source.x is actually vtemp.z = -1 * source.x,
  // as desired.
  //  The grid index variables ([xyz]index, *i[xyz]) are used to
  // index back into import_mesh, and so work off the inverse of the
  // flip transform.  This way we access the new grid in the natural
  // order, and hop through the import grid with probably bad strides.
  // But we get bad strides on import_mesh if subsampling is enabled,
  // so this is apparently the preferred order.
  char* flipbuf = new char[strlen(flipstr)+1];
  strcpy(flipbuf,flipstr);
  char* nexttoken=flipbuf;
  // Component 1
  char *token = Nb_StrSep(&nexttoken,parsestr);
  OC_INDEX len=0;
  if(token==NULL || (len=static_cast<OC_INDEX>(strlen(token)))<1 || len>2
     || (token[len-1]!='x' && token[len-1]!='y' && token[len-1]!='z')
     || (len==2 && token[0]!='-' && token[0]!='+')) {
    NonFatalError(STDDOC,"Invalid transform flip string: %.200s",flipstr);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_GridVec3f","Vf_GridVec3f",
                          VF_MESH_ERRBUFSIZE+200,
                          "Invalid transform flip string: %.200s",flipstr));
  }
  xsign=1; if(len==2 && token[0]=='-') xsign=-1;
  if(token[len-1]=='x') {
    new_xdim = clipped_import_xdim;
    ix = &xindex;
    ixstart = &clipped_import_xstart;
    xstep = xsign;
    vx = &vtemp.x;
  } else if(token[len-1]=='y') {
    new_ydim = clipped_import_xdim;
    iy = &xindex;
    iystart = &clipped_import_xstart;
    ystep = xsign;
    vx = &vtemp.y;
  } else if(token[len-1]=='z') {
    new_zdim = clipped_import_xdim;
    iz = &xindex;
    izstart = &clipped_import_xstart;
    zstep = xsign;
    vx = &vtemp.z;
  }
  // Component 2
  token = Nb_StrSep(&nexttoken,parsestr);
  if(token==NULL || (len=static_cast<OC_INDEX>(strlen(token)))<1 || len>2
     || (token[len-1]!='x' && token[len-1]!='y' && token[len-1]!='z')
     || (len==2 && token[0]!='-' && token[0]!='+')) {
    NonFatalError(STDDOC,"Invalid transform flip string: %.200s",flipstr);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_GridVec3f","Vf_GridVec3f",
                          VF_MESH_ERRBUFSIZE+200,
                          "Invalid transform flip string: %.200s",flipstr));
  }
  ysign=1; if(len==2 && token[0]=='-') ysign=-1;
  if(token[len-1]=='x') {
    new_xdim = clipped_import_ydim;
    ix = &yindex;
    ixstart = &clipped_import_ystart;
    xstep = ysign;
    vy = &vtemp.x;
  } else if(token[len-1]=='y') {
    new_ydim = clipped_import_ydim;
    iy = &yindex;
    iystart = &clipped_import_ystart;
    ystep = ysign;
    vy = &vtemp.y;
  } else if(token[len-1]=='z') {
    new_zdim = clipped_import_ydim;
    iz = &yindex;
    izstart = &clipped_import_ystart;
    zstep = ysign;
    vy = &vtemp.z;
  }
  // Component 3
  token = Nb_StrSep(&nexttoken,parsestr);
  if(token==NULL || (len=static_cast<OC_INDEX>(strlen(token)))<1 || len>2
     || (token[len-1]!='x' && token[len-1]!='y' && token[len-1]!='z')
     || (len==2 && token[0]!='-' && token[0]!='+')) {
    NonFatalError(STDDOC,"Invalid transform flip string: %.200s",flipstr);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_GridVec3f","Vf_GridVec3f",
                          VF_MESH_ERRBUFSIZE+200,
                          "Invalid transform flip string: %.200s",flipstr));
  }
  zsign=1; if(len==2 && token[0]=='-') zsign=-1;
  if(token[len-1]=='x') {
    new_xdim = clipped_import_zdim;
    ix = &zindex;
    ixstart = &clipped_import_zstart;
    xstep = zsign;
    vz = &vtemp.x;
  } else if(token[len-1]=='y') {
    new_ydim = clipped_import_zdim;
    iy = &zindex;
    iystart = &clipped_import_zstart;
    ystep = zsign;
    vz = &vtemp.y;
  } else if(token[len-1]=='z') {
    new_zdim = clipped_import_zdim;
    iz = &zindex;
    izstart = &clipped_import_zstart;
    zstep = zsign;
    vz = &vtemp.z;
  }
  if(Nb_StrSep(&nexttoken,parsestr)!=NULL ||
     vx==vy || vx==vz || vy==vz ||
     new_xdim<0 || new_ydim<0 || new_zdim<0) {
    NonFatalError(STDDOC,"Invalid transform flip string: %.200s",flipstr);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_GridVec3f","Vf_GridVec3f",
                          VF_MESH_ERRBUFSIZE+200,
                          "Invalid transform flip string: %.200s",flipstr));
  }
  delete[] flipbuf;

  // Copy/setup supplementary data members
  data_range = import_mesh.data_range;
  data_range.IntersectWith(clipbox);
  if(!data_range.IsEmpty()) {
    data_range.GetExtremes(minpt,maxpt);
    *vx = xsign*minpt.x; *vy = ysign*minpt.y; *vz = zsign*minpt.z;
    minpt = vtemp;
    *vx = xsign*maxpt.x; *vy = ysign*maxpt.y; *vz = zsign*maxpt.z;
    maxpt = vtemp;
    data_range.SortAndSet(minpt,maxpt);
  }


  range.Set(import_mesh.range);
  if(cliprange) range.IntersectWith(clipbox);
  Nb_NOP(&range);  // There is a bug in the Intel C++ optimizer
  /// (icpc (ICC) 10.0 20070809) that results in range (and/or the
  /// minpt, maxpt in the next line) not being updated with the
  /// clipbox intersection results.  The call to Nb_NOP is a
  /// workaround.
  range.GetExtremes(minpt,maxpt);

  *vx = xsign*minpt.x; *vy = ysign*minpt.y; *vz = zsign*minpt.z;
  minpt = vtemp;
  *vx = xsign*maxpt.x; *vy = ysign*maxpt.y; *vz = zsign*maxpt.z;
  maxpt = vtemp;
  range.SortAndSet(minpt,maxpt);
  range.ExpandWith(data_range);

  *vx = xsign * clipped_import_base.x;
  *vy = ysign * clipped_import_base.y;
  *vz = zsign * clipped_import_base.z;
  coords_base = vtemp;

  *vx = xsign * isubsample * import_mesh.coords_step.x;
  *vy = ysign * isubsample * import_mesh.coords_step.y;
  *vz = zsign * isubsample * import_mesh.coords_step.z;
  coords_step = vtemp;

  boundary_from_data = import_mesh.boundary_from_data;

  boundary.Clear(); // Safety
  if(!boundary_from_data) {
    // Copy and transform boundary from import_mesh
    const Nb_Vec3<OC_REAL8> *vptr;
    Nb_List_Index< Nb_Vec3<OC_REAL8> > key;
    for(vptr=import_mesh.boundary.GetFirst(key);
        vptr!=NULL;
        vptr=import_mesh.boundary.GetNext(key)) {
      Nb_Vec3<OC_REAL8> tmpvec = *vptr;
      if(cliprange) clipbox.MoveInto(tmpvec);
      *vx = xsign * tmpvec.x;
      *vy = ysign * tmpvec.y;
      *vz = zsign * tmpvec.z;
      boundary.Append(vtemp);
    }
  } else {
    // This is the more usual case.  Construct the boundary
    // from the data.
    FillDataBoundaryList(boundary); // NB: This routine requires
    /// initialization complete enough so that GetPreciseRange
    /// returns the correct values.
  }

  // Allocate new grid.  Note that Vf_GridVec3f stores
  // data in grid with zdim as first (slowest) index,
  // xdim as last (fastest) index.
  if(new_xdim>0 && new_ydim>0 && new_zdim>0) {
    grid.Allocate(new_zdim,new_ydim,new_xdim);
    // Fill new grid from import_mesh, stepping in the natural order
    // through the new grid, flipped order through import_mesh
    OC_INDEX i,j,k;
    OC_INDEX abs_xstep = isubsample*(xstep<0 ? -1*xstep : xstep);
    OC_INDEX abs_ystep = isubsample*(ystep<0 ? -1*ystep : ystep);
    OC_INDEX abs_zstep = isubsample*(zstep<0 ? -1*zstep : zstep);
    for(k=0,*iz=*izstart;k<new_zdim;k++,*iz+=abs_zstep) {
      for(j=0,*iy=*iystart;j<new_ydim;j++,*iy+=abs_ystep) {
        for(i=0,*ix=*ixstart;i<new_xdim;i++,*ix+=abs_xstep) {
          const Nb_Vec3<OC_REAL8>& pt
            = import_mesh.GridVec(xindex,yindex,zindex);
          *vx = xsign*pt.x;
          *vy = ysign*pt.y;
          *vz = zsign*pt.z;
          GridVec(i,j,k) = vtemp;
        }
      }
    }
  }
#undef MEMBERNAME
}


// Copy constructor with periodic translation.
Vf_GridVec3f::Vf_GridVec3f
(const Vf_GridVec3f& import_mesh,
 OC_INDEX ioff,OC_INDEX joff,OC_INDEX koff)
  : Vf_Mesh(import_mesh)
{
#define MEMBERNAME "Constructor_copy_with_periodic_translation"
  data_range=import_mesh.data_range;
  range=import_mesh.range;
  boundary=import_mesh.boundary;
  coords_base=import_mesh.coords_base;
  coords_step=import_mesh.coords_step;

  // Make translated copy of import_mesh
  OC_INDEX idim,jdim,kdim;
  import_mesh.grid.GetSize(kdim,jdim,idim);
  grid.Allocate(kdim,jdim,idim);
  ioff %= idim; if(ioff<0) ioff += idim;
  joff %= jdim; if(joff<0) joff += jdim;
  koff %= kdim; if(koff<0) koff += kdim;

  OC_INDEX imid = idim-ioff;
  for(OC_INDEX k=0;k<kdim;++k) {
    OC_INDEX k2 = k + koff; if(k2>=kdim) k2 -= kdim;
    for(OC_INDEX j=0;j<jdim;++j) {
      OC_INDEX j2 = j + joff; if(j2>=jdim) j2 -= jdim;
      OC_INDEX i;
      for(i=0;i<imid;++i) {
        grid(k2,j2,i+ioff) = import_mesh.grid(k,j,i);
      }
      for(i=imid;i<idim;++i) {
        grid(k2,j2,i-imid) = import_mesh.grid(k,j,i);
      }
    }
  }
#undef MEMBERNAME
}

// Copy with resampling from another mesh.  The new mesh has extent
// described by import newrange.  The number of cells along the x, y,
// and z axes are icount, jcount, and kcount, respectively.
void
Vf_GridVec3f::ResampleCopy
(const Vf_GridVec3f& import_mesh,
 const Nb_BoundingBox<OC_REAL8> &newrange,
 OC_INDEX icount,OC_INDEX jcount,OC_INDEX kcount,
 int method_order)
{
#define MEMBERNAME "ResampleCopy"
  if(icount<1 || jcount<1 || kcount<1) {
    FatalError(-1,STDDOC,"%s: icount=%ld, jcount=%ld, kcount=%ld",
               ErrBadParam,long(icount),long(jcount),long(kcount));
  }
  if(method_order != 0 && method_order != 1 && method_order != 3) {
    FatalError(-1,STDDOC,"%s: method_order=%d (should be 0, 1 or 3)",
               ErrBadParam,method_order);
  }
  assert(import_mesh.coords_step.x>0
         && import_mesh.coords_step.y>0
         && import_mesh.coords_step.z>0);

  // Collect grid data from import mesh.  This allows import_mesh
  // and *this to be the same mesh.
  OC_INDEX i2max,j2max,k2max;
  import_mesh.GetDimens(i2max,j2max,k2max); --i2max; --j2max; --k2max;
  Nb_Vec3<OC_REAL8> import_coords_base = import_mesh.coords_base;
  Nb_Vec3<OC_REAL8> import_coords_step = import_mesh.coords_step;

  // Copy Vf_Mesh base from import_mesh
  SetFilename(import_mesh.GetName());
  SetTitle(import_mesh.GetTitle());
  SetDescription(import_mesh.GetDescription());
  SetMeshUnit(import_mesh.GetMeshUnit());
  SetValueUnit(import_mesh.GetValueUnit());
  SetDisplayValueScale(import_mesh.GetDisplayValueScale());
  ValueMultiplier = import_mesh.GetValueMultiplier();
  import_mesh.GetMagHints(MinMagHint,MaxMagHint);

  // Initialize non-data portions of new grid
  range = newrange;
  coords_step.x = range.GetWidth()/icount;
  coords_step.y = range.GetHeight()/jcount;
  coords_step.z = range.GetDepth()/kcount;
  range.GetMinPt(coords_base);
  coords_base += 0.5*coords_step;

  Nb_Vec3<OC_REAL8> maxpt;
  range.GetMaxPt(maxpt);
  maxpt -= 0.5*coords_step;
  data_range.Set(coords_base,maxpt);

  boundary_from_data = 1;
  FillDataBoundaryList(boundary); // NB: This routine requires
  /// that range be already set so that GetPreciseRange returns the
  /// correct values.

  // Fill new grid by sampling other grid, using 0th-order (closest
  // point) fit.
  OC_REAL8 offx
    = (coords_base.x-import_coords_base.x)/import_coords_step.x;
  OC_REAL8 offy
    = (coords_base.y-import_coords_base.y)/import_coords_step.y;
  OC_REAL8 offz
    = (coords_base.z-import_coords_base.z)/import_coords_step.z;
  OC_REAL8 sx = coords_step.x/import_coords_step.x;
  OC_REAL8 sy = coords_step.y/import_coords_step.y;
  OC_REAL8 sz = coords_step.z/import_coords_step.z;

  // i<icount on the new mesh maps to a point x2(i) on the import mesh
  // such that 0 <= x2(i) <=i2max iff ilow <= i < ihigh; in other words,
  // i<ilow implies x2(i)<0 and i>=ihigh implies x2(i)>i2max.
  const OC_INDEX ilow  = OC_MIN(OC_INDEX(ceil((-offx)/sx)),icount);
  const OC_INDEX ihigh = OC_MIN(OC_INDEX(floor((i2max - offx)/sx))+1,icount);

  Nb_Array3D< Nb_Vec3<OC_REAL8> > dummy_grid;
  const Nb_Array3D< Nb_Vec3<OC_REAL8> >* import_grid = &(import_mesh.grid);
  if(grid.IsSameArray(import_mesh.grid)) {
    // Resampling on same grid; create holding space
    grid.Swap(dummy_grid);
    import_grid = &dummy_grid;
  }
  // NB: Coordinates are reversed when accessing grid directly!
  grid.Allocate(kcount,jcount,icount);
  if(method_order==0) {
    // Nearest neighbor "interpolation"
    for(OC_INDEX k=0;k<kcount;++k) {
      OC_INDEX k2 = OC_INDEX(OC_ROUND(offz + k*sz));
      k2 = (k2<0 ? 0 : (k2>k2max ? k2max : k2));
      for(OC_INDEX j=0;j<jcount;++j) {
        OC_INDEX j2 = OC_INDEX(OC_ROUND(offy + j*sy));
        j2 = (j2<0 ? 0 : (j2>j2max ? j2max : j2));
        OC_INDEX i = 0;
        for(;i<ilow;++i) {
          grid(k,j,i) = (*import_grid)(k2,j2,0);
        }
        for(;i<ihigh;++i) {
          OC_INDEX i2 = OC_INDEX(OC_ROUND(offx + i*sx));
          grid(k,j,i) = (*import_grid)(k2,j2,i2);
        }
        for(;i<icount;++i) {
          grid(k,j,i) = (*import_grid)(k2,j2,i2max);
        }
      }
    }
  } else if(method_order==1) {
    // Trilinear interpolation
    Nb_Vec3<OC_REAL8> w000,w100,w001,w101,w010,w110,w011,w111;
    Nb_Vec3<OC_REAL8> v00,v01,v10,v11;
    Nb_Vec3<OC_REAL8> u0,u1;

    for(OC_INDEX k=0;k<kcount;++k) {
      OC_REAL8 z = offz + k*sz;
      OC_INDEX k2a = OC_INDEX(floor(z));
      OC_INDEX k2b = k2a + 1;
      if(k2a<0)          { k2a = 0;     k2b = OC_MIN(1,k2max); }
      else if(k2b>k2max) { k2b = k2max; k2a = OC_MAX(0,k2max-1); }
      OC_REAL8 tz = z - k2a;
      tz = (tz<-0.5 ? -0.5 : (tz>1.5 ? 1.5 : tz)); // Limit extrapolation

      for(OC_INDEX j=0;j<jcount;++j) {
        OC_REAL8 y = offy + j*sy;
        OC_INDEX j2a = OC_INDEX(floor(y));
        OC_INDEX j2b = j2a + 1;
        if(j2a<0)          { j2a = 0;     j2b = OC_MIN(1,j2max); }
        else if(j2b>j2max) { j2b = j2max; j2a = OC_MAX(0,j2max-1); }
        OC_REAL8 ty = y - j2a;
        ty = (ty<-0.5 ? -0.5 : (ty>1.5 ? 1.5 : ty));
        OC_INDEX i2a = 0;
        OC_INDEX i2b = OC_MIN(1,i2max);
        OC_INDEX i=0;
        if(i<ilow) {
          // Off left edge of import mesh; x-interpolation uses i2 = 0 and 1
          w000 = (*import_grid)(k2a,j2a,i2a);
          w100 = (*import_grid)(k2b,j2a,i2a);
          v00 = (1.-tz)*w000 + tz*w100;
          w001 = (*import_grid)(k2a,j2a,i2b);
          w101 = (*import_grid)(k2b,j2a,i2b);
          v01 = (1.-tz)*w001 + tz*w101;
          w010 = (*import_grid)(k2a,j2b,i2a);
          w110 = (*import_grid)(k2b,j2b,i2a);
          v10 = (1.-tz)*w010 + tz*w110;
          w011 = (*import_grid)(k2a,j2b,i2b);
          w111 = (*import_grid)(k2b,j2b,i2b);
          v11 = (1.-tz)*w011 + tz*w111;
          u0 = (1.-ty)*v00 + ty*v10;
          u1 = (1.-ty)*v01 + ty*v11;
          for(;i<ilow;++i) {
            OC_REAL8 tx = OC_MAX(offx + i*sx,-0.5); // Limit extrapolation
            grid(k,j,i) = (1.-tx)*u0 + tx*u1;
          }
        }
        for(;i<ihigh-1;++i) { // x coord overlaps import mesh
          OC_REAL8 x = offx + i*sx;
          i2a = OC_INDEX(floor(x));
          i2b = i2a + 1;
          OC_REAL8 tx = x - i2a;
          w000 = (*import_grid)(k2a,j2a,i2a);
          w100 = (*import_grid)(k2b,j2a,i2a);
          v00 = (1.-tz)*w000 + tz*w100;
          w001 = (*import_grid)(k2a,j2a,i2b);
          w101 = (*import_grid)(k2b,j2a,i2b);
          v01 = (1.-tz)*w001 + tz*w101;
          w010 = (*import_grid)(k2a,j2b,i2a);
          w110 = (*import_grid)(k2b,j2b,i2a);
          v10 = (1.-tz)*w010 + tz*w110;
          w011 = (*import_grid)(k2a,j2b,i2b);
          w111 = (*import_grid)(k2b,j2b,i2b);
          v11 = (1.-tz)*w011 + tz*w111;
          u0 = (1.-ty)*v00 + ty*v10;
          u1 = (1.-ty)*v01 + ty*v11;
          grid(k,j,i) = (1.-tx)*u0 + tx*u1;
        }
        if(i<icount) {
          // Off right edge of import mesh; x-interpolation uses
          // i2 = i2max-1 and i2max
          i2a = OC_MAX(0,i2max - 1);
          i2b = i2max;
          w000 = (*import_grid)(k2a,j2a,i2a);
          w100 = (*import_grid)(k2b,j2a,i2a);
          v00 = (1.-tz)*w000 + tz*w100;
          w001 = (*import_grid)(k2a,j2a,i2b);
          w101 = (*import_grid)(k2b,j2a,i2b);
          v01 = (1.-tz)*w001 + tz*w101;
          w010 = (*import_grid)(k2a,j2b,i2a);
          w110 = (*import_grid)(k2b,j2b,i2a);
          v10 = (1.-tz)*w010 + tz*w110;
          w011 = (*import_grid)(k2a,j2b,i2b);
          w111 = (*import_grid)(k2b,j2b,i2b);
          v11 = (1.-tz)*w011 + tz*w111;
          u0 = (1.-ty)*v00 + ty*v10;
          u1 = (1.-ty)*v01 + ty*v11;
          for(;i<icount;++i) {
            OC_REAL8 tx = OC_MIN(offx - i2a + i*sx,1.5);
            /// Limit extrapolation
            grid(k,j,i) = (1.-tx)*u0 + tx*u1;
          }
        }
      }
    }
  } else {
    // Tricubic interpolation.  Two options, controlled by
    // #if/#else/#endif.  Original option uses interpolating cubics
    // through four equally spaced points (with panel shifting).  The
    // alternative uses Catmull–Rom splines in the interior, and an
    // interpolating quadratic at the boundaries.  The former provides
    // a continuous interpolation but the first derivative is in
    // general discontinuous across nodes.  The latter is C^1.
    Nb_Vec3<OC_REAL8> q[4];
    Nb_Vec3<OC_REAL8> qq[4][4];
    for(OC_INDEX k=0;k<kcount;++k) {
      OC_INDEX k2[4];
      OC_REAL8 zwa,zwb,zwc,zwd;
      OC_REAL8 z = offz + k*sz;
      k2[0] = OC_INDEX(floor(z))-1;
      if(k2max>=3) { // Cubic interpolation
        k2[3] = k2[0] + 3;
        if(k2[0]<0)          { k2[0] = 0;     k2[3] = 3; }
        else if(k2[3]>k2max) { k2[3] = k2max; k2[0] = k2max-3; }
        k2[1] = k2[0]+1;  k2[2] = k2[0]+2;
        OC_REAL8 tz = z - k2[1];
        tz = (tz<-1.5 ? -1.5 : (tz>2.5 ? 2.5 : tz)); // Limit extrapolation
#if 0 // Cubic interpolation
        zwa = (-1./6.)*tz*(tz-1)*(tz-2);
        zwb = (0.5)*(tz+1)*(tz-1)*(tz-2);
        zwc = (-0.5)*(tz+1)*tz*(tz-2);
        zwd = (1./6.)*(tz+1)*tz*(tz-1);
#else // Spline interpolation
        if(tz<0) {
          // At left edge; use quadratic interpolation
          zwa = 0.5*tz*(tz-1.0);
          zwb = (1+tz)*(1.0-tz);
          zwc = 0.5*tz*(1.0+tz);
          zwd = 0.0;
        } else if(tz>1) {
          // At right edge; use quadratic interpolation
          zwa = 0.0;
          zwb = 0.5*(tz-1.0)*(tz-2.0);
          zwc = tz*(2.0-tz);
          zwd = 0.5*tz*(tz-1.0);
        } else {
          // Catmull-Rom cubic spline
          // Note: The first derivative of the preceding left/right
          // edge quadratic interpolations match the first derivative
          // of the adjoining Catmull-Rom cubic spline.
          zwa = 0.5*(tz*((2-tz)*tz-1));
          zwb = 0.5*(tz*tz*(3*tz-5)+2);
          zwc = 0.5*(tz*((4-3*tz)*tz+1));
          zwd = 0.5*tz*tz*(tz-1);
        }
#endif // Cubic/spline interpolation
      } else if(k2max>=1) { // Linear interpolation
        k2[3] = (++k2[0]) + 1;
        if(k2[0]<0)          { k2[0] = 0;     k2[3] = OC_MIN(1,k2max); }
        else if(k2[3]>k2max) { k2[3] = k2max; k2[0] = OC_MAX(0,k2max-1); }
        k2[1] = k2[0];    k2[2] = k2[3];
        OC_REAL8 tz = z - k2[1];
        tz = (tz<-0.5 ? -0.5 : (tz>1.5 ? 1.5 : tz)); // Limit extrapolation
        zwb = 1.0-tz;
        zwc = tz;
        zwa = zwd = 0.0;
      } else { // Single point in z direction; use constant value
        k2[0] = k2[1] = k2[2] = k2[3] = 0;
        zwb = 1.0;
        zwa = zwc = zwd = 0.0;
      }

      for(OC_INDEX j=0;j<jcount;++j) {
        OC_INDEX j2[4];
        OC_REAL8 ywa,ywb,ywc,ywd;
        OC_REAL8 y = offy + j*sy;
        j2[0] = OC_INDEX(floor(y))-1;
        if(j2max>=3) { // Cubic interpolation
          j2[3] = j2[0] + 3;
          if(j2[0]<0)          { j2[0] = 0;     j2[3] = OC_MIN(3,j2max); }
          else if(j2[3]>j2max) { j2[3] = j2max; j2[0] = OC_MAX(0,j2max-3); }
          j2[1] = j2[0]+1;  j2[2] = j2[0]+2;
          OC_REAL8 ty = y - j2[1];
          ty = (ty<-1.5 ? -1.5 : (ty>2.5 ? 2.5 : ty)); // Limit extrapolation
#if 0 // Cubic interpolation
          ywa = (-1./6.)*ty*(ty-1)*(ty-2);
          ywb = (0.5)*(ty+1)*(ty-1)*(ty-2);
          ywc = (-0.5)*(ty+1)*ty*(ty-2);
          ywd = (1./6.)*(ty+1)*ty*(ty-1);
#else // Spline interpolation
          if(ty<0) { // Left edge; use quadratic interpolation
            ywa = 0.5*ty*(ty-1.0);
            ywb = (1+ty)*(1.0-ty);
            ywc = 0.5*ty*(1.0+ty);
            ywd = 0.0;
          } else if(ty>1) { // Right edge; use quadratic interpolation
            ywa = 0.0;
            ywb = 0.5*(ty-1.0)*(ty-2.0);
            ywc = ty*(2.0-ty);
            ywd = 0.5*ty*(ty-1.0);
          } else { // Catmull-Rom cubic spline
            ywa = 0.5*(ty*((2-ty)*ty-1));
            ywb = 0.5*(ty*ty*(3*ty-5)+2);
            ywc = 0.5*(ty*((4-3*ty)*ty+1));
            ywd = 0.5*ty*ty*(ty-1);
          }
#endif // Cubic/spline interpolation
        } else if(j2max>=1) { // Linear interpolation
          j2[3] = (++j2[0]) + 1;
          if(j2[0]<0)          { j2[0] = 0;     j2[3] = OC_MIN(1,j2max); }
          else if(j2[3]>j2max) { j2[3] = j2max; j2[0] = OC_MAX(0,j2max-1); }
          j2[1] = j2[0];    j2[2] = j2[3];
          OC_REAL8 ty = y - j2[1];
          ty = (ty<-0.5 ? -0.5 : (ty>1.5 ? 1.5 : ty)); // Limit extrapolation
          ywb = 1.0-ty;
          ywc = ty;
          ywa = ywd = 0.0;
        } else { // Single point in y direction; use constant value
          j2[0] = j2[1] = j2[2] = j2[3] = 0;
          ywb = 1.0;
          ywa = ywc = ywd = 0.0;
        }

        for(OC_INDEX i=0;i<icount;++i) {
          OC_INDEX i2[4];
          OC_REAL8 xwa,xwb,xwc,xwd;
          OC_REAL8 x = offx + i*sx;
          i2[0] = OC_INDEX(floor(x))-1;
          if(i2max>=3) { // Cubic interpolation
            i2[3] = i2[0] + 3;
            if(i2[0]<0)          { i2[0] = 0;     i2[3] = OC_MIN(3,i2max); }
            else if(i2[3]>i2max) { i2[3] = i2max; i2[0] = OC_MAX(0,i2max-3); }
            i2[1] = i2[0]+1;  i2[2] = i2[0]+2;
            OC_REAL8 tx = x - i2[1];
            tx = (tx<-1.5 ? -1.5 : (tx>2.5 ? 2.5 : tx)); // Limit extrapolation
#if 0 // Cubic interpolation
            xwa = (-1./6.)*tx*(tx-1)*(tx-2);
            xwb = (0.5)*(tx+1)*(tx-1)*(tx-2);
            xwc = (-0.5)*(tx+1)*tx*(tx-2);
            xwd = (1./6.)*(tx+1)*tx*(tx-1);
#else // Spline interpolation
            if(tx<0) { // Left edge; use quadratic interpolation
              xwa = 0.5*tx*(tx-1.0);
              xwb = (1+tx)*(1.0-tx);
              xwc = 0.5*tx*(1.0+tx);
              xwd = 0.0;
            } else if(tx>1) { // Right edge; use quadratic interpolation
              xwa = 0.0;
              xwb = 0.5*(tx-1.0)*(tx-2.0);
              xwc = tx*(2.0-tx);
              xwd = 0.5*tx*(tx-1.0);
            } else { // Catmull-Rom cubic spline
              xwa = 0.5*(tx*((2-tx)*tx-1));
              xwb = 0.5*(tx*tx*(3*tx-5)+2);
              xwc = 0.5*(tx*((4-3*tx)*tx+1));
              xwd = 0.5*tx*tx*(tx-1);
            }
#endif // Cubic/spline interpolation
          } else if(i2max>=1) { // Linear interpolation
            i2[3] = (++i2[0]) + 1;
            if(i2[0]<0)          { i2[0] = 0;     i2[3] = OC_MIN(1,i2max); }
            else if(i2[3]>i2max) { i2[3] = i2max; i2[0] = OC_MAX(0,i2max-1); }
            i2[1] = i2[0];    i2[2] = i2[3];
            OC_REAL8 tx = x - i2[1];
            tx = (tx<-0.5 ? -0.5 : (tx>1.5 ? 1.5 : tx)); // Limit extrapolation
            xwb = 1.0 - tx;
            xwc = tx;
            xwa = xwd = 0.0;
          } else { // Single point in x direction; use constant value
            i2[0] = i2[1] = i2[2] = i2[3] = 0;
            xwb = 1.0;
            xwa = xwc = xwd = 0.0;
          }

          // First do cubic interpolations along z
          OC_INDEX ii,jj;
          for(jj=0;jj<4;++jj) {
            for(ii=0;ii<4;++ii) {
              qq[ii][jj]
                = zwa*(*import_grid)(k2[0],j2[jj],i2[ii])
                + zwb*(*import_grid)(k2[1],j2[jj],i2[ii])
                + zwc*(*import_grid)(k2[2],j2[jj],i2[ii])
                + zwd*(*import_grid)(k2[3],j2[jj],i2[ii]);
            }
          }

          // Next cubic interpolations along y
          for(ii=0;ii<4;++ii) {
            q[ii] = ywa*qq[ii][0] + ywb*qq[ii][1]
                  + ywc*qq[ii][2] + ywd*qq[ii][3];
          }

          // Finally, cubic interpolation along x
          grid(k,j,i) = xwa*q[0] + xwb*q[1] + xwc*q[2] + xwd*q[3];
        }
      }
    }
  }

  // dummy_grid is automatically freed on exit
#undef MEMBERNAME
}


// ResampleCopyAverage is similar to ResampleCopy, except rather than
// resampling through samples on a fit curve, instead each resampled
// value is the average value from the cells in source mesh lying in
// the footprint of the resample cell.
void
Vf_GridVec3f::ResampleCopyAverage
(const Vf_GridVec3f& import_mesh,
 const Nb_BoundingBox<OC_REAL8> &newrange,
 OC_INDEX icount,OC_INDEX jcount,OC_INDEX kcount)
{
#define MEMBERNAME "ResampleCopyAverage"
  if(icount<1 || jcount<1 || kcount<1) {
    FatalError(-1,STDDOC,"%s: icount=%ld, jcount=%ld, kcount=%ld",
               ErrBadParam,long(icount),long(jcount),long(kcount));
  }
  assert(import_mesh.coords_step.x>0
         && import_mesh.coords_step.y>0
         && import_mesh.coords_step.z>0);

  // Collect grid data from import mesh.  This allows import_mesh
  // and *this to be the same mesh.
  OC_INDEX i2max,j2max,k2max;
  import_mesh.GetDimens(i2max,j2max,k2max); --i2max; --j2max; --k2max;
  Nb_Vec3<OC_REAL8> import_coords_base = import_mesh.coords_base;
  Nb_Vec3<OC_REAL8> import_coords_step = import_mesh.coords_step;

  // Copy Vf_Mesh base from import_mesh
  SetFilename(import_mesh.GetName());
  SetTitle(import_mesh.GetTitle());
  SetDescription(import_mesh.GetDescription());
  SetMeshUnit(import_mesh.GetMeshUnit());
  SetValueUnit(import_mesh.GetValueUnit());
  SetDisplayValueScale(import_mesh.GetDisplayValueScale());
  ValueMultiplier = import_mesh.GetValueMultiplier();
  import_mesh.GetMagHints(MinMagHint,MaxMagHint);

  // Initialize non-data portions of new grid
  range = newrange;
  coords_step.x = range.GetWidth()/icount;
  coords_step.y = range.GetHeight()/jcount;
  coords_step.z = range.GetDepth()/kcount;
  range.GetMinPt(coords_base);
  coords_base += 0.5*coords_step;

  Nb_Vec3<OC_REAL8> maxpt;
  range.GetMaxPt(maxpt);
  maxpt -= 0.5*coords_step;
  data_range.Set(coords_base,maxpt);

  boundary_from_data = 1;
  FillDataBoundaryList(boundary); // NB: This routine requires
  /// that range be already set so that GetPreciseRange returns the
  /// correct values.

  OC_REAL8 offx
    = (coords_base.x-import_coords_base.x)/import_coords_step.x;
  OC_REAL8 offy
    = (coords_base.y-import_coords_base.y)/import_coords_step.y;
  OC_REAL8 offz
    = (coords_base.z-import_coords_base.z)/import_coords_step.z;
  OC_REAL8 sx = coords_step.x/import_coords_step.x;
  OC_REAL8 sy = coords_step.y/import_coords_step.y;
  OC_REAL8 sz = coords_step.z/import_coords_step.z;

  Nb_Array3D< Nb_Vec3<OC_REAL8> > dummy_grid;
  const Nb_Array3D< Nb_Vec3<OC_REAL8> >* import_grid = &(import_mesh.grid);
  if(grid.IsSameArray(import_mesh.grid)) {
    // Resampling on same grid; create holding space
    grid.Swap(dummy_grid);
    import_grid = &dummy_grid;
  }
  // NB: Coordinates are reversed when accessing grid directly!
  grid.Allocate(kcount,jcount,icount);

  for(OC_INDEX k=0;k<kcount;++k) {
    OC_REAL8 k2a = OC_MAX(-0.5,offz + (k-0.5)*sz);
    OC_INDEX k2start = OC_MAX(0,OC_INDEX(OC_ROUND(k2a)));
    OC_REAL8 kawgt = (k2start+0.5)-k2a;

    OC_REAL8 k2b = OC_MIN(offz + (k+0.5)*sz,k2max+0.5);
    OC_INDEX k2stop = OC_MIN(OC_INDEX(OC_ROUND(k2b)),k2max);
    OC_REAL8 kbwgt = k2b-(k2stop-0.5);
    if(k2start == k2stop) { kawgt = kbwgt = k2b-k2a; } // 1 cell

    for(OC_INDEX j=0;j<jcount;++j) {
      OC_REAL8 j2a = OC_MAX(-0.5,offy + (j-0.5)*sy);
      OC_INDEX j2start = OC_MAX(0,OC_INDEX(OC_ROUND(j2a)));
      OC_REAL8 jawgt = (j2start+0.5)-j2a;

      OC_REAL8 j2b = OC_MIN(offy + (j+0.5)*sy,j2max+0.5);
      OC_INDEX j2stop = OC_MIN(OC_INDEX(OC_ROUND(j2b)),j2max);
      OC_REAL8 jbwgt = j2b-(j2stop-0.5);
      if(j2start == j2stop) { jawgt = jbwgt = j2b-j2a; } // 1 cell

      for(OC_INDEX i=0;i<icount;++i) {
        OC_REAL8 i2a = OC_MAX(-0.5,offx + (i-0.5)*sx);
        OC_INDEX i2start = OC_MAX(0,OC_INDEX(OC_ROUND(i2a)));
        OC_REAL8 iawgt = (i2start+0.5)-i2a;

        OC_REAL8 i2b = OC_MIN(offx + (i+0.5)*sx,i2max+0.5);
        OC_INDEX i2stop = OC_MIN(OC_INDEX(OC_ROUND(i2b)),i2max);
        OC_REAL8 ibwgt = i2b-(i2stop-0.5);
        if(i2start == i2stop) { iawgt = ibwgt = i2b-i2a; } // 1 cell

        Nb_Vec3<OC_REAL8> sum(0.0,0.0,0.0);
        OC_REAL8 zwgt,yzwgt,xyzwgt;
        for(OC_INDEX k2=k2start;k2<=k2stop;++k2) {
          if(k2==k2start)     zwgt = kawgt;
          else if(k2==k2stop) zwgt = kbwgt;
          else                zwgt = 1.0;
          for(OC_INDEX j2=j2start;j2<=j2stop;++j2) {
            if(j2==j2start)     yzwgt = jawgt*zwgt;
            else if(j2==j2stop) yzwgt = jbwgt*zwgt;
            else                yzwgt = zwgt;
            for(OC_INDEX i2=i2start;i2<=i2stop;++i2) {
              if(i2==i2start)     xyzwgt = iawgt*yzwgt;
              else if(i2==i2stop) xyzwgt = ibwgt*yzwgt;
              else                xyzwgt = yzwgt;
              sum += xyzwgt * ((*import_grid)(k2,j2,i2));
            }
          }
        }
        sum *= 1.0/((k2b-k2a)*(j2b-j2a)*(i2b-i2a));
        grid(k,j,i) = sum;
      }
    }
  }

  // dummy_grid is automatically freed on exit
#undef MEMBERNAME
}

//////////////////////////////////////////////////////////////////////////
// General (non-regular) 3D mesh of Nb_Vec3<OC_REAL4>'s
const ClassDoc Vf_GeneralMesh3f::class_doc("Vf_GeneralMesh3f",
                    "Michael J. Donahue (michael.donahue@nist.gov)",
                    "1.0.0","1-Sep-1997");

const OC_INDEX Vf_GeneralMesh3f::zslice_count=100;  // Number of zslices.

// Generic Mesh functions /////////////////////
void
Vf_GeneralMesh3f::GetPreciseRange(Nb_BoundingBox<OC_REAL8> &myrange) const
{
  myrange = range;
}

void Vf_GeneralMesh3f::UpdateRange()
{
  if(boundary_from_data && !boundary_range.Contains(data_range)) {
    Nb_Vec3<OC_REAL8> minpt,maxpt;
    data_range.GetExtremes(minpt,maxpt);
    Nb_Vec3<OC_REAL8> margin=step_hints;
    margin*=0.5;
    minpt -= margin;
    maxpt += margin;
    range.Set(minpt,maxpt);
  } else {
    range=data_range;
    range.ExpandWith(boundary_range);
  }
}

void
Vf_GeneralMesh3f::GetPreciseDataRange(Nb_BoundingBox<OC_REAL8> &myrange) const
{
  myrange=data_range;
}

void
Vf_GeneralMesh3f::GetPreciseBoundaryList
(Nb_List< Nb_Vec3<OC_REAL8> > &boundary_list) const {
  if(boundary_from_data) {
    FillDataBoundaryList(boundary_list); // NB: FillDataBoundaryList is
    /// a routine in the parent class, which will turn around and call
    /// GetPreciseRange in this child class to get the information
    /// needed to construct the boundary.
  } else {
    boundary_list=boundary;
  }
}

OC_INDEX Vf_GeneralMesh3f::ColorQuantityTypes(Nb_List<Nb_DString> &types) const
{
  types.Clear();
  types.Append(Nb_DString("x"));
  types.Append(Nb_DString("y"));
  types.Append(Nb_DString("z"));
  types.Append(Nb_DString("slice"));
  types.Append(Nb_DString("mag"));
  types.Append(Nb_DString("xy-angle"));
  types.Append(Nb_DString("xz-angle"));
  types.Append(Nb_DString("yz-angle"));
  types.Append(Nb_DString("none"));
  return types.GetSize();
}

OC_BOOL Vf_GeneralMesh3f::ColorQuantityTransform
(const Nb_DString flipstr,
 const Nb_DString& quantity_in,OC_REAL8m phase_in,OC_BOOL invert_in,
 Nb_DString& quantity_out,OC_REAL8m& phase_out,OC_BOOL& invert_out) const
{
  return ColorQuantityTransformHelper(flipstr,
                                      quantity_in,phase_in,invert_in,
                                      quantity_out,phase_out,invert_out);
}

OC_REAL4m Vf_GeneralMesh3f::GetVecShade(const char* colorquantity,
                                     OC_REAL8m phase,OC_BOOL invert,
                                     const Nb_Vec3<OC_REAL4>& v) const
{// NOTE: The input vector v is assumed to be pre-scaled, i.e., no
 //  DisplayValueScale or ValueMultiplier is applied inside this routine.
 //  MagHints are used, however.
  OC_REAL8m shade = 0.5;

  if(strcmp("xy-angle",colorquantity)==0) {
    if(v.x==0. && v.y==0.) {
      if(invert) shade = 1. - phase;
      else       shade = -phase;
    } else {
      shade = Oc_Atan2((double)v.y,(double)v.x)*(1./(2*PI));
      shade = shade - floor(shade+phase);  // Force into [0,1)
    }
  } else if(strcmp("xz-angle",colorquantity)==0) {
    if(v.x==0. && v.z==0.) {
      if(invert) shade = 1. - phase;
      else       shade = -phase;
    } else {
      shade = Oc_Atan2((double)v.z,(double)v.x)*(1./(2*PI));
      shade = shade - floor(shade+phase);  // Force into [0,1)
    }
  } else if(strcmp("yz-angle",colorquantity)==0) {
    if(v.y==0. && v.z==0.) {
      if(invert) shade = 1. - phase;
      else       shade = -phase;
    } else {
      shade = Oc_Atan2((double)v.z,(double)v.y)*(1./(2*PI));
      shade = shade - floor(shade+phase);  // Force into [0,1)
    }
  } else if(strcmp("slice",colorquantity)==0) {
    // Slice color doesn't depend on vector components,
    // but rather position.  Technically, we should
    // probably return 0.5 or -1 here, but as a convenience
    // to calls from the coords display, we'll treat v here
    // as referring to the position, and furthermore expand
    // to maximum range
    if(v.z<0)      shade=0.0;
    else if(v.z>0) shade=1.0;
    else           shade=0.5;
  } else {
    OC_REAL8m val=0.0;
    if(strcmp("x",colorquantity)==0)        val = v.x;
    else if(strcmp("y",colorquantity)==0)   val = v.y;
    else if(strcmp("z",colorquantity)==0)   val = v.z;
    else                                    return -1;

    OC_REAL8m minsize=0.0,maxsize=1.0;
    if(MaxMagHint<1.0 || GetDisplayValueScale()<DBL_MAX/MaxMagHint) {
      maxsize = GetDisplayValueScale()*MaxMagHint;
    }
    if(MaxMagHint>MinMagHint) {
      minsize = maxsize*MinMagHint/MaxMagHint;
    }

    if(minsize>0.0 && fabs(v.x)<minsize
       && fabs(v.y)<minsize && fabs(v.z)<minsize) {
      // Overflow-protected min size check
      OC_REAL8m tmp,sum=0.0;
      tmp = v.x/minsize; sum += tmp*tmp;
      tmp = v.y/minsize; sum += tmp*tmp;
      tmp = v.z/minsize; sum += tmp*tmp;
      if(sum<1.0) return -1;
    }

    if(fabs(val)<maxsize) shade = (1.+val/maxsize)/2.;
    else if(val>0.)   shade = 1.0;
    else              shade = 0.0;
  }

  shade+=phase;
  if(shade>1.0)      shade=1.0;
  else if(shade<0.0) shade=0.0;
  if(invert) shade = 1.0 - shade;

  return static_cast<OC_REAL4m>(shade);
}


OC_BOOL Vf_GeneralMesh3f::FindPreciseClosest(const Nb_Vec3<OC_REAL8> &pos,
                                             Nb_LocatedVector<OC_REAL8> &lv)
{
  if(GetSize()<1) return 1; // Empty mesh
  OC_INT4m dummy;
  Nb_Vec3<OC_REAL8> work_pos=pos;
  Nb_LocatedVector<OC_REAL8> *lvp;
  data_range.MoveInto(work_pos);
  OC_BOOL errorcode=mesh.GetClosest2D(work_pos,lvp,dummy);
  if(errorcode==0) {
    lv=*lvp;
    if(!range.IsIn(pos)) errorcode = 1;
  }
  return errorcode;
}

void Vf_GeneralMesh3f::GetZsliceParameters(OC_REAL8m& zslicebase,
                                           OC_REAL8m& zstep)
{
  Nb_Vec3<OC_REAL8> tempmin,tempmax;
  data_range.GetExtremes(tempmin,tempmax);
  OC_REAL8m zrangespan = tempmax.z-tempmin.z;
  zslicebase = tempmin.z;
  zstep = zrangespan/zslice_count;
}

void Vf_GeneralMesh3f::GetZslice(OC_REAL8m zlow,OC_REAL8m zhigh,
                                 OC_INDEX& islicelow,OC_INDEX& islicehigh)
{ // Coordinate this with the method used in GetDisplayList()
  // The exports should be interpreted as including islicelow,
  // and everything above up to but not including islicehigh.
  Nb_Vec3<OC_REAL8> tempmin,tempmax;
  data_range.GetExtremes(tempmin,tempmax);
  OC_REAL8m zrangespan = tempmax.z-tempmin.z;

  if(zrangespan==0.0) {
    // Special case; all data vectors are in the same slice,
    // namely 0.
    islicelow=0;
    if(zlow<=tempmin.z && zhigh>tempmax.z) islicehigh=1; // All
    else                                   islicehigh=0; // None
    return;
  }

  // Bound import range, to protect against integer overflow below
  if(zlow<=tempmin.z-zrangespan) {
    zlow=tempmin.z-zrangespan;
    if(zhigh<zlow) zhigh=zlow;
  }
  if(zhigh>tempmax.z+zrangespan) {
    zhigh=tempmax.z+zrangespan;
    if(zlow>zhigh) zlow=zhigh;
  }

  islicelow =
    static_cast<OC_INDEX>(floor(zslice_count*(zlow-tempmin.z)
                             /zrangespan));
  islicehigh =
    static_cast<OC_INDEX>(ceil(zslice_count*(zhigh-tempmin.z)
                             /zrangespan));

  if(islicelow<0)             islicelow=0; // Safety
  if(islicehigh>zslice_count) islicehigh=zslice_count;
  if(islicehigh<islicelow)    islicehigh=islicelow;
}

OC_INDEX
Vf_GeneralMesh3f::GetDisplayList(OC_REAL4m &xstep_request,
                                 OC_REAL4m &ystep_request,
                                 OC_REAL4m & /* zstep_request */,
                                 const Nb_BoundingBox<OC_REAL4> & range_request,
                                 const char *colorquantity,
                                 OC_REAL8m phase,
                                 OC_BOOL invert, OC_BOOL trimtiny,
                                 Nb_List<Vf_DisplayVector> &display_list)
{
#define MEMBERNAME "GetDisplayList"

  display_list.Clear();
  if(GetSize()<1) {
    return 0; // Empty mesh, empty display list.  Incidentally,
    /// this check ensures that we don't call mesh.GetClosest2D
    /// below on an empty mesh (which is something Vf_BoxList's
    /// don't like).
  }

  enum Gm3Color { GM3_NONE, GM3_X, GM3_Y, GM3_Z, GM3_ZSLICE, GM3_MAG,
                  GM3_XYANGLE, GM3_XZANGLE, GM3_YZANGLE };
  Gm3Color cq=GM3_NONE;
  if(colorquantity!=NULL) {
    if(strcmp("x",colorquantity)==0)             cq=GM3_X;
    else if(strcmp("y",colorquantity)==0)        cq=GM3_Y;
    else if(strcmp("z",colorquantity)==0)        cq=GM3_Z;
    else if(strcmp("slice",colorquantity)==0)    cq=GM3_ZSLICE;
    else if(strcmp("mag",colorquantity)==0)      cq=GM3_MAG;
    else if(strcmp("xy-angle",colorquantity)==0) cq=GM3_XYANGLE;
    else if(strcmp("xz-angle",colorquantity)==0) cq=GM3_XZANGLE;
    else if(strcmp("yz-angle",colorquantity)==0) cq=GM3_YZANGLE;
  }
  OC_REAL8m shade;

  Nb_BoundingBox<OC_REAL8> work_range(data_range);


  Nb_Vec3<OC_REAL4> sminpt,smaxpt;
  Nb_Vec3<OC_REAL8> minpt,maxpt;
  range_request.GetExtremes(sminpt,smaxpt);
  Convert(sminpt,minpt);
  Convert(smaxpt,maxpt);
  work_range.ExpandWith(minpt);
  work_range.ExpandWith(maxpt);
  work_range.GetExtremes(minpt,maxpt);

  // Vf_BoxList gets very upset if you call GetClosest2D with a
  // point outside its refinement boundary.
  OC_REAL8 xmin,ymin,xmax,ymax;
  mesh.GetBoxRegion(xmin,ymin,xmax,ymax);
  if(minpt.x<xmin) minpt.x=xmin;
  if(minpt.y<ymin) minpt.y=ymin;
  if(maxpt.x>xmax) maxpt.x=xmax;
  if(maxpt.y>ymax) maxpt.y=ymax;

  OC_REAL8m magsq,minmagsq; // NOTE: Properties are scaled so
  /// that effective "maxmagsq" is 1.0.
  if(MinMagHint>MaxMagHint) {
    NonFatalError(STDDOC,"MinMagHint (%g) > MaxMagHintValue (%g)",
                  static_cast<double>(MinMagHint),
                  static_cast<double>(MaxMagHint));
    return display_list.GetSize(); // Empty grid
  }

  if(!trimtiny) {
    minmagsq = 0.0;
  } else if(MaxMagHint>0.0) {
    // minmagsq is intended for display optimization, not display
    // selection.  Guarantee here some more-or-less arbitrary minimum
    // range between minmag cutoff and MaxMagHint.
    if(MinMagHint<MaxMagHint/1024.) {
      minmagsq=MinMagHint/MaxMagHint;
      minmagsq*=minmagsq;
      minmagsq*=0.999; // Round down 0.1% to protect against round-off
    } else {
      minmagsq = 1.0/(1024.*1024.);
    }
  } else {
    minmagsq=1e-16;  // Pretend MaxMagHint=1.0, and MinMagHint=1e-8
  }

  const OC_REAL8m datascale = GetDisplayValueScale();
  OC_REAL8m wvscale=MaxMagHint;

  if(fabs(ValueMultiplier)<1.0 && wvscale>FLT_MAX*ValueMultiplier) {
    NonFatalError(STDDOC,"Value scale range error (too big): %g",
                  static_cast<double>(datascale));
    return display_list.GetSize(); // Empty grid
  }
  wvscale/=ValueMultiplier;

  // minmagsq is used for excluding small vectors.  Transform
  // this to raw data scale, so we can do this check before
  // scaling the data vectors.  NOTE: minmagsq scaling should
  // be unaffected by datascale.
  minmagsq *= wvscale; // NB: (wvscale*wvscale) might overflow,
  minmagsq *= wvscale; //  but (magsq*wvscale)*wvscale shouldn't.

  if(datascale>1.0 && wvscale>FLT_MAX/datascale) {
    NonFatalError(STDDOC,"Value scale range error (too big): %g",
                  static_cast<double>(datascale));
    return display_list.GetSize(); // Empty grid
  }
  wvscale*=datascale;

  if(wvscale==0.0) wvscale=1.0; // Safety
  OC_REAL8m wvmult=1.0/wvscale;

  // Extract z-range for use by zslice coloring
  OC_INDEX zslice;
  OC_REAL8m zrangemid=0.,zrangemult=1.0,zrangemin=0.,zrangespanrecip=1.0;
  Nb_Vec3<OC_REAL8> tempmin,tempmax;
  data_range.GetExtremes(tempmin,tempmax);
  zrangemin = tempmin.z;
  zrangemid = (tempmin.z + tempmax.z)/2.0;
  OC_REAL8m zrangespan = tempmax.z-tempmin.z;
  if(zrangespan==0.0) zrangespanrecip=1.0;  // Safety
  else                zrangespanrecip=1.0/zrangespan;
  if(cq==GM3_ZSLICE) {
    zrangemult=zrangespanrecip;
    OC_REAL8m tempgrvs=GetDisplayValueScale();
    if(tempgrvs>1.0 && zrangemult > FLT_MAX / tempgrvs) {
      NonFatalError(STDDOC,"Value scale range error (too big): %g",
                    static_cast<double>(tempgrvs));
      return display_list.GetSize(); // Empty grid
    } else {
      zrangemult *= tempgrvs;
    }
    if(zrangemult==0.0) zrangemult=1.0; // Safety
  }

  mesh.ClearSelectCounts();
  if((minpt.x+xstep_request)==minpt.x ||
     (maxpt.x+xstep_request)==maxpt.x ||
     (minpt.y+ystep_request)==minpt.y ||
     (maxpt.y+ystep_request)==maxpt.y   ) {
    // Step request is 0 or too small; return whole list
    Nb_LocatedVector<OC_REAL8> wlv;
    Nb_LocatedVector<OC_REAL4> lv;
    mesh.ResetWholeAccess();
    while(mesh.GetWholeNext(wlv)==0) {

        // NB: There is a parallel branch below corresponding to the
        //     sub-sampled case

        shade=0.5;
        magsq=wlv.value.MagSq();
        if(magsq<=minmagsq) continue; // Skip tiny vectors
        /// Note that minmagsq is at raw data scale, unless trimtiny is
        /// false, in which case minmagsq=0.0. Note: There are cases,
        /// particularly for scalar fields, where one would like
        /// magsq==0 pixels displayed. But it is not obvious how to
        /// discriminate between those situations and the far more
        /// common case where magsq==0 indicates an Ms==0 empty space
        /// that is more usefully left unpainted.

        if(fabs(wvscale)<1.0 && magsq>(FLT_MAX*wvscale)*wvscale) {
          // Vector will overflow floating point range; resize
          wlv.value*=sqrt(FLT_MAX/magsq);
          magsq=FLT_MAX;
        } else {
          magsq*=wvmult; // NB: (wvmult*wvmult) might overflow,
          magsq*=wvmult; //  but (magsq*wvmult)*wvmult shouldn't.
          wlv.value*=wvmult; // User specified data value scaling
        }

        double val;
        switch(cq) {
        case GM3_X:
          val=wlv.value.x;
          shade=(1.+val)/2.;
          break;
        case GM3_Y:
          val=wlv.value.y;
          shade=(1.+val)/2.;
          break;
        case GM3_Z:
          val=wlv.value.z;
          shade=(1.+val)/2.;
          break;
        case GM3_ZSLICE:
          shade = (wlv.location.z-zrangemid)*zrangemult + 0.5;
          break;
        case GM3_MAG:
          shade=sqrt(magsq);
          break;
        case GM3_XYANGLE:
          if(wlv.value.y==0. && wlv.value.x==0.) {
            if(invert) shade = 1. - phase;
            else       shade = -phase;
          } else {
            shade = Oc_Atan2(wlv.value.y,wlv.value.x)*(1./(2*PI));
            shade = shade - floor(shade+phase);  // Force into [0,1)
          }
          break;
        case GM3_XZANGLE:
          if(wlv.value.z==0. && wlv.value.x==0.) {
            if(invert) shade = 1. - phase;
            else       shade = -phase;
          } else {
            shade = Oc_Atan2(wlv.value.z,wlv.value.x)*(1./(2*PI));
            shade = shade - floor(shade+phase);  // Force into [0,1)
          }
          break;
        case GM3_YZANGLE:
          if(wlv.value.z==0. && wlv.value.y==0.) {
            if(invert) shade = 1. - phase;
            else       shade = -phase;
          } else {
            shade = Oc_Atan2(wlv.value.z,wlv.value.y)*(1./(2*PI));
            shade = shade - floor(shade+phase);  // Force into [0,1)
          }
          break;
        default:
          break;
        }

        // Make phase and invert adjustments
        shade+=phase;
        if(shade>1.0)      shade=1.0;
        else if(shade<0.0) shade=0.0;
        if(invert) shade = 1.0 - shade;

        if(magsq>1.0) {
          // Cut back (trim) overly large vectors
          wlv.value*=1.0/sqrt(magsq);
          magsq=1.0;
        }
        Convert(wlv.location,lv.location);
        Convert(wlv.value,lv.value);
        // zslice calculation.  Coordinate this with the method used in
        // GetZslice()
        zslice =
          static_cast<OC_INDEX>(floor(zslice_count*(wlv.location.z-zrangemin)
                                   *zrangespanrecip));
        if(zslice<0)                  zslice=0;
        else if(zslice>=zslice_count) zslice=zslice_count-1;
        display_list.Append(Vf_DisplayVector(lv.location,lv.value,
                                             static_cast<OC_REAL4m>(shade),
                                             zslice));
    }
  }
  else {
    // Otherwise, center a maximal regular grid, with lattice
    // steps xstep_request, ystep_request, inside working mesh box
    // region "work_range", and then use mesh.GetClosest2D to collect
    // samples from the mesh closest to the overlaid regular grid.
    const OC_REAL8m fudge=1.001;  // Fudge factor, used to help
    /// against display "banding" caused by rounding.

    OC_REAL8m xextent=maxpt.x-minpt.x;
    OC_REAL8m xstep=fabs(xstep_request);
    OC_INDEX  ixcount=static_cast<OC_INDEX>(floor((xextent*fudge)/xstep))+1;
    OC_REAL8m xmargin=(xextent-xstep*(ixcount-1))/(2.*fudge);

    OC_REAL8m yextent=maxpt.y-minpt.y;
    OC_REAL8m ystep=fabs(ystep_request);
    OC_INDEX  iycount=static_cast<OC_INDEX>(floor((yextent*fudge)/ystep))+1;
    OC_REAL8m ymargin=(yextent-ystep*(iycount-1))/(2.*fudge);

    // If margin is close to an integral multiple of step_hints,
    // then round it to the multiple.  This should help remove
    // some display "banding" when sampling from grids that are
    // in actual fact regular.
    if(step_hints.x>0) {
      OC_REAL8m frac=fmod(xmargin,static_cast<OC_REAL8m>(step_hints.x));
      if(fabs(frac)<0.1) xmargin-=frac;
    }
    if(step_hints.y>0) {
      OC_REAL8m frac=fmod(ymargin,static_cast<OC_REAL8m>(step_hints.y));
      if(fabs(frac)<0.1) ymargin-=frac;
    }
    OC_INDEX ix,iy; OC_REAL8m x,y;
    OC_INT4m select_count;
    Nb_LocatedVector<OC_REAL4> lv;
    Nb_LocatedVector<OC_REAL8> wlv;
    Nb_LocatedVector<OC_REAL8> *wlvp;
    Nb_Vec3<OC_REAL8> pos(0.,0.,0.);
    for(ix=0, x=minpt.x+xmargin; ix<ixcount; ix++, x+=xstep) {
      pos.x=x;
      for(iy=0, y=minpt.y+ymargin; iy<iycount; iy++, y+=ystep) {
        // NB: There is a parallel branch above corresponding to the
        //     whole list (not sub-sampled) case
        pos.y=y;
        mesh.GetClosest2D(pos,wlvp,select_count);
        if(select_count==1) {
          shade=0.5;
          wlv = (*wlvp);
          magsq=wlv.value.MagSq();

          if(magsq<=minmagsq) continue; // Skip tiny vectors
          /// Note that minmagsq is at raw data scale, unless trimtiny
          /// is false, in which case minmagsq=0.0. Note: There are
          /// cases, particularly for scalar fields, where one would
          /// like magsq==0 pixels displayed. But it is not obvious how
          /// to discriminate between those situations and the far more
          /// common case where magsq==0 indicates an Ms==0 empty space
          /// that is more usefully left unpainted.

          if(fabs(wvscale)<1.0 && magsq>(FLT_MAX*wvscale)*wvscale) {
            // Vector will overflow floating point range; resize
            wlv.value*=sqrt(FLT_MAX/magsq);
            magsq=FLT_MAX;
          } else {
            magsq*=wvmult; // NB: (wvmult*wvmult) might overflow,
            magsq*=wvmult; //  but (magsq*wvmult)*wvmult shouldn't.
            wlv.value*=wvmult; // User specified data value scaling
          }

          double val;
          switch(cq) {
          case GM3_X:
            val=wlv.value.x;
            shade=(1.+val)/2.;
            break;
          case GM3_Y:
            val=wlv.value.y;
            shade=(1.+val)/2.;
            break;
          case GM3_Z:
            val=wlv.value.z;
            shade=(1.+val)/2.;
            break;
          case GM3_ZSLICE:
            shade = (wlv.location.z-zrangemid)*zrangemult + 0.5;
            break;
          case GM3_MAG:
            shade=sqrt(magsq);
            break;
          case GM3_XYANGLE:
            if(wlv.value.y==0. && wlv.value.x==0.) {
              if(invert) shade = 1. - phase;
              else       shade = -phase;
            } else {
              shade = Oc_Atan2(wlv.value.y,wlv.value.x)*(1./(2*PI));
              shade = shade - floor(shade+phase);  // Force into [0,1)
            }
            break;
          case GM3_XZANGLE:
            if(wlv.value.z==0. && wlv.value.x==0.) {
              if(invert) shade = 1. - phase;
              else       shade = -phase;
            } else {
              shade = Oc_Atan2(wlv.value.z,wlv.value.x)*(1./(2*PI));
              shade = shade - floor(shade+phase);  // Force into [0,1)
            }
            break;
          case GM3_YZANGLE:
            if(wlv.value.z==0. && wlv.value.y==0.) {
              if(invert) shade = 1. - phase;
              else       shade = -phase;
            } else {
              shade = Oc_Atan2(wlv.value.z,wlv.value.y)*(1./(2*PI));
              shade = shade - floor(shade+phase);  // Force into [0,1)
            }
          default:
            break;
          }

          // Make phase and invert adjustments
          shade+=phase;
          if(shade>1.0)      shade=1.0;
          else if(shade<0.0) shade=0.0;
          if(invert) shade = 1.0 - shade;

          if(magsq>1.0) { // Cut back (trim) overly large vectors
            wlv.value*=1.0/sqrt(magsq);
            magsq=1.0;
          }
          Convert(wlv.location,lv.location);
          Convert(wlv.value,lv.value);
          zslice =
            static_cast<OC_INDEX>(floor(zslice_count*(wlv.location.z-zrangemin)
                                     *zrangespanrecip));
          if(zslice<0)                  zslice=0;
          else if(zslice>=zslice_count) zslice=zslice_count-1;
          display_list.Append(Vf_DisplayVector(lv.location,lv.value,
                                               static_cast<OC_REAL4m>(shade),
                                               zslice));
        }
      }
    }
  }
  return display_list.GetSize();
#undef MEMBERNAME
}

void Vf_GeneralMesh3f::SetStepHints(const Nb_Vec3<OC_REAL8> &_step)
{
  Nb_Vec3<OC_REAL8> test;
  test=_step;

  // Some parts of the code behave poorly if some component of
  // step_hints is negative.  So protect against that.
  OC_REAL8m a,b,c,t;
  a=test.x; b=test.y; c=test.z;
  if(a<b) { t=a; a=b; b=t; } // Sort, a biggest to c smallest
  if(b<c) { t=b; b=c; c=t; }
  if(a<b) { t=a; a=b; b=t; }
  if(a<=0.) return; // Reject step request
  if(c<=0.) {
    OC_REAL8m minval=a;
    if(b>0.) minval=b;
    if(test.x<=0.) test.x=minval;
    if(test.y<=0.) test.y=minval;
    if(test.z<=0.) test.z=minval;
  }

  step_hints=test;
  UpdateRange();
}

void Vf_GeneralMesh3f::SetApproximateStepHints()
{ // We should have a sophisticated algorithm here that computes
  // and returns the average nearest neighbor distance, but for
  // now we'll just calculate some "average" cell dimensions.
  // NOTE: This functions overwrites any pre-existing value in
  //       member variable step_hints, which may override a file
  //       specified value.  A client may want to check to see
  //       if step_hints.x==0 before calling this routine.
#define MEMBERNAME "SetApproximateStepHints"
  step_hints.Set(1.,1.,1.);
  OC_INDEX point_count=GetSize();
  if(point_count<2) return;  // Meaningless case

  OC_REAL8m dx=data_range.GetWidth();
  OC_REAL8m dy=data_range.GetHeight();
  OC_REAL8m dz=data_range.GetDepth();


  int dim_count=0;
  if(dx>0) dim_count++;
  if(dy>0) dim_count++;
  if(dz>0) dim_count++;
  if(dim_count==0.0) return; // Meaningless case

  // "Cells" here really mean "cells" for display purposes, which are
  // centered about the sample nodes.  If the cells are assumed to
  // be cubes, then the edge length e is a root of
  //     dx.dy.dz + (dx.dy+dx.dz+dy.dz)*e + (dx+dy+dz)*e^2 - (n-1)*e^3
  // where n is the number of cells (i.e., point_count).

  OC_REAL8m e = 1.0;
  if(dim_count==1) {
    // In this case the cubic above reduces to linear
    e = (dx+dy+dz)/(point_count-1);
  }
  else if(dim_count==2) {
    // In this case the cubic reduces to quadratic
    OC_REAL8m a = static_cast<OC_REAL8m>(point_count-1);
    OC_REAL8m b = dx+dy+dz;
    OC_REAL8m c = dx*dy+dx*dz+dy*dz;
    e = (b + sqrt(b*b+4*a*c))/(2*a);
  } else {
    // General cubic case.  Rather than solving exactly what is after
    // all probably only a crude approximation (in general, the cells
    // aren't cubes), just take a decent first guess,
    //            (dx.dy.dz/(n-1))^(1/3)
    // which is an underestimate, and apply a couple Newton steps.
    OC_REAL8m d = dx*dy*dz;
    OC_REAL8m c = dx*dy+dx*dz+dy*dz;
    OC_REAL8m b = dx+dy+dz;
    OC_REAL8m a = static_cast<OC_REAL8m>(1 - point_count);

    OC_REAL8m emin = pow(fabs(d/a),
                      static_cast<OC_REAL8m>(1.)/static_cast<OC_REAL8m>(3.));
    OC_REAL8m emax = 2*emin; // Don't increase estimate by more than 100%
    e=emin;
    OC_REAL8m y = ((a*e+b)*e+c)*e+d;
    OC_REAL8m slope = (3*a*e+2*b)*e+c;  // Slope should e negative
    if(e*slope-y<=emax*slope) e  = emax;
    else                      e += -y/slope;
    y = ((a*e+b)*e+c)*e+d;
    slope = (3*a*e+2*b)*e+c;
    if(y<0 && slope<0) {
      // Overstepped.  This is the expected case given the curvature
      // of the cubic.
      if(e*slope-y>=emin*slope) e = emin;
      else                      e += -y/slope;
    } else if(y>0 && slope<0) {
      // Understepped
      if(e*slope-y<=emax*slope) e = emax;
      else                      e += -y/slope;
    }
    // NOTE: Because of the curvature of the cubic, this last e is
    // expected to still overshoot, i.e., y<0.  I'm not certain if this
    // is a good thing, or a bad thing.  If one prefers to undershoot,
    // one can replace the last Newton step with a secant fit, or
    // perhaps do a quadratic estimate instead of Newton's method.
  }

  step_hints.x=step_hints.y=step_hints.z=e;
  UpdateRange();
  return;
#undef MEMBERNAME
}

void Vf_GeneralMesh3f::SetBoundaryList(const Nb_List< Nb_Vec3<OC_REAL8> >
                                       &boundary_list)
{
#define MEMBERNAME "SetBoundaryList"
  boundary_from_data=0;
  boundary=boundary_list;
  boundary_range.Reset();
  for(Nb_Vec3<OC_REAL8> *vp=boundary.GetFirst();vp!=NULL;
      vp=boundary.GetNext()) {
    boundary_range.ExpandWith(*vp);
  }
  UpdateRange();
#undef MEMBERNAME
}

void Vf_GeneralMesh3f::SetBoundaryList() {
  boundary_from_data=1;   // When boundary is generated from data,
  boundary.Clear();       // both boundary and boundary_range are
  boundary_range.Reset(); // generated as needed so that they will
  /// be up to date with data_range.
  UpdateRange();
}

Nb_Vec3<OC_REAL4> Vf_GeneralMesh3f::GetApproximateCellDimensions() const
{ // NOTE: This function is "conceptually const" as opposed to
  //  "bitwise const"; cf. p76-78 of Meyer's book, "Effective C++."
  //  As a result, we need to do some ugly un-const casting.
  //  This is dangerous, since it looses compiler checks against
  //  changes to ::SetApproximateStepHints that may destroy
  //  conceptual const-ness of this function.
#define MEMBERNAME "GetApproximateCellDimensions"
  Vf_GeneralMesh3f * const localThis = const_cast<Vf_GeneralMesh3f *>(this);
  if(step_hints.x==0) localThis->SetApproximateStepHints();
  if(step_hints.x==0) return Nb_Vec3<OC_REAL4>(1.,1.,1.);  // Nonsense case
  Nb_Vec3<OC_REAL4> temp;
  Convert(step_hints,temp);
  return temp;
#undef MEMBERNAME
}

// Vf_GeneralMesh3f specific functions
void Vf_GeneralMesh3f::ResetMesh()
{
#define MEMBERNAME "ResetMesh"
  mesh.ClearList();
  data_range.Reset();
  boundary.Clear();
  boundary_range.Reset();
  boundary_from_data=1;
  step_hints.Set(0.,0.,0.);
#undef MEMBERNAME
}

void Vf_GeneralMesh3f::ResetMesh(const char* newFilename)
{
#define MEMBERNAME "ResetMesh(const char*)"
  SetFilename(newFilename);
  ResetMesh();
#undef MEMBERNAME
}

void Vf_GeneralMesh3f::ResetMesh(const char* newFilename,
                                 const char* newTitle,
                                 const char* newDescription)
{
#define MEMBERNAME "ResetMesh(const char*,const char*)"
  SetTitle(newTitle);
  SetDescription(newDescription);
  ResetMesh(newFilename);
#undef MEMBERNAME
}

void Vf_GeneralMesh3f::AddPoint(const Nb_LocatedVector<OC_REAL8>& lv)
{
#define MEMBERNAME "AddPoint"
  mesh.AddPoint(lv);
  data_range.ExpandWith(lv.location);
  range.ExpandWith(lv.location);
#undef MEMBERNAME
}

void Vf_GeneralMesh3f::SortPoints()
{
#define MEMBERNAME "SortPoints"
  // Guess at cell dimensions, if necessary
  if(step_hints.x==0) SetApproximateStepHints();

  // Make BoxList mesh bounding box big enough so that
  // refinement boxes will include all sampling points
  // we may ask of it.
  UpdateRange(); // Safety
  Nb_BoundingBox<OC_REAL8> tmp_range;
  GetPreciseRange(tmp_range);
  if(!tmp_range.IsEmpty()) {
    Nb_Vec3<OC_REAL8> min,max;
    tmp_range.GetExtremes(min,max);
    mesh.ExpandBoxRegion(min.x,min.y,max.x,max.y);
  }
  mesh.InflateBoxRegion(1.2,1.2);   // Add a 10% margin on all sides

  // OK, now do refinement
  // mesh.Refine(6);
  mesh.Refine(1024,2.,15.);

#ifndef NDEBUG
  ClassDebugMessage(STDDOC,"Mesh refinement stats:\n"
          "  Number of subboxes: %" OC_INDEX_MOD "d\n"
          "  Average number of points per box: %.2f\n"
          "  Average box list length:  %.2f\n"
          "  Memory waste: %" OC_INDEX_MOD "d\n",
          mesh.GetBoxCount(),mesh.GetAveInCount(),mesh.GetAveListCount(),
          mesh.GetSpaceWaste());
#endif

#undef MEMBERNAME
}


OC_BOOL Vf_GeneralMesh3f::GetFirstPt(Vf_Mesh_Index* &key_export,
                                     Nb_LocatedVector<OC_REAL8> &lv) const
{
  Vf_GeneralMesh3f_Index *index=new Vf_GeneralMesh3f_Index;
  key_export=(Vf_Mesh_Index*)index;
  const Nb_LocatedVector<OC_REAL8> *lvp=mesh.GetFirst(index->key);
  if(lvp==NULL) return 0;
  lv= *lvp;
  return 1;
}

OC_BOOL Vf_GeneralMesh3f::GetNextPt(Vf_Mesh_Index* &key_import,
                                    Nb_LocatedVector<OC_REAL8> &lv) const
{
  Vf_GeneralMesh3f_Index *index=(Vf_GeneralMesh3f_Index *)key_import;
  const Nb_LocatedVector<OC_REAL8> *lvp=mesh.GetNext(index->key);
  if(lvp==NULL) return 0;
  lv= *lvp;
  return 1;
}


OC_BOOL Vf_GeneralMesh3f::SetNodeValue(const Vf_Mesh_Index* key_import,
                                       const Nb_Vec3<OC_REAL8>& value)
{ // For use by generic Vf_Mesh code for manipulation of
  // mesh node values (as opposed to node locations).
  // Returns 0 on success, 1 if the index is invalid
  // or out-of-bounds.

  Vf_GeneralMesh3f_Index *index = (Vf_GeneralMesh3f_Index *)key_import;
  /// This should be a dynamic cast, but we don't want to
  /// require RTTI in this code.  So we leave it up to the
  /// programmer to insure that keys imported here were created
  /// by the corresponding GetFirstPt or GetNextPt functions.

  return mesh.SetValue(index->key,value);
}

// Copy constructor with transform
Vf_GeneralMesh3f::Vf_GeneralMesh3f
(const Vf_Mesh& import_mesh,
 OC_REAL8m subsample,
 const char* flipstr,
 Nb_BoundingBox<OC_REAL8>& clipbox,
 OC_BOOL cliprange)
  : Vf_Mesh(import_mesh)
{
#define MEMBERNAME "Constructor_copy_with_transform"

  // Parameter value check
  if(subsample!=0.0 && subsample != 1.0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Vf_GeneralMesh3f","Vf_GeneralMesh3f",
                          VF_MESH_ERRBUFSIZE+200,
                          "Requested subsample value (%g) not supported; "
                          "subsampling not supported for this mesh type. "
                          "subsample must be 0 or 1",subsample));
  }

  // Interpret flipstr.  The transform procedure is to read data one
  // point at a time from import_mesh into source_pt, copy from
  // source_pt to target_pt through pointers lx, ly, lz, vx, vy, vz with
  // sign multipliers xsign, ysign, zsign on the first, second and third
  // components of source_pt, respectively.  For example, if the flipstr
  // is -z:y:x, then lx will point to target_pt.location.z and xsign
  // will be -1, so *lx = xsign*source_pt.location.x is actually
  //        target_pt.location.z = -1 * source_pt.location.x
  // as desired.
  const char* parsestr=" \t\n:";
  Nb_LocatedVector<OC_REAL8> source_pt, target_pt;
  int xsign=0,ysign=0,zsign=0;
  OC_REAL8 *lx=NULL,*ly=NULL,*lz=NULL,*vx=NULL,*vy=NULL,*vz=NULL;
  char* flipbuf = new char[strlen(flipstr)+1];
  strcpy(flipbuf,flipstr);
  char* nexttoken=flipbuf;
  // Component 1
  char *token = Nb_StrSep(&nexttoken,parsestr);
  OC_INDEX len=0;
  if(token==NULL || (len=static_cast<OC_INDEX>(strlen(token)))<1 || len>2
     || (token[len-1]!='x' && token[len-1]!='y' && token[len-1]!='z')
     || (len==2 && token[0]!='-' && token[0]!='+')) {
    NonFatalError(STDDOC,"Invalid transform flip string: %.200s",flipstr);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_Vf_GeneralMesh3f",
                          "Vf_GeneralMesh3f",VF_MESH_ERRBUFSIZE+200,
                          "Invalid transform flip string: %.200s",flipstr));
  }
  xsign=1; if(len==2 && token[0]=='-') xsign=-1;
  if(token[len-1]=='x') {
    lx = &target_pt.location.x;
    vx = &target_pt.value.x;
  } else if(token[len-1]=='y') {
    lx = &target_pt.location.y;
    vx = &target_pt.value.y;
  } else if(token[len-1]=='z') {
    lx = &target_pt.location.z;
    vx = &target_pt.value.z;
  }
  // Component 2
  token = Nb_StrSep(&nexttoken,parsestr);
  if(token==NULL || (len=static_cast<OC_INDEX>(strlen(token)))<1 || len>2
     || (token[len-1]!='x' && token[len-1]!='y' && token[len-1]!='z')
     || (len==2 && token[0]!='-' && token[0]!='+')) {
    NonFatalError(STDDOC,"Invalid transform flip string: %.200s",flipstr);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_Vf_GeneralMesh3f",
                          "Vf_GeneralMesh3f",VF_MESH_ERRBUFSIZE+200,
                          "Invalid transform flip string: %.200s",flipstr));
  }
  ysign=1; if(len==2 && token[0]=='-') ysign=-1;
  if(token[len-1]=='x') {
    ly = &target_pt.location.x;
    vy = &target_pt.value.x;
  } else if(token[len-1]=='y') {
    ly = &target_pt.location.y;
    vy = &target_pt.value.y;
  } else if(token[len-1]=='z') {
    ly = &target_pt.location.z;
    vy = &target_pt.value.z;
  }
  // Component 3
  token = Nb_StrSep(&nexttoken,parsestr);
  if(token==NULL || (len=static_cast<OC_INDEX>(strlen(token)))<1 || len>2
     || (token[len-1]!='x' && token[len-1]!='y' && token[len-1]!='z')
     || (len==2 && token[0]!='-' && token[0]!='+')) {
    NonFatalError(STDDOC,"Invalid transform flip string: %.200s",flipstr);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_Vf_GeneralMesh3f",
                          "Vf_GeneralMesh3f",VF_MESH_ERRBUFSIZE+200,
                          "Invalid transform flip string: %.200s",flipstr));
  }
  zsign=1; if(len==2 && token[0]=='-') zsign=-1;
  if(token[len-1]=='x') {
    lz = &target_pt.location.x;
    vz = &target_pt.value.x;
  } else if(token[len-1]=='y') {
    lz = &target_pt.location.y;
    vz = &target_pt.value.y;
  } else if(token[len-1]=='z') {
    lz = &target_pt.location.z;
    vz = &target_pt.value.z;
  }
  if(Nb_StrSep(&nexttoken,parsestr)!=NULL ||
     lx==ly || lx==lz || ly==lz) {
    NonFatalError(STDDOC,"Invalid transform flip string: %.200s",flipstr);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Vf_Vf_GeneralMesh3f",
                          "Vf_GeneralMesh3f",VF_MESH_ERRBUFSIZE+200,
                          "Invalid transform flip string: %.200s",flipstr));
  }
  delete[] flipbuf;

  // Copy/setup supplementary data members
  import_mesh.GetPreciseDataRange(data_range);
  data_range.IntersectWith(clipbox);
  data_range.GetExtremes(source_pt.location,source_pt.value);
  *lx = xsign*source_pt.location.x;
  *ly = ysign*source_pt.location.y;
  *lz = zsign*source_pt.location.z;
  *vx = xsign*source_pt.value.x;
  *vy = ysign*source_pt.value.y;
  *vz = zsign*source_pt.value.z;
  data_range.SortAndSet(target_pt.location,target_pt.value);

  if(import_mesh.IsBoundaryFromData()) {
    // Boundary is generated as needed from data_range
    boundary_from_data=1;
    boundary.Clear();         // Safety
    boundary_range.Reset();   // Safety
    import_mesh.GetPreciseRange(boundary_range);
    if(cliprange) boundary_range.IntersectWith(clipbox);
    if(!boundary_range.IsEmpty()) {
      Nb_Vec3<OC_REAL8> minpt,maxpt;
      boundary_range.GetExtremes(minpt,maxpt);
      *vx = xsign * minpt.x;
      *vy = ysign * minpt.y;
      *vz = zsign * minpt.z;
      minpt = target_pt.value;
      *vx = xsign * maxpt.x;
      *vy = ysign * maxpt.y;
      *vz = zsign * maxpt.z;
      maxpt = target_pt.value;
      boundary_range.SortAndSet(minpt,maxpt);
    }
  } else {
    // Copy and transform boundary from import_mesh.
    boundary_from_data=0;
    Nb_List< Nb_Vec3<OC_REAL8> > tmpbdry;
    import_mesh.GetPreciseBoundaryList(tmpbdry);
    const Nb_Vec3<OC_REAL8> *vptr;
    Nb_List_Index< Nb_Vec3<OC_REAL8> > key;
    boundary.Clear();         // Safety
    boundary_range.Reset();   // Safety
    for(vptr=tmpbdry.GetFirst(key);vptr!=NULL;vptr=tmpbdry.GetNext(key)) {
      Nb_Vec3<OC_REAL8> tmpvec = *vptr;
      if(cliprange) clipbox.MoveInto(tmpvec);
      *vx = xsign * tmpvec.x;
      *vy = ysign * tmpvec.y;
      *vz = zsign * tmpvec.z;
      boundary.Append(target_pt.value);
      boundary_range.ExpandWith(target_pt.value);
    }
  }

  // Iterate through entire import mesh, transforming points
  // as we go.
  Vf_Mesh_Index* index=NULL;
  if(import_mesh.GetFirstPt(index,source_pt)) {
    do {
      if(clipbox.IsIn(source_pt.location)) {
        *lx = xsign*source_pt.location.x;
        *ly = ysign*source_pt.location.y;
        *lz = zsign*source_pt.location.z;
        *vx = xsign*source_pt.value.x;
        *vy = ysign*source_pt.value.y;
        *vz = zsign*source_pt.value.z;
        AddPoint(target_pt);
      }
    } while(import_mesh.GetNextPt(index,source_pt));
    SortPoints(); // Is this necessary?
  }
  if(index!=NULL)  delete index;

  // Setup step hints
  if(strcmp(import_mesh.GetMeshType(),"Vf_GridVec3f")==0) {
    // Regular rectangular mesh
    Vf_GridVec3f* mesh_rect=(Vf_GridVec3f*)(&import_mesh);
    step_hints = mesh_rect->GetGridStep();
    *vx = step_hints.x; *vy = step_hints.y; *vz = step_hints.z;
    step_hints = target_pt.value;
  } else if(strcmp(import_mesh.GetMeshType(),"Vf_GeneralMesh3f")==0) {
    // Irregular mesh
    Vf_GeneralMesh3f* mesh_irreg=(Vf_GeneralMesh3f*)(&import_mesh);
    step_hints = mesh_irreg->GetStepHints();
    *vx = step_hints.x; *vy = step_hints.y; *vz = step_hints.z;
    step_hints = target_pt.value;
  } else {
    // Punt
    SetApproximateStepHints();
  }

  UpdateRange();

#undef MEMBERNAME
}

