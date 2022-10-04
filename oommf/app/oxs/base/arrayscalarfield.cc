/* FILE: arrayscalarfield.h      -*-Mode: c++-*-
 *
 * Scalar field wrapper for Nb_Array3D<OC_REAL8m>.  This is not intended to
 * be used as a standalone field, but rather to be embedded inside
 * another field object, for implementation purposes.
 *    After construction, owner clients should call the InitArray method
 * to set up the array size and user-space to array mapping.  This
 * mapping positions each array element in the center of the
 * corresponding rectangular cell. Individual array values are then set
 * or read via the SetArrayElt and GetArrayElt members.  The *Value
 * members are the generic Oxs_ScalarField interface; the array elements
 * should be initialized before any use of the *Value members.
 *    NOTE: The Nb_Array3D class accesses arr(i,j,k) as arr[i][j][k].
 * Since Oxs assumes x is the long dimension, the Oxs_ArrayScalarField
 * interface reorders these so that, e.g., SetArrayElt(ix,iy,iz) maps
 * to arr(iz,iy,iz).
 */

#include "arrayscalarfield.h"

#include <cassert>
#include <string>

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ArrayScalarField);

/* End includes */


// Constructor
Oxs_ArrayScalarField::Oxs_ArrayScalarField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ScalarField(name,newdtr,argstr),
    scale(1,1,1),offset(0,0,0),
    default_value(0),exterior(EXTERIOR_ERROR)
{
  // Process arguments.

  vector<String> params;
  if(FindInitValue("exterior",params)) {
    OC_BOOL badparams = 0;
    if(params.size()!=1) {
      badparams = 1;
    } else if(params[0].compare("error")==0) {
      exterior = EXTERIOR_ERROR;
    } else if(params[0].compare("boundary")==0) {
      exterior = EXTERIOR_BOUNDARY;
    } else {
      exterior = EXTERIOR_DEFAULT;
      default_value = Nb_Atof(params[0].c_str(),badparams);
    }

    if(badparams) {
      String initstr;
      FindInitValue("exterior",initstr);
      String msg = String("Invalid exterior init string: \"")
        + initstr
        + String("\"; should be error, boundary, or a single numeric value.");
      throw Oxs_ExtError(this,msg.c_str());
    }
  }

  VerifyAllInitArgsUsed();
}

void
Oxs_ArrayScalarField::InitArray
(OC_INDEX xdim,OC_INDEX ydim,OC_INDEX zdim,
 const Oxs_Box& range)
{
  if(xdim<1 || ydim<1 || zdim<1) {
    throw Oxs_ExtError(this,"Invalid x/y/z dimensions");
  }
  array.Allocate(zdim,ydim,xdim);

  offset.x = range.GetMinX(); // Conversion from user to array coords is
  offset.y = range.GetMinY(); // handled using "floor()", so we don't
  offset.z = range.GetMinZ(); // need +cellsize/2 here.

  OC_REAL8m xrange = range.GetMaxX() - offset.x;
  OC_REAL8m yrange = range.GetMaxY() - offset.y;
  OC_REAL8m zrange = range.GetMaxZ() - offset.z;
  if(xrange<=0.0 || yrange<=0.0 || zrange<=0.0) {
    throw Oxs_ExtError(this,"Invalid range");
  }

  scale.x = static_cast<OC_REAL8m>(xdim);
  if(xrange<1.0 && scale.x >= DBL_MAX * xrange) {
    throw Oxs_ExtError(this,"x range-scale overflow");
  }
  scale.x /= xrange;

  scale.y = static_cast<OC_REAL8m>(ydim);
  if(yrange<1.0 && scale.y >= DBL_MAX * yrange) {
    throw Oxs_ExtError(this,"y range-scale overflow");
  }
  scale.y /= yrange;

  scale.z = static_cast<OC_REAL8m>(zdim);
  if(zrange<1.0 && scale.z >= DBL_MAX * zrange) {
    throw Oxs_ExtError(this,"z range-scale overflow");
  }
  scale.z /= zrange;

  // Conversion from user to array coords is, for example,
  //   x -> int(floor((x-offset.x)*scale.x))
}

OC_REAL8m
Oxs_ArrayScalarField::Value
(const ThreeVector& pt) const
{
  OC_INDEX ix = static_cast<OC_INDEX>(floor((pt.x-offset.x)*scale.x));
  OC_INDEX iy = static_cast<OC_INDEX>(floor((pt.y-offset.y)*scale.y));
  OC_INDEX iz = static_cast<OC_INDEX>(floor((pt.z-offset.z)*scale.z));

  if(0<=ix && ix<array.GetDim3() &&
     0<=iy && iy<array.GetDim2() &&
     0<=iz && iz<array.GetDim1()) {
    // In bounds
    return array(iz,iy,ix);
  }

  // Otherwise, pt is out-of-bounds
  if(exterior==EXTERIOR_DEFAULT) {
    return default_value;
  }
  if(exterior==EXTERIOR_ERROR) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"Requested point (%g,%g,%g)-->(%d,%d,%d)"
		" is outside mesh range: (%g,%g,%g) x (%g,%g,%g)"
		" (Error in member function Oxs_ArrayScalarField::Value)",
		static_cast<double>(pt.x),
                static_cast<double>(pt.y),
                static_cast<double>(pt.z),ix,iy,iz,
                static_cast<double>(offset.x),
                static_cast<double>(offset.y),
                static_cast<double>(offset.z),
                static_cast<double>(array.GetDim3()*scale.x+offset.x),
                static_cast<double>(array.GetDim2()*scale.y+offset.y),
                static_cast<double>(array.GetDim1()*scale.z+offset.z));
    throw Oxs_ExtError(this,buf);
  }

  assert(exterior==EXTERIOR_BOUNDARY);

  if(ix<0)                 ix = 0;
  if(ix>array.GetDim3()-1) ix = array.GetDim3()-1;

  if(iy<0)                 iy = 0;
  if(iy>array.GetDim2()-1) iy = array.GetDim2()-1;

  if(iz<0)                 iz = 0;
  if(iz>array.GetDim1()-1) iz = array.GetDim1()-1;

  return array(iz,iy,ix);
}

void
Oxs_ArrayScalarField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& fillarray) const
{
  DefaultFillMeshValue(mesh,fillarray);
}

void
Oxs_ArrayScalarField::IncrMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& fillarray) const
{
  DefaultIncrMeshValue(mesh,fillarray);
}

void
Oxs_ArrayScalarField::MultMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& fillarray) const
{
  DefaultMultMeshValue(mesh,fillarray);
}
